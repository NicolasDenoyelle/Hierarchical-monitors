#########################################################################################################
###                                              Handle Options                                         #
#########################################################################################################
library("optparse")

##Parse options
inOpt = make_option(
    opt_str = c("-i", "--input"),
    type = "character",
    default = NULL,
    help = "Data set input file"
)

outOpt = make_option(
    opt_str = c("-o", "--output"),
    type = "character",
    default = NULL,
    help = "Output pdf file (static)"
)

titleOpt = make_option(
    opt_str = c("-t", "--title"),
    type = "character",
    default = NULL,
    help = "Plot title"
)

filterOpt = make_option(
    opt_str = c("-f", "--filter"),
    type = "character",
    default = NULL,
    help = "Provide a regex to filter input monitor base on their id (first column)"
)

yOpt = make_option(
    opt_str = c("-y", "--yaxis"),
    type = "integer",
    default = 4,
    help = "Use column yaxis instead of column 4 as Y values to plot"
)

logOpt = make_option(
    opt_str = c("-l", "--log"),
    type = "logical",
    default = FALSE,
    action = "store_true",
    help = "Plot yaxis in logscale"
)

xOpt = make_option(
    opt_str = c("-x", "--xaxis"),
    type = "integer",
    default = 3,
    help = "Use column xaxis instead of column 3 as x values to plot"
)

splitOpt = make_option(
    opt_str = c("-s", "--split"),
    type = "logical",
    default = FALSE,
    action = "store_true",
    help = "Split each monitor into several plots, one per hwloc obj"
)

clusterOpt = make_option(
    opt_str = c("-c", "--cluster"),
    type = "logical",
    default = FALSE,
    action = "store_true",
    help = "Split each monitor into several clusters using dbscan on monitor events"
)

