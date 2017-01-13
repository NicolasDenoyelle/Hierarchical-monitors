import os
import re
import multiprocessing
import numpy                   as np
import numpy.random            as rand
import matplotlib.pyplot       as plt
import matplotlib.cm           as cm
import pandas                  as pd
from   sklearn                 import cluster, preprocessing, linear_model, metrics, svm
from   sklearn.externals       import joblib
from   sklearn.neural_network  import MLPRegressor
from   sklearn.ensemble        import RandomForestRegressor
from   sklearn.metrics.pairwise import polynomial_kernel
from   optparse                import OptionParser, make_option

###########################################################################################################
#                                                Parse options                                            #
###########################################################################################################    

inOpt = make_option("-i", "--input", type = "string", default = None,
                    help = "Data set input file")

outOpt = make_option("-o", "--output", type = "string", default = None,
                     help = "Output to file (static). Output format is set according to file extension")

titOpt = make_option("-t", "--title", type = "string", default = None,
                       help = "Plot title")

yOpt = make_option("-y", "--yaxis", type = "string", default = None,
                   help = "Use column named 'yaxis' instead of column 2 as Y values to plot")

logOpt = make_option("-l", "--log", action = "store_true",
                     help = "Plot yaxis in logscale")

xOpt = make_option("-x", "--xaxis", type = "string", default = None,
                   help = "Use column named 'xaxis' instead of column 1 as x values to plot")

splitOpt = make_option("-s", "--split", action = "store_true",
                       help = "Split monitors into several plots, one per hwloc obj")

filterOpt = make_option("-f", "--filter", type = "string", default = "nan,inf,outliers",
                        help = """Filter monitors trace to remove value that could create errors or unexpected plots.
                        Format: value,value,...
                        Values: inf, nan, neg, outliers.""")

logOpt = make_option("-l", "--log", action = "store_true",
                     help = "Plot yaxis in logscale")

clusterOpt = make_option("-c", "--cluster", default = None,
             help = """Split each monitor into several clusters based on monitor events.
             Created clusters are appended at the end of the dataframe.
             -c takes one argument method:<arg> as option indicating which clustering method to use and with what 
             argument.
             possible methods are:
             * kmeans:arg, with arg (integer) beeing the number of clusters to compute.
             * dbscan:arg, with arg (float) beeing the eps parameter of dbscan methods from sklearn (should be in ]0,1])
             """)

kernelOpt = make_option("-k", "--kernel", type = "string", default = None,
            help = """
            Apply a kernel to each monitor before fitting a model. 
            -k takes a list of arguments method0:<arg0>,method1:<arg1> as option indicating which kernel method to apply 
            and with what argument. Kernel are applied with given order 

            pipeline:<n_stage>
            Concatenate previous <arg> rows of previous samples to current row of the dataframe. 
            This is usefull for clustering or modeling to take into account the past in analysis such as clustering or
            model fitting.
            example:
              Nanoseconds   X0  Nanoseconds_0 X0_0 
                       10  7.3            NaN  NaN
                       20  4.2             10  7.3
                       30  1.6             20  4.2
                      NaN  NaN             30  1.6
            If pipeline options is greater than 0, then NaN values are created at the tip of the dataframe and data frame 
            is filtered consequently to remove NaN. Thus the frame tips of pipeline rows are also removed

            sample:<n_samples>
            Remove random samples from the trace to lower its size and processing time.
            proportion must be in ]0,1[.

            polynomial:<degree>
            Append pairwise polynomial function of <degree> on monitors features. 
            For example, if an input sample is two dimensional and of the form [a, b], 
            the degree-2 polynomial features are [a, b, a^2, ab, b^2].

            normalize
            Normalize the monitor dataframe. 
            This preprocessing should be used prior to any other preprocessing, including clustering and model fitting.
            """)

fitOpt = make_option("-m", "--model", type = "string", default = None,
         help = """
         fit y columns with help of others columns using the model provided as argument.
         If (--model = \"RANSAC\"), then use a RANdom SAmple Consensu model robust to noise.
         See: http://scikit-learn.org/stable/modules/linear_model.html#ransac-random-sample-consensus
         If (--model = \"Ridge\"), then use a Regularized linear regression with cross validation on several penalties.
         See: http://scikit-learn.org/stable/modules/linear_model.html#ridge-regression
         If (--model = \"Bayesian\"), then use a Bayesian model similar to ridge regression with automatic penalty tuning.
         See: http://scikit-learn.org/stable/modules/linear_model.html#bayesian-ridge-regression
         If (--model = \"SVR\"), then use a Support Vector Regression (SVR) model.
         See: http://scikit-learn.org/stable/modules/svm.html#regression
         If (--model = \"MLP\"), then use a Multi Layer Perceptron regressor.
         See: http://scikit-learn.org/stable/modules/neural_networks_supervised.html#regression
         If (--model = \"RandomForest\"), then use a decision tree model to fit the data.
         See: http://scikit-learn.org/stable/modules/generated/sklearn.ensemble.RandomForestRegressor.html#sklearn-ensemble-randomforestregressor
         """)

