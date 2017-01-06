import os
import multiprocessing
import numpy             as np
import matplotlib.pyplot as plt
import matplotlib.cm     as cm
import pandas            as pd
from   sklearn  import cluster
from   sklearn  import preprocessing
from   optparse import OptionParser
from   optparse import make_option

###########################################################################################################
#                                                Parse options                                            #
###########################################################################################################    

inOpt = make_option("-i", "--input", type = "string", default = None,
                    help = "Data set input file")

outOpt = make_option("-o", "--output", type = "string", default = None,
                     help = "Output to file (static). Output format is set according to file extension")

titleOpt = make_option("-t", "--title", type = "string", default = None,
                       help = "Plot title")

yOpt = make_option("-y", "--yaxis", type = "string", default = None,
                   help = "Use column named 'yaxis' instead of column 2 as Y values to plot")

logOpt = make_option("-l", "--log", action = "store_true",
                     help = "Plot yaxis in logscale")

xOpt = make_option("-x", "--xaxis", type = "string", default = None,
                   help = "Use column named 'xaxis' instead of column 1 as x values to plot")

splitOpt = make_option("-s", "--split", action = "store_true",
                       help = "Split each monitor into several plots, one per hwloc obj")

filterOpt = make_option("-f", "--filter", type = "string", default = "nan,inf,outliers",
                        help = """Filter monitors trace to remove value that could create errors or unexpected plots.
                        Format: value,value,...
                        Values: inf, nan, neg, outliers.""")

logOpt = make_option("-l", "--log", action = "store_true",
                     help = "Plot yaxis in logscale")

clusterOpt = make_option("-c", "--cluster", default = None,
                         help = """Split each monitor into several clusters based on monitor events.
                         Created clusters are appended at the end of the dataframe.
                         -c takes one argument as option which the method to apply for clustering and one argument.
                         possible methods are:
                         * kmeans:arg, with arg (integer) beeing the number of clusters to compute.
                         * dbscan:arg, with arg (float) beeing the eps parameter of dbscan methods from sklearn (should be in ]0,1])""")

pipOpt = make_option("-p", "--pipeline", type = "int", default = 0,
                     help = """Concatenate previous <arg> rows of previous samples to current row of the dataframe. 
                     This is usefull for clustering or modeling to take into account the past in analysis such as clustering or
                     model fitting.

                     example:
                     Nanoseconds   X0  Nanoseconds_0 X0_0 
                              10  7.3            NaN  NaN
                              20  4.2             10  7.3
                              30  1.6             20  4.2
                             NaN  NaN             30  1.6

                     If pipeline options is greater than 0, then NaN values are created at the tip of the dataframe and data frame 
                     is filtered consequently to remove NaN. Thus the frame tips of pipeline rows are also removed""")

# fitOpt = make_option("-m", "--model", type = "string", default = None,
#                      help = """fit y data according to model argument.

#   If (--model = \"linear\"), then use a linear model of events (except y) to fit y.
#   Try to load model from file named after monitor id. If file doesn't exists, compute linear model and save to file.
#   If (--model = \"linear:2\"), then use a linear model with 2 timesteps for each sample of the training set.
#   If (--model = \"nnet\"), same as linear but use a neural network instead of linear regression model. 
#   If this monitor neural network already exists then load the one existing and do not train the network.
#   If (--model = \"nnet:train\"), then fitting a monitor will update its existing neural network or create a new one to train.
#   If (--model = \"nnet:2\"), train a neural network using 2 timesteps for each sample of the training set.
#   If (--model = \"rsnns\"), fit with a recurrent neural network.
#   If (--model = \"rsnns:train\"), train (not update) a new network with new input.
#   If (--model = \"periodic\"), then fit y column with a fourier serie of time column.
#   If (--model = \"gaussian\"), then fit y column as a normal distribution, and output y*P(y) on cross validation set.""")

# dynOpt = make_option("-d", "--dynamic", action = "store_true",    
#                      help = "Plot monitor by part dynamically. --window points are shown each step")

# winOpt = make_option("-w", "--window", type = "int", default = 10000,
#                      help = "number of points to plot. Used with -d option and -v option")

# freqOpt = make_option("-u", "--update", type = "float", default = 0.5,
#                       help = """frequency of read from trace file and plot in seconds. 
#                       Plot should be displayed as parts of a large or updated trace.
#                       In this case -w points will be plot and window will move by -w/4 steps ahead every -f seconds""")

parser = OptionParser(option_list = [inOpt, outOpt, titleOpt, filterOpt, xOpt, yOpt, logOpt, splitOpt, clusterOpt, pipOpt])

###########################################################################################################
#                                      Read Trace and Extract Monitors                                    #
###########################################################################################################    
        