fitOpt = make_option(
    opt_str = c("-m", "--model"),
    type = "character",
    default = NULL,
    help = "fit y data according to model argument.

  If (--model = \"linear\"), then use a linear model of events (except y) to fit y.
  Try to load model from file named after monitor id. If file doesn't exists, compute linear model and save to file.
  If (--model = \"nnet\"), same as linear but use a neural network instead of linear regression model. 
  If this monitor neural network already exists then load the one existing and do not train the network.
  If (--model = \"nnet:train\"), then fitting a monitor will update its existing neural network or create a new one to train.
  If (--model = \"nnet:2\"), train a neural network using 2 timesteps for each sample of the training set.
  If (--model = \"rsnns\"), fit with a recurrent neural network.
  If (--model = \"rsnns:train\"), train (not update) a new network with new input.
  If (--model = \"periodic\"), then fit y column with a fourier serie of time column.
  If (--model = \"gaussian\"), then fit y column as a normal distribution, and output y*P(y) on cross validation set."
)

winOpt = make_option(
    opt_str = c("-w", "--window"),
    type = "integer",
    default = 0,
    help = "number of points to plot. If > 0, then plot by part dynamicaly"
)

freqOpt = make_option(
    opt_str = c("-u", "--update"),
    type = "numeric",
    default = 0.5,
    help = "frequency of read from trace file and plot in seconds. Plot should be displayed as parts of a large or updated trace.
  In this case -w points will be plot and window will move by -w/4 steps ahead every -f seconds"
)

optParse = OptionParser(option_list = c(
                            inOpt,
                            outOpt,
                            titleOpt,
                            filterOpt,
                            xOpt,
                            yOpt,
                            logOpt,
                            splitOpt,
                            clusterOpt,
                            fitOpt,
                            winOpt,
                            freqOpt))

options = parse_args(
    optParse,
    args = commandArgs(trailingOnly = TRUE),
    print_help_and_exit = TRUE,
    positional_arguments = FALSE
)

########################################
### Provide a title according to options
########################################
get_title <- function(str) {
    if (!is.null(options$title)) {
        options$title
    } else if(!is.null(str)){
        str
    } else {
        options$input
    }
}


#########################################################################################################
###                                           monitor modeling                                          #
#########################################################################################################



##############################################################################
### Prepare monitor features before applying learning algorithm
###
### @arg monitor: the monitor frame where to extract features
### @arg test.ratio: the amount of row in [0,1[ to take from monitor for validation set.
### @arg shuffle: If TRUE, shuffle monitor rows.
### @arg past: an integer giving the amount of previous row to append on features columns,
###            in order to take into account passed features.
### @arg fit: an already obtained fit object to scale new features accordingly.
###
### @attr X: normalized inputs
### @attr X.centers: X means for normalization
### @attr X.scales: X scales for normalization
### @attr y: normalized output
### @attr y.center: y mean for (de)normalization
### @attr y.scale: y scale for (de)normalization
### @attr train.set: the setof indexes to use for training
### @attr test.set: the set of indexes to use for test if test.ratio is not set to 0
##############################################################################
monitor.fit.prepare <- function(monitor, test.ratio = 0, shuffle=FALSE, past=0, fit=NULL){
    ret = NULL
    
    ##Features and target
    X = monitor[, setdiff(4:ncol(monitor), options$yaxis)]
    y = monitor[, options$yaxis]

    ##Append past timestep columns
    if(!is.null(fit)){
        if(ncol(fit$X)>ncol(X)){past = ncol(fit$X)/ncol(X)-1}
    }
    if(past>0){
        passed = X
        for(r in 1:past){
            passed = rbind(rep(0, ncol(passed)), passed)
            X = cbind(X, passed[1:nrow(X),])
        }
        ##Rename columns
        colnames(X) = sapply(1:ncol(X), function(i){sprintf("V%d",i)})
    }

    ##Normalize features and target
    if(is.null(fit)){
        X.centers = sapply(1:ncol(X), function(i){mean(X[,i])})
        y.center = mean(y)
        X.scales = sapply(1:ncol(X), function(i){max(X[,i])-min(X[,i])})
        y.scale = max(y, na.rm=T)-min(y, na.rm=T)
    } else {
        X.centers = fit$X.centers
        y.center = fit$y.center
        X.scales = fit$X.scales
        y.scale = fit$y.scale
    }
    sapply(1:ncol(X), function(i){X[,i] = (X[,i]-X.centers[i])/X.scales[i]})
    y = (y-y.center)/y.scale

    #Shuffle rows
    if(shuffle){
        shuffle = sample(1:nrow(monitor));
    } else{shuffle = 1:nrow(monitor)}
    
    ##Train and test sets
    train.set = 1:nrow(monitor)
    test.set = train.set
    if(test.ratio > 0){
        test.limit = round(nrow(monitor)*test.ratio)
        test.set = shuffle[1:test.limit]
        train.set = shuffle[(test.limit+1):nrow(monitor)]
    }

    ##Assign attributes
    ret$X = X
    ret$y = y
    ret$X.centers = X.centers
    ret$y.center = y.center
    ret$X.scales = X.scales
    ret$y.scale = y.scale
    ret$train.set = train.set
    ret$test.set = test.set
    ret
}

monitor.fit.denormalize <- function(fit, y){y*fit$y.scale + fit$y.center}

######################################################################
### Compute linear model of monitor output given monitor other events.
### Output cross validation set and prediction
######################################################################
monitor.linear.fit <- function(monitor, save = NULL, recurse = 0){
    if(ncol(monitor)<=4){
        print("linear plot cannot be applied on a monitor with a single event")
        return(NULL)
    }
    
    if(is.null(save) || !file.exists(save)){
        fit = monitor.fit.prepare(monitor, shuffle=T, test.ratio=0.1, past=recurse)
        if(ncol(fit$X)==1){
            fit.lm = lm(fit$y[fit$train.set] ~ fit$X[fit$train.set])  
        } else {
            fit.lm = lm(fit$y[fit$train.set] ~ .^5, data = fit$X[fit$train.set,])  
        }
        if(!is.null(save) && !file.exists(save)){
            save(list=c("fit.lm","fit"), file = save)
        }
        return (list(fit$test.set, monitor.fit.denormalize(fit,predict(fit.lm, newdata = fit$X[fit$test.set,]))))
        
    } else if(!is.null(save) && file.exists(save)){
        load(file = save)
        fit = monitor.fit.prepare(monitor, fit=fit)
        return (list(fit$test.set, monitor.fit.denormalize(fit,predict(fit.lm, newdata = fit$X[fit$test.set,]))))
    }
}

##############################################################################
### Compute non-linear model of monitor based on an artificial neural network.
### Output cross validation set and prediction
##############################################################################
monitor.nnet.fit <- function(monitor, save = NULL, recurse = 0, train=FALSE){
    ##check there are features and target
    if(ncol(monitor)<=4){
        print("linear plot cannot be applied on a monitor with a single event")
        return(NULL)
    }

    ##Comment lines 65-66 from calculate.neuralnet to avoid error
    ##fixInNamespace("calculate.neuralnet", pos="package:neuralnet")
    ##Or
    ##install.packages("neuralnet_1.33.tar.gz", repos=NULL, type="source")
    ##Compute input/output, scaled input/output, cross validation set
    library("neuralnet")

    fit = NULL
    model = NULL
    
    ##Load an already existing model
    if(!is.null(save) && file.exists(save)){load(file = save)}
 
    ##Train the model
    if(train || is.null(model)){
        fit = monitor.fit.prepare(monitor, shuffle=T, test.ratio = 0.1, fit=fit, past=recurse)
        f = as.formula(sprintf("fit$y[fit$train.set] ~ %s",paste(names(fit$X), collapse="+")))
        print("fitting with neural network. this may take a few minutes...")
        model = neuralnet(
            f,
            data = fit$X[fit$train.set,],
            rep = 1,
            linear.output = TRUE,
            threshold = 0.01,
            stepmax = 1000,
            hidden = c(ncol(fit$X) * 2, ncol(fit$X)),
            startweights = model$weights
        )
        if(!is.null(save)){save(list=c("model","fit"), file = save)}
    } else {
        fit = monitor.fit.prepare(monitor, shuffle=T, fit=fit, past=recurse)
    }

    ##Predict on cross validation set
    model.pred = compute(model, fit$X[fit$test.set,])

    ##output error
    list(fit$test.set, monitor.fit.denormalize(fit, model.pred$net.result))
}

#################################################################
### Compute non-linear model of monitor based on a recurrent ANN.
### Output cross validation set and prediction
#################################################################
monitor.rsnns.fit <- function(monitor, save = NULL, train = FALSE){
    library("RSNNS")

    model = NULL
    fit = NULL
    
    ##Load an already existing model
    if(!is.null(save) && file.exists(save)){load(file = save)}
    
    ##Train the model
    if(train || is.null(model)){
        fit = monitor.fit.prepare(monitor, shuffle=T, test.ratio = 0.1, fit=fit)
        f = as.formula(sprintf("fit$y ~ %s",paste(names(fit$X), collapse="+")))
        print("fitting with neural network. this may take a few minutes...")
        model = elman(x=fit$X[fit$train.set,],
                       y=fit$y[fit$train.set],
                       size=c(ncol(fit$X) * 2),
                       maxit=1000,
                       shufflePatterns=FALSE,
                       linOut=TRUE,
                       inputsTest=fit$X[fit$test.set,],
                       targetsTest=fit$y[fit$test.set])
        pred.y = model$fittedTestValues
        ##Save network
        if(!is.null(save)){save(list=c("model", "fit"), file = save)}
    } else {
        fit = monitor.fit.prepare(monitor, fit=fit)
        pred.y = predict(model, newdata = fit$X[fit$test.set,])
    }

    ##output error
    list(fit$test.set, monitor.fit.denormalize(fit, pred.y))
}

########################################################################
### Model monitor as a gaussian distribution using random set of sample.
### Output cross validation set and prediction
########################################################################
monitor.gaussian.fit <- function(monitor){
    fit = monitor.fit.prepare(monitor, shuffle=T, test.ratio = 0.1)
    y = fit$y[fit$train.set]
    P.y = dnorm(fit$y[fit$test.set], mean=mean(y), sd = sd(y))
    list(fit$test.set, monitor.fit.denormalize(fit, fit$y[fit$test.set]*P.y))
}

#################################################################
## Credit to http://www.di.fc.ul.pt/~jpn/r/fourier/fourier.html 
## returns the x.n time series for a given time sequence (ts) and
## a vector with the amount of frequencies k in the signal (X.k)
#################################################################
get.trajectory <- function(X.k,ts,acq.freq) {
    N   <- length(ts)
    i   <- complex(real = 0, imaginary = 1)
    x.n <- rep(0,N)           # create vector to keep the trajectory
    ks  <- 0:(length(X.k)-1)
    for(n in 0:(N-1)) {       # compute each time point x_n based on freqs X.k
        x.n[n+1] <- sum(X.k * exp(i*2*pi*ks*n/N)) / N
    }
    Re(x.n) * acq.freq 
}


###########################################################################
### Compute fourier transform of monitor output given monitor other events.
### Event must be uniform accross time.
### Output cross validation set and prediction
###########################################################################
monitor.frequency.fit <- function(monitor){
    fit.range = 1:(nrow(monitor)/4)
    pred.range = setdiff(1:nrow(monitor), fit.range)
    fit.y = monitor[fit.range, options$yaxis]
    acq.freq = 10^-6*(monitor[nrow(monitor),id.time] - monitor[1,id.time])/nrow(monitor)
    X.k = fft(fit.y)
    fit.pred = get.trajectory(X.k, monitor[pred.range,id.time], acq.freq)    
    list(pred.range, fit.pred)
}


#################################################
### Relative Root Mean Squared Error
### Compare Error to Standard Deviation
#################################################
rrmse <- function(y,fit) sqrt(mean((y-fit)^2))/sd(y)


####################
### Plot monitor fit
####################
monitor.plot.fit <-
    function(monitor,
             type = "linear",
             n = 6,
             pch = 1,
             col = 1) {
        
        fit = NULL
        
        if (type == "linear") {
            fit = monitor.linear.fit(monitor, save = sprintf("%s_%s_linear.rda", monitor[1,1], monitor[1,2]))    
        } else if(grepl("nnet", substr(type, start=0, stop=nchar("nnet")))){
            if(type == "nnet"){
                fit = monitor.nnet.fit(monitor, save = sprintf("%s_%s_nnet.rda", monitor[1,1], monitor[1,2]), train = F)
            } else if(type == "nnet:train"){
                fit = monitor.nnet.fit(monitor, save = sprintf("%s_%s_nnet.rda", monitor[1,1], monitor[1,2]), train = T)
            } else {
                recurse = as.integer(substr(type, start=nchar("nnet:")+1, stop = nchar(type)))
                fit = monitor.nnet.fit(monitor, save = sprintf("%s_%s_nnet.rda", monitor[1,1], monitor[1,2]), train = T, recurse = recurse)
            }
        } else if(grepl("rsnns", substr(type, start=0, stop=nchar("rsnns")))){
            if(type == "rsnns"){
                fit = monitor.rsnns.fit(monitor, save = sprintf("%s_%s_rsnns.rda", monitor[1,1], monitor[1,2]), train = F)
            } else if(type == "rsnns:train"){
                fit = monitor.rsnns.fit(monitor, save = sprintf("%s_%s_rsnns.rda", monitor[1,1], monitor[1,2]), train = T)
            }
        } else if(type == "periodic"){
            fit = monitor.frequency.fit(monitor)
        } else if(type == "gaussian"){
            fit = monitor.gaussian.fit(monitor)
        }

        if(!is.null(fit)){
            points(monitor[fit[[1]],options$xaxis], fit[[2]], pch = pch, col = col)
            return(rrmse(monitor[fit[[1]],options$yaxis], fit[[2]]))
        }
    }

#########################################################################################################
###                                          monitors partitionning                                     #
#########################################################################################################

####################################
### list monitors in a monitor frame
####################################
monitors.list <- function(monitors){
    ids = unique(monitors[, id.id])
    ret = vector("list", length = length(ids))
    for(i in 1:length(ids)){
        ret[[i]] = subset(monitors, monitors[,id.id] == ids[[i]])
    }
    ret
}

###############################################################
### Split a monitor in several monitors using dbscan clustering
###############################################################
monitor.cluster <- function(monitor){
    library("dbscan")
    if(nrow(monitor)<=10){return(list(monitor))}
    p = monitor[, 4:ncol(monitor)]
    p = scale(p, center = TRUE, scale = TRUE)
    model = dbscan(x = p, eps=.2)
    n = max(model$cluster)
    cluster.set = vector("list", length=n)
    for (i in 1:n) cluster.set[[i]] = monitor[model$cluster == i,]
    cluster.set
}

############################################################
### Split a monitor into several hwloc_obj (column 2)
### If option cluster is set, also split in several clusters
############################################################
monitor.split <- function(monitor) {
    obj.list = unique(monitor[, id.obj])
    split.set = vector("list", length = length(obj.list))
    for (i in 1:length(obj.list)) {
        split.set[[i]] = subset(monitor, monitor[, id.obj] == obj.list[i])
    }
    
    if (options$cluster) {
        cluster.set = c()
        for (i in 1:length(split.set))
            cluster.set = c(cluster.set, monitor.cluster(split.set[[i]]))
        return(cluster.set)
    }
    split.set
}

#########################################################################################################
###                                              monitor utils                                          #
#########################################################################################################

monitors.frame = NULL
id.time = 3
id.obj = 2
id.id  = 1

################################
### Get a monitor mapping object
################################
monitor.obj <- function(monitor)
    as.character(monitor[1, id.obj])

###########################
### Get a monitor obj index
###########################
monitor.obj.index <- function(monitor) 
    as.integer(strsplit(monitor.obj(monitor), ":", fixed=T)[[1]][2])

####################
### Get a monitor id
####################
monitor.id <- function(monitor) 
    as.character(monitor[1, id.id])

#####################################################################
### Filter NA cols then inf, NaN, NA lines, and return filtered frame
#####################################################################
monitor.check <- function(frame){
    cols.del = c()
    rows.del = c()
    ##Remove columns with only NA
    for (i in 4:ncol(frame)) {
        if (length(which(!is.na(frame[, i]))) == 0)
            cols.del = c(cols.del, i)
    }
    frame = frame[, setdiff(1:ncol(frame), cols.del)]
    
    ##Remove lines with NA, NaN and inf
    for (i in 1:nrow(frame)) {
        ##Remove unplottable y values in log scale.
        if(is.nan(frame[i,options$yaxis]) || (options$log && frame[i,options$yaxis]<=0)){
            rows.del = c(rows.del, i)
        } else {
            for (j in 4:ncol(frame)) {
                if (is.infinite(frame[i, j]) ||
                    is.na(frame[i, j]) || is.nan(frame[i, j])) {
                    rows.del = c(rows.del, i)
                    break
                }
            }
        }
    }
    frame = frame[setdiff(1:nrow(frame), rows.del),]
    frame
}

####################################################################
### set column names of a monitor frame and return the updated frame
####################################################################
monitors.set.colnames <-function(frame){
    names = colnames(frame)
    names[1] = "id"
    names[2] = "hwloc_obj"
    names[3] = "nanoseconds"
    colnames(frame) = names
    frame
}

#############################################################
### Acquire monitors from input and return a list of monitors
#############################################################
monitors.read <- function(){
    monitors.frame <<- read.table(options$input, stringsAsFactors=F, fill=T)
    monitors.frame <<- monitors.set.colnames(monitors.frame)
    if (!is.null(options$filter))
        monitors.frame <<- monitors.frame[grepl(options$filter, monitors.frame[, id.id], ignore.case = TRUE),]
    monitors.list(monitors.frame)
}


## Monitors limits and ticks
monitors.xlim = new.env(hash = T)
monitors.xticks = new.env(hash = T)
monitors.ylim = new.env(hash = T)
monitors.yticks = new.env(hash = T)

##################################################
### Save in environment a monitor limits and ticks
##################################################
monitor.set.limits <- function(monitor){
    ## xlim = monitors.xlim[[monitor.id(monitor)]]
    ## ylim = monitors.xlim[[monitor.id(monitor)]]
    xmax = max(monitor[,options$xaxis], na.rm = TRUE)
    xmin = min(monitor[,options$xaxis], na.rm = TRUE)
    ymax = max(monitor[,options$yaxis], na.rm = TRUE)
    ymin = min(monitor[,options$yaxis], na.rm = TRUE)
    monitors.xlim[[monitor.id(monitor)]] <<- c(xmin, xmax)
    monitors.ylim[[monitor.id(monitor)]] <<- c(ymin, ymax)
    monitors.xticks[[monitor.id(monitor)]] <<- seq(from = xmin, to = xmax, by = (xmax-xmin)/10)
    monitors.yticks[[monitor.id(monitor)]] <<- seq(from = ymin, to = ymax, by = (ymax-ymin)/10)
}

#########################################################################################################
###                                              monitor ploting                                        #
#########################################################################################################
ylog = ""; if(options$log){ylog = "y"}

#############################
### Plot a unsplitted monitor
#############################
monitor.plot.merge <- function(monitor) {
    errors = c()
    monitor.list = monitor.split(monitor)
    for (i in 1:length(monitor.list)) {
        m = monitor.list[[i]]
        plot(
            x = m[, options$xaxis],
            y = m[, options$yaxis],
            log = ylog,      
            type = 'p',
            col = i,
            xlim = monitors.xlim[[monitor.id(monitor)]],
            ylim = monitors.ylim[[monitor.id(monitor)]],
            axes = FALSE,
            ann = FALSE,
            pch = i,
            panel.first = abline(
                h = monitors.yticks[[monitor.id(monitor)]],
                v = monitors.xticks[[monitor.id(monitor)]],
                col = "darkgray",
                lty = 3
            )
        )
        if (!is.null(options$model))
            errors = c(errors ,monitor.plot.fit(m, type=options$model, pch=i+1, col=i+1))
        if (i<length(monitor.list))
            par(new = TRUE)
    }
    title(
        main = get_title(NULL),
        ylab = monitor.id(m),
        xlab = colnames(monitor)[options$xaxis]
    )
    axis(1, at = monitors.xticks[[monitor.id(monitor)]])
    axis(2, at = monitors.yticks[[monitor.id(monitor)]])
    sequence = 1:length(monitor.list)
    legend.text = sapply(sequence, function(i) monitor.obj(monitor.list[[i]]), simplify = "array")
    legend.col = sequence
    legend.pch = sequence
    if (!is.null(options$model)){
        legend.text = c(legend.text, sapply(sequence, function(i) sprintf("%s prediction (rrmse=%f)",
                                                                          options$model, errors[[i]]), simplify = "array"))
        legend.col = c(legend.col, 2:(length(monitor.list)+1))
        legend.pch = c(legend.pch, 2:(length(monitor.list)+1))
    }
    legend(
        "bottomright",
        legend = legend.text,
        col = legend.col,
        pch = legend.pch,
        bg = "white"
    )
}

#####################################################################
### Split a monitor into parts and plot these parts in a split device
#####################################################################
monitor.plot.split <- function(monitor) {
    monitor.list = monitor.split(monitor)
    monitor.list = monitor.list[order(sapply(monitor.list, monitor.obj.index))]
    par(mfrow = c(1,length(monitor.list)), oma = c(0,0,2.5,0))
    
    for (i in 1:length(monitor.list)) {
        m = monitor.list[[i]]
        xlab = colnames(m)[options$xaxis]
        if(i==1){
            par(mar = c(5, 4, 0, 0) + 0.1)
            ylab = monitor.id(m)
        } else if(i == length(monitor.list)){
            par(mar = c(5, 0, 0, 1) + 0.1)
            ylab=""
        } else{
            par(mar = c(5, 0, 0, 0) + 0.1)
            ylab=""
        }
        
        plot(
            x = m[, options$xaxis],
            y = m[, options$yaxis],
            type = 'p',
            col = i,
            pch = i,
            xlab = xlab,
            ylab = ylab,
            yaxt = "n",
            xaxt = "n",
            xlim = monitors.xlim[[monitor.id(monitor)]],
            ylim = monitors.ylim[[monitor.id(monitor)]],
            log=ylog,
            abline(
                h = monitors.yticks[[monitor.id(monitor)]],
                v = monitors.xticks[[monitor.id(monitor)]],
                col = "darkgray",
                lty = 3
            )
        )
        legend.text = monitor.obj(m)
        legend.col = i
        legend.pch = i
        if (!is.null(options$model)){
            error = monitor.plot.fit(m, options$model, pch=i+1, col=i+1) 
            legend.text = c(legend.text, sprintf("%s prediction (rrmse=%f)", options$model, error))
            legend.col = c(legend.col, i+1)
            legend.pch = c(legend.pch, i+1)
        }
        legend("bottomright", legend = legend.text, bg="white", pch=legend.pch, col=legend.col)
        axis(1, at = monitors.xticks[[monitor.id(monitor)]])
        if(i==1){
            axis(2, at = monitors.yticks[[monitor.id(monitor)]])
        }
    }
    title(main = get_title(NULL), outer=T)
}

monitor.plot <- function(monitor) {
    monitor = monitor.check(monitor)
    if(options$yaxis > ncol(monitor)){
        print(sprintf("yaxis option set to %d but monitor %s has only %d columns",options$yaxis,monitor.id(monitor),ncol(monitor)))
        return()
    }
    if(options$xaxis > ncol(monitor)){
        print(sprintf("yaxis option set to %d but monitor %s has only %d columns",options$xaxis,monitor.id(monitor),ncol(monitor)))
        return()
    }
    monitor.set.limits(monitor)
    if (options$split)
        monitor.plot.split(monitor)
    if (!options$split)
        monitor.plot.merge(monitor)
}

#################################
### Plot all monitors in pdf file
#################################
monitors.plot.pdf <- function(monitors) {
    pdf(
        options$output,
        family = "Helvetica",
        width = 10,
        height = 5,
        title = get_title(options$file)
    )
    sapply(monitors, monitor.plot)
    graphics.off()
}


#########################################################################################################
###                                            X11 handlers                                             #
#########################################################################################################

monitors.connexion = NULL
monitors.devs = new.env(hash = T)
monitors.graphicEnv = NULL

x11.devset <- function()
    if (dev.cur() != monitors.graphicEnv$which) dev.set(monitors.graphicEnv$which)

x11.keyboardHandler <- function(key) {
    if (key == "q" || key == "\033") {
        for(key in ls(monitors.devs)){
            dev.off(which = monitors.devs[[key]]) 
        }
    }
    NULL
}

##############################################################
### get screen dimensions as a list c(width, height) in inches
##############################################################
screen.dim <- function(){
    ##get screen size
    scrn = system("xrandr  | fgrep '*'", wait = FALSE, intern = TRUE)
    sc.dim = as.numeric(unlist(regmatches(scrn, regexec(
                                                    "(\\d+)x(\\d+)", scrn
                                                )))[-1])
    res = system("xdpyinfo  | grep 'resolution:'",
                 wait = FALSE,
                 intern = TRUE)
    dpi = as.numeric(unlist(regmatches(res, regexec(
                                                "(\\d+)x(\\d+)",  res
                                            )))[-1])
    c(sc.dim[1] / dpi[1], sc.dim[2] / dpi[2])
}

monitor.create.x11 <- function(monitor) {
    dim = screen.dim()
    dev = x11(type = "Xlib", width = dim[1], height=dim[2])
    ##set event handlers for this device
    setGraphicsEventHandlers(onKeybd = x11.keyboardHandler)
    monitors.graphicEnv <<- getGraphicsEventEnv()
    dev.cur()
}

monitor.plot.x11 <- function(monitor) {
    id = as.character(monitor.id(monitor))
    if (is.null(monitors.devs[[id]])) {
        monitors.devs[[id]] = monitor.create.x11(monitor)
    } else {
        dev.set(monitors.devs[[id]])
    }
    monitor.plot(monitor)
}

monitors.plot.x11 <- function(monitors) sapply(monitors, monitor.plot.x11)

#########################################################################################################
###                                        Streaming monitors                                           #
#########################################################################################################

################################
### Stream monitor trace.
### Can be called several times.
################################
monitors.stream = function(n = options$window) {
    ##Open a fifo if the trace is to be read by parts
    if (is.null(monitors.connexion)) {
        monitors.connexion <<- fifo(options$input, open = "r")
    }
    ##read by part
    lines = readLines(monitors.connexion, n = n)
    for (line in lines) {
        if(is.null(monitors.frame)){
            frame_line = read.table(textConnection(line), flush = T)  
        } else {
            frame_line = read.table(textConnection(line), flush = T,col.names = colnames(monitors.frame))
        }
        if (is.null(options$filter) || grepl(options$filter, frame_line[1, 1], ignore.case = TRUE)) {
            if(is.null(monitors.frame)){
                monitors.frame <<- frame_line
            } else{
                monitors.frame <<- rbind(monitors.frame, frame_line)
            }
        }
    }
    ##check the frame is not too large after expension
    if(nrow(monitors.frame)>n*10){
        start = nrow(monitors.frame)-n*9
        monitors.frame <<- monitors.frame[start:nrow(monitors.frame), ]
    }
    monitors.frame <<- monitors.set.colnames(monitors.frame)
    monitors.list(monitors.frame)
}

#########################################################################################################

script.run <- function() {
    ##Check input is provided
    if (is.null(options$input))
        stop("Input option is mandatory")
    
    if (options$window > 0) {
        repeat {
            monitors.plot.x11(monitors.stream())
            Sys.sleep(options$update)
        }
    } else {
        monitors = monitors.read()
        if (is.null(options$output)) {
            monitors.plot.x11(monitors)
            print("Press [enter] to close windows")
            readLines(con = "stdin", n = 1)
        } else {
            monitors.plot.pdf(monitors)
        }
    }
}

script.run()

test.run <- function(){
    options$input <<- "~/Documents/hmon/tests/hpccg/blob.out"
    options$model <<- "nnet:train"
    monitors <<- monitors.read()
    monitors.plot.x11(monitors)

### options$input <<- "../tests/hpccg/lulesh.out"
### options$output <<- "./test.pdf"
### options$filter <<- "write"
### options$log <<- T
### options$split <<- T
### options$cluster <<- T
### options$title <<- "test_title"
### options$xaxis <<- 3
### options$yaxis <<- 7
### options$window <<- 1000
### options$update <<- 0.5
### monitors <<- monitors.stream()
### monitor <<- monitors[[1]]
### sapply(monitors, monitor.plot)
### monitor.create.x11(monitor)
### monitors.plot.pdf(monitors)
### monitor.plot.split(monitor)
### monitor.plot.merge(monitor)
}

