import os
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.cm as cm
import pandas as pd
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
                   help = "Use column 'yaxis' instead of column 2 as Y values to plot")

logOpt = make_option("-l", "--log", action = "store_true",
                     help = "Plot yaxis in logscale")

xOpt = make_option("-x", "--xaxis", type = "string", default = None,
                   help = "Use column 'xaxis' instead of column 1 as x values to plot")

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
    data = None       #Trace data columns excluding hwloc obj column
    objs = []         #List of objs in trace
    xcol = ""         #Index of x column
    ycol = ""         #Index of y column
    xmax = 0          #maximum value of x inside trace
    xmin = 0          #minimum value of x inside trace
    ymax = 0          #maximum value of y inside trace
    ymin = 0          #minimum value of x inside trace
    colors = []       #monitors colors
    
    def __init__(self, fname, name=None, xcol = None, ycol=None):
        #Store id for title
        if(name == None): self.name = os.path.basename(fname)
        else: self.name = name
        
        #Load data frame
        self.data = pd.read_table(fname, delim_whitespace=True)
        self.objs = np.unique(self.data[self.data.columns[0]])

        
        #select xcol and ycol
        if(xcol == None): self.xcol = self.data.columns[1]
        else: self.xcol = xcol        
        if(ycol == None): self.ycol = self.data.columns[2]
        else: self.ycol = ycol
        
        #prepare color rainbow        
        self.colors = cm.rainbow(np.linspace(0, 1, self.objs.size))
        
        #Set x and y stats
        self.xmax = np.max(self.data[self.xcol])
        self.xmin = np.min(self.data[self.xcol])
        self.ymax = np.max(self.data[self.ycol])
        self.ymin = np.min(self.data[self.ycol])

    def filter(self, nan = True, inf = True, neg = False, outliers=False):
        if(inf): self.data.replace([np.inf, -np.inf], np.nan)
        if(neg): self.data[self.data<0] = np.nan        
        if(outliers):
            xp = np.nanpercentile(self.data[self.xcol], q = [1, 99], interpolation = "nearest")
            self.xmin , self.xmax = xp[0], xp[1]
            yp = np.nanpercentile(self.data[self.ycol], q = [1, 99], interpolation = "nearest")            
            self.ymin, self.ymax = yp[0], yp[1]
        if(nan): self.data.dropna(how='any', inplace=True)
                    
    def count(self):
        return self.objs.size

    def plot(self,subplot=False, ylog = False, output=None):
        axes = None
        fig = plt.figure()

        plt.xticks(np.linspace(self.xmin,self.xmax, 9, endpoint=True))
        plt.yticks(np.linspace(self.ymin,self.ymax, 9, endpoint=True))
        fig.text(0.5, 0.04, self.xcol, ha='center', va='center')
        fig.text(0.04, 0.5, self.ycol, ha='center', va='center', rotation='vertical')
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

    def __init__(self, monitors, i = 0, subplot = False, shared_axes = None):
        self.axes = plt.gca()
        self.xmax = monitors.xmax
        self.xmin = monitors.xmin
        self.ymax = monitors.ymax
        self.ymin = monitors.ymin
        self.objs = [monitors.objs[i]]
        self.xcol = monitors.xcol
        self.ycol = monitors.ycol
        self.data = monitors.data[ monitors.data[monitors.data.columns[0]] == self.objs[0] ]
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
        self.axes.scatter(self.data[self.xcol], self.data[self.ycol], 1, label=self.objs[0], color=color)
        self.axes.legend(loc='best', frameon=True, fancybox=True, shadow=True)

###########################################################################################################
#                                                   Program                                               #
###########################################################################################################

args = ["-i", "/home/ndenoyel/Documents/specCPU2006/filtered.out", "-f", "neg,inf,nan,outliers", "-t", "test", "-s"]
options, args = parser.parse_args()
filters = options.filter.split(",")


monitors = Monitors(fname=options.input, xcol=options.xaxis, ycol=options.yaxis, name=options.title)

monitors.filter(nan=filters.__contains__("nan"),
                inf=filters.__contains__("inf"),
                neg=filters.__contains__("neg"),
                outliers=filters.__contains__("outliers"))

monitors.plot(subplot=options.split, ylog=options.log, output=options.output)