foptionOpt = make_option("--model-option", type = "string", default = None,
         help = """
         Use options for model fitting. Option argument is a list of comma separated pairs attribute:value.
         option0:value0, option1:value1, ...
         
         save:<file>
         Make the fitted model persistent

         load:<file>
         load a pré-existing model whether to update it or to make prediction on a new dataset.

         repeat:n_times
         When training MLP regression one can achieve a better fit by training the network several times on the same dataset 

         no-train
         Do not train the model. Must be used with load option.
         """)

parser = OptionParser(option_list = [inOpt, outOpt, titOpt, filterOpt, xOpt, yOpt, logOpt, splitOpt, clusterOpt, kernelOpt, fitOpt, foptionOpt])

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
    
    def __init__(self, dataframe, name=None, xcol = None, ycol=None):
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

    @classmethod
    def fromfilename(cls, fname, name=None, xcol = None, ycol=None):
        #Store id for title
        if(name == None): name = os.path.basename(fname)
        df = pd.read_table(fname, delim_whitespace=True)
        return cls(df, name, xcol, ycol)

    def pipeline(self, n_stage):
        data = None
        for i in range(self.count()):
            monitor = Monitor(self, i)
            monitor.pipeline(n_stage)
            if(i==0): data = monitor.data
            else:     data.append(monitor.data, ignore_index=False)
        self.data = data

    def sample(self, n_sample):
        data = None
        for i in range(self.count()):
            monitor = Monitor(self, i)
            monitor.sample(n_sample)
            if(i==0): data = monitor.data
            else:     data.append(monitor.data, ignore_index=False)
        data.reset_index(drop=True, inplace=True)    
        self.data = data

    def normalize(self, data = None):
        if(data is None): data = self.data
        if(not "NORMALIZED" in self.name.upper()): self.name = "Normalized " + self.name
        cols = data.columns
        X = data.ix[:, (cols != self.ycol) & (cols != 'Obj') & (cols != 'Nanoseconds')]
        cst = self.data[['Obj', 'Nanoseconds', self.ycol]]
        X = pd.DataFrame(preprocessing.normalize(X.values), columns=X.columns)
        cst.index = X.index
        data = pd.DataFrame(pd.concat([cst, X], axis=1, join='inner'), columns=cols)
        
    def get_Xy(self, data=None):
        if(data is None): data = self.data            
        y = data[self.ycol]
        X = data.ix[:, (data.columns != self.ycol) & (data.columns != 'Obj') & (data.columns != 'Nanoseconds')]
        return X.values, y.values

    def train_test_split(self, data=None, test_size=None, train_size=None, random=False):
        if(train_size != None):
            if(train_size < 0 or train_size > 1):
                print("Warning: train_size must be in ]0,1[")
                train_size = None
        if(train_size == None and test_size != None):
            if(test_size < 0 or test_size > 1):
                print("Warning: test_size must be in ]0,1[")
                test_size = None
            else: train_size = 1-test_size
        if(test_size == None and train_size == None):
            test_size = 0.25
            train_size = 0.75
        if(data is None): data = self.data

        train_range = range(int((float(data.shape[0])*train_size)))
        train_index = data.index[train_range]
        test_range = range(len(train_range), data.shape[0]-1)
        test_index = data.index[test_range]
        return data.ix[train_index,:], data.ix[test_index,:]

    
    def polynomial(self, degree=3):
        X,y = self.get_Xy() #Extract features of the frame
        #apply kernel
        poly_frame = pd.DataFrame(preprocessing.PolynomialFeatures(degree).fit_transform(X))
        #Remove redundant columns
        poly_frame = poly_frame.ix[:,X.shape[1]+1:]
        #join to monitor dataframe
        self.data = pd.concat([self.data, poly_frame], axis=1, join='inner')
    
        
    def model(self, model_name, save = None, load = None, do_train=True, repeat=1):
        model = None
        
        if(load): model = joblib.load(load)
        else:
            if(model_name.upper() == "RANSAC"):   model = linear_model.RANSACRegressor(linear_model.LinearRegression())
            if(model_name.upper() == "BAYESIAN"): model = linear_model.BayesianRidge()
            if(model_name.upper() == "RIDGE"):    model = linear_model.RidgeCV(alphas=[0.1, 1.0, 10.0])
            if(model_name.upper() == "SVR"):      model = svm.SVR(cache_size=4000)
            if(model_name.upper() == "RANDOMFOREST"):
                model = RandomForestRegressor(n_estimators=10, n_jobs=1, min_impurity_split=1e-3, verbose=0, warm_start=True)
            if(model_name.upper() == "MLP"):
                model = MLPRegressor(hidden_layer_sizes=[100, 100, 100], shuffle=False, warm_start=True)
            if(model == None):
                print("Wrong model name provided: " + model_name)
                return

        train, test = self.train_test_split()
        X_train, y_train = self.get_Xy(train)
        X_test, y_test = self.get_Xy(test)

        for i in range(repeat):
            if(repeat>1): print("step " + str(i+1) + "/" + str(repeat) + "...")
            if(do_train):
                print("Fitting " + model_name + "model...")        
                model.fit(X_train, y_train)

            print("Predicting using " + model_name + " model") 
            y_pred = model.predict(X_test)
            score = metrics.explained_variance_score(y_test, y_pred)
            print("Output explained variance score = " + str(score))                
                
        print("Value less than 0: the model scores worth than the mean prediction.")                
        print("Value close to 0: the model scores as well as the mean prediction.")
        print("Value close to 1: the model predicts almost perfectly.")

        if(save):
            joblib.dump(model, save)
            print("model saved to " + save)
        
        return X_test, y_test, y_pred, score
        
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
            cols = self.data.columns[range(1, self.data.shape[1])]
            self.data[self.data[cols]<0] = np.nan
        if(nan):
            self.data.dropna(how='any', inplace=True)
            self.nan = True
        if(outliers):
            index = np.repeat(True, self.data.shape[0])
            #filter column by column
            for i in range(2,self.data.shape[1]):
                col = self.data.ix[:,i].values
                xp = np.nanpercentile(col, q = [1, 99], interpolation = "nearest")
                index = np.logical_or(index, col<xp[1])
                index = np.logical_or(index, col>xp[0])
                if(self.data.columns[i] == self.xcol): self.xmin, self.xmax = xp[0], xp[1]
                if(self.data.columns[i] == self.ycol): self.ymin, self.ymax = xp[0], xp[1]                
            self.data = self.data[index]
            
        
    def count(self):
        return self.objs.size
    
    def cluster(self, method="dbscan", arg="0.5"):
        monitors = []
        #create clustered monitors
        for i in range(self.count()):
            monitor = Monitor(self, i)
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
            monitor = Monitor(self, i)
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
    def __init__(self, monitors, i=0):
        self.xmax = monitors.xmax
        self.xmin = monitors.xmin
        self.ymax = monitors.ymax
        self.ymin = monitors.ymin
        self.objs = np.array([monitors.objs[i]])
        self.xcol = monitors.xcol
        self.ycol = monitors.ycol
        self.nan = monitors.nan
        subset = monitors.data['Obj'] == self.objs[0]
        self.name = monitors.name + "_" + self.objs[0]
        self.data = monitors.data[subset]
        
    def pipeline(self, n_stage = 0):
        if(n_stage <= 0): return
        cols = self.data.columns
        tmp = self.data.ix[:, (cols != 'Obj') & (cols != 'Nanoseconds') & (cols != self.ycol)].copy()
        
        for i in range(n_stage):
            #Create new data frame without 'Obj' and y columns            
            sub = tmp.copy()
            
            #Append the cluster id to the columns name
            columns = sub.columns.copy().tolist()
            for j in range(len(columns)): columns[j] = columns[j] + "_" + str(i)
            sub.columns = columns

            #stride index
            index = sub.index.copy().tolist()
            for j in range(i):                                          
                index.pop(0)                                        #Remove first lines
                index.append(i+1+sub.index[len(sub.index)-1])       #Append new lines
            sub.index = index
            
            #Left join with monitor dataframe based on frames index.
            self.data = pd.merge(self.data, sub, how="left", left_index=True, right_index=True, sort=False)
            
        if(self.nan): #Filter NaN introduced
            begin = self.data.index[n_stage]
            end = self.data.index[self.data.shape[0]-n_stage-1]
            self.data = self.data[begin:end]
                    
        
    def __assign_clusters__(self, labels):
        obj = self.objs[0]
        
        #make hard copy of the dataframe to allow modifications
        self.data = self.data.copy()

        #count clusters
        n_clusters = np.sum(np.unique(labels)>=0)
        print("found " + str(n_clusters) + "clusters")
        
        #Create new objects list
        self.objs = []
        for i in range(n_clusters): self.objs.append(obj + "_(" + str(i) + ")")
        #Assign new objects in 'Obj' Column.
        objs = []
        for i in range(labels.size):
            if(labels[i]<0): objs.append("Noise")
            else: objs.append(self.objs[labels[i]])
        self.data['Obj'] = objs
        #Cast self to parent Monitors class to prevent usage of Monitor class methods
        self.__class__ = Monitors
        
    def cluster_kmeans(self, n_clusters=8):
        if(self.objs.size>1): print("Cannot cluster several monitors."); return
        n_proc = multiprocessing.cpu_count()
        model = cluster.KMeans(n_clusters=n_clusters, max_iter=300, tol=1e-6, n_init=n_proc, verbose=0, n_jobs=n_proc)
        kmeans = model.fit()
        self.__assign_clusters__(kmeans.labels_)

    def cluster_dbscan(self, eps=5e-1):
        if(self.objs.size>1): print("Cannot cluster several monitors."); return
        n_proc = multiprocessing.cpu_count()
        model = cluster.DBSCAN(eps)
        dbscan = model.fit()
        self.__assign_clusters__(dbscan.labels_)
        
    def sample(self, n_sample):
        if(n_sample <= 0): return
        self.data = self.data.sample(n_sample, replace=True)

    def plot(self, axes, ylog=False, color=[0.0,0.0,0.0,0.0]):
        if(ylog): axes.set_yscale('log')
        axes.scatter(self.data[self.xcol], self.data[self.ycol], 1, label=self.objs[0], color=color)
        axes.set_xlim([self.xmin, self.xmax])
        axes.set_ylim([self.ymin, self.ymax])
        axes.legend(loc='best', frameon=True, fancybox=True, shadow=True)
        
