import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
from optparse import OptionParser
from optparse import make_option

###########################################################################################################
#                                                Parse options                                            #
###########################################################################################################    

inOpt = make_option("-i", "--input", type = "string", default = None,
                    help = "Data set input file")

outOpt = make_option("-o", "--output", type = "string", default = None,
                     help = "Output pdf file (static)")

titleOpt = make_option("-t", "--title", type = "string", default = None,
                       help = "Plot title")

yOpt = make_option("-y", "--yaxis", type = "int", default = 2,
                   help = "Use column yaxis instead of column 3 as Y values to plot")

logOpt = make_option("-l", "--log", action = "store_true",
                     help = "Plot yaxis in logscale")

xOpt = make_option("-x", "--xaxis", type = "int", default = 1,
                   help = "Use column xaxis instead of column 3 as x values to plot")

splitOpt = make_option("-s", "--split", action = "store_true",
                       help = "Split each monitor into several plots, one per hwloc obj")

filterOpt = make_option("-f", "--filter", type = "string", default = "nan,inf,outliers",
                      help = """Filter monitors trace to remove value that could create errors or unexpected plots.
                      Format: value,value,...
                      Values: inf, nan, neg, outliers.""")

logOpt = make_option("-l", "--log", action = "store_true",
                     help = "Plot yaxis in logscale")

# clusterOpt = make_option("-c", "--cluster", action = "store_true",
#                          help = "Split each monitor into several clusters using dbscan on monitor events")

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

parser = OptionParser(option_list = [inOpt, outOpt, titleOpt, filterOpt, xOpt, yOpt, logOpt, splitOpt])

###########################################################################################################
#                                      Read Trace and Extract Monitors                                    #
###########################################################################################################    
        
class Monitors:
    name = "Monitors"
    objs = []         #List of unique hwloc obj in the trace
    objs_indices = [] #Corresponding hwloc_obj indices in data and objs
    data = [[]]       #Trace data columns excluding hwloc obj column
    colnames = []     #Columns name excluding hwloc obj column
    xcol = 0          #Index of x column
    ycol = 1          #Index of y column
    xmax = 0          #maximum value of x inside trace
    xmin = 0          #minimum value of x inside trace
    ymax = 0          #maximum value of y inside trace
    ymin = 0          #minimum value of x inside trace
    colors = []       #monitors colors
    
    def __init__(self, fname, name=None, xcol = 0, ycol=1):
        #Store id for title
        if(name == None): self.name = os.path.basename(fname)
        else: self.name = name
        
        #Load column names
        self.colnames = np.genfromtxt(fname=fname, dtype='S32', max_rows = 1)
        #Load data skipping first column containing topology objects
        self.data = np.genfromtxt(fname=fname, skip_header=1, usecols=range(1,self.colnames.size), dtype=float)

        #Shrink column names to exclude topology objects
        self.colnames = self.colnames[range(1,self.colnames.size)]

        #Load objects column
        objs = np.genfromtxt(fname=fname, usecols=(0), skip_header=1, dtype='S16')
        self.objs, self.objs_indices = np.unique(objs, return_inverse=True)
        self.colors = cm.rainbow(np.linspace(0, 1, self.objs.size))
        
        #Set x and y stats
        self.xcol = xcol
        self.ycol = ycol
        self.xmax = np.max(self.data[:,xcol])
        self.xmin = np.min(self.data[:,xcol])
        self.ymax = np.max(self.data[:,ycol])
        self.ymin = np.min(self.data[:,ycol])

    def filter(self, nan = True, inf = True, neg = False, outliers=False):
        #Array of matched indices to remove
        rm = np.repeat(False, self.data.shape[0])
        if(nan): rm = np.logical_or(rm, np.apply_along_axis(np.any, arr=np.isnan(self.data), axis=1))
        if(inf): rm = np.logical_or(rm, np.apply_along_axis(np.any, arr=np.isinf(self.data), axis=1))
        if(neg): rm = np.logical_or(rm, np.apply_along_axis(np.any, arr=self.data<0, axis=1))
        if(outliers):
            percentiles = np.percentile(self.data, q = [1, 99], interpolation = "nearest")
            rm = np.logical_or(rm, np.apply_along_axis(np.any, arr=self.data>percentiles[1], axis=1))
            rm = np.logical_or(rm, np.apply_along_axis(np.any, arr=self.data<percentiles[0], axis=1))
            
        #Remove matched indices
        self.data = self.data[~rm]
        self.objs_indices = self.objs_indices[~rm]
        
    def count(self):
        return self.objs.size

    def plot(self,subplot=False, ylog = False, output=None):
        axes = None
        fig = plt.figure()

        plt.xticks(np.linspace(self.xmin,self.xmax, 9, endpoint=True))
        plt.yticks(np.linspace(self.ymin,self.ymax, 9, endpoint=True))
        fig.text(0.5, 0.04, self.colnames[self.xcol], ha='center', va='center')
        fig.text(0.04, 0.5, self.colnames[self.ycol], ha='center', va='center', rotation='vertical')
        plt.suptitle(self.name)
        
        for i in range(0,self.count()):
            monitor = Monitor(self, i, subplot, axes)
            if(i==0): axes = monitor.axes
            monitor.plot(color=self.colors[i],ylog=ylog)

        if(subplot):
            plt.subplots_adjust(wspace=0, hspace=0)            
        else:
            plt.legend(loc='best', frameon=True, fancybox=True, shadow=True)

        plt.ylim(self.ymin, self.ymax)
        plt.xlim(self.xmin, self.xmax)
        
        if(output==None): plt.show()
        else:
            extension = output.split(".")
            extension = extension[extension.__len__()-1]
            plt.savefig(output, papertype="a4", orientation="landscape", format=extension)