class Monitors:
    name = "Monitors"
    data = None       #Trace data columns excluding hwloc obj column
    objs = None       #Array of objs in trace
    xcol = None       #Index of x column
    ycol = None       #Index of y column
    xmax = 0          #maximum value of x inside trace
    xmin = 0          #minimum value of x inside trace
    ymax = 0          #maximum value of y inside trace
    ymin = 0          #minimum value of x inside trace
    colors = None     #monitors colors
    nan = False       #Allow nan values in dataframe
    pipeline = 0      #Do we pipeline monitors ?
    
    def __init__(self, dataframe, name=None, xcol = None, ycol=None, pipeline=0):
        self.data = dataframe

        #set objs list
        self.objs = np.unique(self.data['Obj'])
        
        #set name
        if(name != None): self.name = name
        
        #select xcol and ycol
        if(xcol == None): self.xcol = self.data.columns[1]
        else: self.xcol = xcol        
        if(ycol == None): self.ycol = self.data.columns[2]
        else: self.ycol = ycol
        
        #prepare color rainbow        
        self.colors = cm.rainbow(np.linspace(0, 1, self.objs.size))

        self.pipeline = pipeline            
        
    @classmethod
    def fromfilename(cls, fname, name=None, xcol = None, ycol=None, pipeline=0):
        #Store id for title
        if(name == None): name = os.path.basename(fname)
        df = pd.read_table(fname, delim_whitespace=True)
        return cls(df, name, xcol, ycol, pipeline=pipeline)

    def append(self, monitors):
        self.objs = np.append(self.objs, monitors.objs)
        self.objs = np.unique(self.objs)
        self.xmax = np.max([monitors.xmax, self.xmax])
        self.xmin = np.min([monitors.xmin, self.xmin])
        self.ymax = np.max([monitors.ymax, self.ymax])
        self.ymin = np.min([monitors.ymin, self.ymin])
        self.data = self.data.append(monitors.data, ignore_index=True)        
        self.colors = cm.rainbow(np.linspace(0, 1, self.objs.size))
        
    def filter(self, nan = True, inf = True, neg = False, outliers=False):
        if(inf):
            self.data.replace([np.inf, -np.inf], np.nan)
        if(neg):
            self.data[self.data<0] = np.nan
        if(outliers):
            xp = np.nanpercentile(self.data[self.xcol], q = [1, 99], interpolation = "nearest")
            self.xmin , self.xmax = xp[0], xp[1]
            yp = np.nanpercentile(self.data[self.ycol], q = [1, 99], interpolation = "nearest")            
            self.ymin, self.ymax = yp[0], yp[1]
        if(nan):
            self.data.dropna(how='any', inplace=True)
            self.nan = True
        
    def count(self):
        return self.objs.size

    def cluster(self, method="dbscan", arg="0.5"):
        monitors = []
        #create clustered monitors
        for i in range(self.count()):
            monitor = Monitor(self, i, pipeline=self.pipeline)
            if(method == "dbscan"): monitor.cluster_dbscan(float(arg))
            elif(method == "kmeans"): monitor.cluster_kmeans(int(arg))
            else: print("wrong method provided to cluster function"); return()
            monitors.append(monitor)
            
        #append clustered monitors
        while monitors: self.append(monitors.pop())

            
    def plot(self,subplot=False, ylog = False, output=None):
        #Set x and y bounds if necessary
        if(self.xmin == self.xmax):
            self.xmax = np.max(self.data[self.xcol])
            self.xmin = np.min(self.data[self.xcol])
        if(self.ymin == self.ymax):            
            self.ymax = np.max(self.data[self.ycol])
            self.ymin = np.min(self.data[self.ycol])

        #ticks , title and labels
        plt.xticks(np.linspace(self.xmin,self.xmax, 9, endpoint=True))
        plt.yticks(np.linspace(self.ymin,self.ymax, 9, endpoint=True))
        plt.gcf().text(0.5, 0.04, self.xcol, ha='center', va='center')
        plt.gcf().text(0.04, 0.5, self.ycol, ha='center', va='center', rotation='vertical')
        plt.suptitle(self.name)

        #plot each monitor
        sax = None
        for i in range(self.count()):
            monitor = Monitor(self, i, pipeline=0)
            if(subplot):
                ax = plt.subplot(1, self.count(), i+1, sharex=sax, sharey=sax)
                if(i == 0): sax = ax
                else:
                    for tic in ax.yaxis.get_major_ticks():
                        tic.tick1On = tic.tick2On = False
                        tic.label1On = tic.label2On = False
                        ticks = ax.xaxis.get_major_ticks()
                    ticks[0].tick1On = ticks[0].tick2On = False
                    ticks[0].label1On = ticks[0].label2On = False
                ax.grid(True)                
                monitor.plot(ax, color=self.colors[i], ylog=ylog)
            else: monitor.plot(plt.gca(), color=self.colors[i], ylog=ylog)

        #adjust plot in case
        if(subplot):
            plt.subplots_adjust(wspace=0, hspace=0)            
        else:
            plt.grid()
            plt.legend(loc='best', frameon=True, fancybox=True, shadow=True)
        
        if(output==None): plt.show()
        else:
            extension = output.split(".")
            extension = extension[extension.__len__()-1]
            plt.savefig(output, papertype="a4", orientation="landscape", format=extension)