###########################################################################################################
#                                                   Program                                               #
###########################################################################################################

args = [
    "-i", "/home/ndenoyel/Documents/specCPU2006/filtered.out",
#    "-i", "/home/ndenoyel/Documents/hmon/tests/hpccg/blob2.out",
    "-f", "neg,inf,nan,outliers",
    "-t", "test",
#    "-c", "dbscan:0.3",
#    "-c", "kmeans:4",
    "-k", "sample:500000",        
    "-m", "MLP",
    "--model-option", "load:model.pkl,save:model.pkl,repeat:10",
#    "--model-option", "load:model.pkl,no-train",    
    "-s"]

#Parsing options
options, args = parser.parse_args()

#Parsing model options
load = None
save = None
train = True
repeat = 1
if(options.model_option):
    model_options = options.model_option.split(",")
    for option in model_options:
        opt = option.split(":")[0].upper()
        if(opt == "LOAD"):
            load = option.split(":")[1]
        if(opt == "SAVE"):
            save = option.split(":")[1]
        if(opt == "NO-TRAIN"):
            train=False
        if(opt == "REPEAT"):
            repeat=int(option.split(":")[1])
            
if( not train and load is None):
    print("no-train option must be combined with load option.")
    exit()

#Reading input
print("Reading: " + options.input + "...")
monitors = Monitors.fromfilename(fname=options.input,
                                 xcol=options.xaxis,
                                 ycol=options.yaxis,
                                 name=options.title)