###########################################################################################################
#                                          Monitor Analysis and Plot                                      #
###########################################################################################################

class Monitor(Monitors):
    axes = None
    
    def __init__(self, monitors, i, subplot = False, shared_axes = None):
        self.axes = plt.gca()
        self.xmax = monitors.xmax
        self.xmin = monitors.xmin
        self.ymax = monitors.ymax
        self.ymin = monitors.ymin
        self.objs = [monitors.objs[i]]
        self.objs_indices = [monitors.objs_indices[i]]
        self.colnames = monitors.colnames
        self.xcol = monitors.xcol
        self.ycol = monitors.ycol
        self.data = monitors.data[np.in1d(monitors.objs_indices,i)]
        if(subplot):
            self.axes = plt.subplot(1, monitors.count(), i+1, sharex=shared_axes, sharey=shared_axes)
            if(i<>0):
                for tic in self.axes.yaxis.get_major_ticks():
                    tic.tick1On = tic.tick2On = False
                    tic.label1On = tic.label2On = False
                ticks = self.axes.xaxis.get_major_ticks()
                ticks[0].tick1On = ticks[0].tick2On = False
                ticks[0].label1On = ticks[0].label2On = False
        self.axes.grid(True)
            
    def plot(self, output = "plot.pdf", ylog=False, color=[0.0,0.0,0.0,0.0]):
        if(ylog): self.axes.set_yscale('log')
        self.axes.scatter(self.data[:,self.xcol], self.data[:,self.ycol], 1, label=self.objs[0], color=color)
        self.axes.legend(loc='best', frameon=True, fancybox=True, shadow=True)

###########################################################################################################
#                                                   Program                                               #
###########################################################################################################

args = ["-i", "../../tests/hpccg/multi_hpccg.out", "-f", "neg,inf,nan,outliers", "-t", "test", "-l", "-s", "-o", "test.pdf"]
options, args = parser.parse_args()
filters = options.filter.split(",")

monitors = Monitors(fname=options.input, xcol=options.xaxis-1, ycol=options.yaxis-1, name=options.title)

monitors.filter(nan=filters.__contains__("nan"),
                inf=filters.__contains__("inf"),
                neg=filters.__contains__("neg"),
                outliers=filters.__contains__("outliers"))

monitors.plot(subplot=options.split, ylog=options.log, output=options.output)