###########################################################################################################
#                                          Monitor Analysis and Plot                                      #
###########################################################################################################

class Monitor(Monitors):
    n_data = None #Normalized data_set
    
    def __init__(self, monitors, i=0, pipeline=0):
        self.xmax = monitors.xmax
        self.xmin = monitors.xmin
        self.ymax = monitors.ymax
        self.ymin = monitors.ymin
        self.objs = np.array([monitors.objs[i]])
        self.xcol = monitors.xcol
        self.ycol = monitors.ycol
        self.nan = monitors.nan
        subset = monitors.data['Obj'] == self.objs[0]
        self.data = monitors.data[subset]
        self.n_data = None

        #append previous row if pipeline is set
        if(pipeline>0):
            df_cpy = self.data.copy()
            nrow = self.data.shape[0]        
            for i in range(pipeline):
                cols = df_cpy.columns.tolist()
                cols.pop(0)
                sub = df_cpy[cols]
                for j in range(len(cols)): cols[j] = cols[j] + "_" + str(i)
                sub.columns = cols
                for j in range(i):
                    sub.index.pop(0)
                    sub.index.append(i+1+sub.index[len(sub.index)-1])
                self.data = pd.merge(self.data, sub, how="left", left_index=True, right_index=True, sort=False)
            if(self.nan): self.filter(nan=True, inf=False)
        
    def normalize(self):
        columns = self.data.columns[range(1,self.data.columns.size)]
        self.n_data = pd.DataFrame(preprocessing.robust_scale(self.data[columns].values), columns=columns)

    def __assign_clusters__(self, labels):
        obj = self.objs[0]
        
        #make hard copy of the dataframe to allow modifications
        self.data = self.data.copy()

        #count clusters
        n_clusters = np.sum(np.unique(labels)>=0)
        
        #Create new objects list
        self.objs = np.empty(n_clusters, dtype = 'S32')
        for i in range(n_clusters): self.objs[i] = obj + "_(" + str(i) + ")"

        #Assign new objects in 'Obj' Column.
        objs = np.empty(labels.size, dtype = 'S32')
        for i in range(labels.size):
            if(labels[i]<0): objs[i] = "Noise";
            else: objs[i] = self.objs[labels[i]]
        self.data['Obj'] = objs
        
        #Cast self to parent Monitors class to prevent usage of Monitor class methods
        self.__class__ = Monitors
        
    def cluster_kmeans(self, n_clusters=8):
        if(self.objs.size>1): print("A monitor cannot be clustered twice."); return
        if(self.n_data == None): self.normalize()
        n_proc = multiprocessing.cpu_count()
        model = cluster.KMeans(n_clusters=n_clusters, max_iter=300, tol=1e-4, n_init=n_proc, verbose=0, n_jobs=n_proc)
        kmeans = model.fit(self.n_data)
        self.__assign_clusters__(kmeans.labels_)

    def cluster_dbscan(self, eps=5e-1):
        if(self.objs.size>1): print("A monitor cannot be clustered twice."); return
        if(self.n_data == None): self.normalize()
        n_proc = multiprocessing.cpu_count()
        model = cluster.DBSCAN(eps)
        dbscan = model.fit(self.n_data)
        self.__assign_clusters__(dbscan.labels_)
        
    def plot(self, axes, ylog=False, color=[0.0,0.0,0.0,0.0]):
        if(ylog): axes.set_yscale('log')
        axes.scatter(self.data[self.xcol], self.data[self.ycol], 1, label=self.objs[0], color=color)
        axes.set_xlim([self.xmin, self.xmax])
        axes.set_ylim([self.ymin, self.ymax])
        axes.legend(loc='best', frameon=True, fancybox=True, shadow=True)
        
###########################################################################################################
#                                                   Program                                               #
###########################################################################################################

args = ["-i", "/home/ndenoyel/Documents/hmon/tests/hpccg/multi_hpccg.out", #"/home/ndenoyel/Documents/specCPU2006/filtered.out"
        "-f", "neg,inf,nan,outliers",
        "-t", "test",
        "-c", "dbscan:0.4",
        "-p", "1",
        "-s"]

options, args = parser.parse_args()

filters = options.filter.split(",")

monitors = Monitors.fromfilename(fname=options.input,
                                 xcol=options.xaxis,
                                 ycol=options.yaxis,
                                 name=options.title,
                                 pipeline=options.pipeline)

monitors.filter(nan=filters.__contains__("nan"),
                inf=filters.__contains__("inf"),
                neg=filters.__contains__("neg"),
                outliers=filters.__contains__("outliers"))

if(options.cluster != None):
    cluster_args = options.cluster.split(":")
    monitors.cluster(cluster_args[0], cluster_args[1])

monitors.plot(subplot=options.split, ylog=options.log, output=options.output)