#Filtering Frame
filters = options.filter.split(",")
print("filtering: " + str(filters) + "...")
monitors.filter(nan=filters.__contains__("nan"),
                inf=filters.__contains__("inf"),
                neg=filters.__contains__("neg"),
                outliers=filters.__contains__("outliers"))

#Applying kernels
if(options.kernel):
    kernels = options.kernel.split(",")
    #to upper case for stirng comparison
    kernels[:] = [k.upper() for k in kernels] 

    for k in kernels:
        split = k.split(":")
        kernel = split[0]
        if(kernel == "NORMALIZE"):
            print("preprocessing: normalize...")
            monitors.normalize()
        if(kernel == "PIPELINE"):
            print("preprocessing: pipeline...")
            monitors.pipeline(int(split[1]))
        if(kernel == "SAMPLE"):
            print("preprocessing: sample...")
            monitors.sample(int(split[1]))
        if(kernel == "POLYNOMIAL"):
            print("preprocessing: polynomial...")
            monitors.polynomial(int(split[1]))

#Clustering monitors
if(options.cluster != None):
    cluster_args = options.cluster.split(":")
    print("Clustering monitors with method: " + cluster_args[0] + "...")
    monitors.cluster(cluster_args[0], cluster_args[1])
            
#Modeling monitors
if(options.model):
    monitors.model(options.model, load=load, save=save, do_train=train, repeat=repeat)

#Plotting monitors
print("Plotting monitors...")
monitors.plot(subplot=options.split, ylog=options.log, output=options.output)

