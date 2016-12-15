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

yOpt = make_option(
    opt_str = c("-y", "--yaxis"),
    type = "integer",
    default = 3,
    help = "Use column yaxis instead of column 3 as Y values to plot"
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
    default = 2,
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
  If (--model = \"linear:2\"), then use a linear model with 2 timesteps for each sample of the training set.
  If (--model = \"nnet\"), same as linear but use a neural network instead of linear regression model. 
  If this monitor neural network already exists then load the one existing and do not train the network.
  If (--model = \"nnet:train\"), then fitting a monitor will update its existing neural network or create a new one to train.
  If (--model = \"nnet:2\"), train a neural network using 2 timesteps for each sample of the training set.
  If (--model = \"rsnns\"), fit with a recurrent neural network.
  If (--model = \"rsnns:train\"), train (not update) a new network with new input.
  If (--model = \"periodic\"), then fit y column with a fourier serie of time column.
  If (--model = \"gaussian\"), then fit y column as a normal distribution, and output y*P(y) on cross validation set."
)

dynOpt = make_option(
    opt_str = c("-d", "--dynamic"),
    type = "logical",
    default = FALSE,
    action = "store_true",    
    help = "Plot monitor by part dynamically. --window points are shown each step"
)

winOpt = make_option(
    opt_str = c("-w", "--window"),
    type = "integer",
    default = 10000,
    help = "number of points to plot. Used with -d option and -v option"
)

fastOpt = make_option(
    opt_str = c("-f", "--fast"),
    type = "logical",
    default = FALSE,
    action = "store_true",
    help = "Option to enable fast reading of large trace.
            If set: Do not filter NAs and inf values,
                    Use faster library for reading trace,
                    Plot a subset of each monitor containing --window random points."
)

logOpt = make_option(
    opt_str = c("-l", "--log"),
    type = "logical",
    default = FALSE,
    action = "store_true",
    help = "Plot yaxis in logscale"
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
                            fastOpt,
                            xOpt,
                            yOpt,
                            logOpt,
                            splitOpt,
                            clusterOpt,
                            fitOpt,
                            winOpt,
                            dynOpt,
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
        monitor.id()
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
    X = monitor[, setdiff(3:ncol(monitor), options$yaxis)]
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
    if(ncol(monitor)<=3){
        print("linear plot cannot be applied on a monitor with a single event")
        return(NULL)
    }
    
    if(is.null(save) || !file.exists(save)){
        fit = monitor.fit.prepare(monitor, shuffle=T, test.ratio=0.1, past=recurse)
        if(ncol(fit$X)==1){
            fit.lm = lm(fit$y[fit$train.set] ~ fit$X[fit$train.set])  
        } else {
            fit.lm = lm(fit$y[fit$train.set] ~ .^2, data = fit$X[fit$train.set,])  
        }
        if(!is.null(save) && !file.exists(save)){
            save(list=c("fit.lm","fit"), file = save)
        }
        return (list(fit$test.set, monitor.fit.denormalize(fit,predict(fit.lm, newdata = fit$X[fit$test.set,]))))
        
    } else if(!is.null(save) && file.exists(save)){
        load(file = save)
        fit = monitor.fit.prepare(monitor, fit=fit)
        pred.y = predict(fit.lm, newdata = fit$X[fit$test.set,])
        return (list(fit$test.set, monitor.fit.denormalize(fit, pred.y)))
    }
}

##############################################################################
### Compute non-linear model of monitor based on an artificial neural network.
### Output cross validation set and prediction
##############################################################################
monitor.nnet.fit <- function(monitor, save = NULL, recurse = 0, train=FALSE){
    ##check there are features and target
    if(ncol(monitor)<=3){
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
rrmse <- function(y,fit.y,mean) {
    sum.sq.error = sum((y-fit.y)^2)
    sum.std.error = sum((y-mean)^2)
    sqrt(sum.sq.error/sum.std.error)
}


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
        
        if (grepl("linear", substr(type, start=0, stop=nchar("linear")))){
            if(type == "linear"){
                fit = monitor.linear.fit(monitor, save = sprintf("%s_%s_linear.rda", monitor[1,1], monitor[1,2]))
            } else {
                recurse = as.integer(substr(type, start=nchar("linear:")+1, stop = nchar(type)))
                fit = monitor.linear.fit(monitor, save = sprintf("%s_%s_linear.rda", monitor[1,1], monitor[1,2]), recurse = recurse)
            }
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
            x = monitor[fit[[1]],options$xaxis]
            y = monitor[fit[[1]],options$yaxis]
            mean = mean(monitor[,options$yaxis])
            points(x, fit[[2]], pch = pch, col = col)
            return(rrmse(y, fit[[2]], mean))
        }
    }

#########################################################################################################
###                                          monitor partitionning                                     #
#########################################################################################################

###############################################################
### Split a monitor in several monitor using dbscan clustering
###############################################################
monitor.cluster <- function(monitor){
    library("dbscan")
    if(nrow(monitor)<=10){return(list(monitor))}
    p = monitor[, 3:ncol(monitor)]
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

monitor.frame = NULL
xlim = NULL
xticks = NULL
ylim = NULL
yticks = NULL
id.time = 2
id.obj = 1
ylog = ""; if(options$log){ylog = "y"}

###########################
### Get a monitor obj index
###########################
monitor.obj.index <- function(monitor) 
    as.integer(strsplit(monitor[1,id.obj], ":", fixed=T)[[1]][2])

####################
### Get a monitor id
####################
monitor.id <- function(){
    tail(unlist(strsplit(options$input,"/", fixed=T)), n=1)
}

#####################################################################
### Filter NA cols then inf, NaN, NA lines, and return filtered frame
#####################################################################
monitor.check <- function(frame){
    cols.del = c()
    rows.del = c()
    ##Remove columns with only NA
    for (i in 3:ncol(frame)) {
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
            for (j in 3:ncol(frame)) {
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
monitor.set.colnames <-function(frame){
    names = colnames(frame)
    names[1] = "id"
    names[2] = "hwloc_obj"
    names[2] = "nanoseconds"
    colnames(frame) = names
    frame
}

##############################
### Acquire monitor from input
##############################
monitor.read <- function(){
    if(options$fast){
        library("data.table", verbose=FALSE, quietly=TRUE)
        library("bit64", verbose=FALSE, quietly=TRUE)
        monitor = fread(options$input, header=FALSE, integer64="numeric", blank.lines.skip=TRUE, showProgress=TRUE, data.table=TRUE)
    } else {
        monitor = read.table(options$input, header=FALSE, stringsAsFactors=FALSE)        
    }
    monitor.set.colnames(monitor)
}

##################################################
### Save in environment a monitor limits and ticks
##################################################
monitor.set.limits <- function(monitor){
    x = monitor[,options$xaxis]
    y = monitor[,options$yaxis]
    xmin = min(x, na.rm=T)
    xmax = max(x,na.rm=T)
    mean = mean(y)
    qtl = quantile(y, na.rm = T, probs = c(0.01, 0.99))
    ymin = qtl[1]
    ymax = qtl[2]
    xlim <<- c(xmin, xmax)
    ylim <<- c(ymin, ymax)
    xticks <<- seq(from = xmin, to = xmax, by = (xmax-xmin)/10)
    yticks <<- seq(from = ymin, to = ymax, by = (ymax-ymin)/10)
}

#####################################################################
## Restrict the monitor set to plot in case fast option is turned on.
## Else return the monitor set.
#####################################################################
monitor.restrict.set <- function(monitor){
    if(!options$fast){
        return(1:nrow(monitor))
    } else {
        subset = sample(x=1:nrow(monitor), size = options$window)
        return(subset[order(subset)])
    }
}

#########################################################################################################
###                                              monitor ploting                                        #
#########################################################################################################

#############################
### Plot a unsplitted monitor
#############################
monitor.plot.merge <- function(monitor) {
    errors = c()
    id = monitor.id()
    monitor.list = monitor.split(monitor)
    for (i in 1:length(monitor.list)) {
        m = monitor.list[[i]]
        set = monitor.restrict.set(m)
        plot(
            x = m[set, options$xaxis],
            y = m[set, options$yaxis],
            log = ylog,      
            type = 'p',
            col = i,
            xlim = xlim,
            ylim = ylim,
            axes = FALSE,
            ann = FALSE,
            pch = i,
            panel.first = abline(
                h = yticks,
                v = xticks,
                col = "darkgray",
                lty = 2
            )
        )
        if (!is.null(options$model))
            errors = c(errors ,monitor.plot.fit(m, type=options$model, pch=i+1, col=i+1))
        if (i<length(monitor.list))
            par(new = TRUE)
    }
    title(
        main = get_title(NULL),
        ylab = colnames(monitor)[options$yaxis],
        xlab = colnames(monitor)[options$xaxis]
    )
    axis(1, at = xticks)
    axis(2, at = yticks)
    sequence = 1:length(monitor.list)
    legend.text = sapply(sequence, function(i) monitor.list[[i]][1,id.obj], simplify = "array")
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
        set = monitor.restrict.set(m)
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
            x = m[set, options$xaxis],
            y = m[set, options$yaxis],
            type = 'p',
            col = i,
            pch = i,
            xlab = xlab,
            ylab = ylab,
            yaxt = "n",
            xaxt = "n",
            xlim = xlim,
            ylim = ylim,
            log=ylog,
            abline(
                h = yticks,
                v = xticks,
                col = "darkgray",
                lty = 2
            )
        )
        legend.text = m[1,id.obj]
        legend.col = i
        legend.pch = i
        if (!is.null(options$model)){
            error = monitor.plot.fit(m, options$model, pch=i+1, col=i+1) 
            legend.text = c(legend.text, sprintf("%s prediction (rrmse=%f)", options$model, error))
            legend.col = c(legend.col, i+1)
            legend.pch = c(legend.pch, i+1)
        }
        legend("bottomright", legend = legend.text, bg="white", pch=legend.pch, col=legend.col)
        axis(1, at = xticks)
        if(i==1){axis(2, at = yticks)}
    }
    title(main = get_title(NULL), outer=T)
}

monitor.plot <- function(monitor) {
    if(!options$fast){monitor = monitor.check(monitor)}
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

############################
### Plot monitor in pdf file
############################
monitor.plot.pdf <- function(monitor){
    pdf(
        options$output,
        family = "Helvetica",
        width = 10,
        height = 5,
        title = get_title(options$file)
    )
    monitor.plot(monitor)
    graphics.off()
}


#########################################################################################################
###                                            X11 handlers                                             #
#########################################################################################################

monitor.connexion = NULL
monitor.x11 = NULL

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

monitor.plot.x11 <- function(monitor) {
    if(is.null(monitor.x11)){
        dim = screen.dim()
        x11(type = "Xlib", width = dim[1], height=dim[2])
        monitor.x11 <<- dev.cur()
    } else {
        dev.set(monitor.x11)
    }
    monitor.plot(monitor)
}

#########################################################################################################
###                                        Streaming monitor                                            #
#########################################################################################################

################################
### Stream monitor trace.
### Can be called several times.
################################
monitor.stream = function(n = options$window) {
    ##Open a fifo if the trace is to be read by parts
    if (is.null(monitor.connexion)) {
        monitor.connexion <<- fifo(options$input, open = "r")
    }
    ##read by part
    lines = readLines(monitor.connexion, n = n)
    for (line in lines) {
        if(is.null(monitor.frame)){
            frame_line = read.table(textConnection(line), flush = T)  
        } else {
            frame_line = read.table(textConnection(line), flush = T,col.names = colnames(monitor.frame))
        }
        if(is.null(monitor.frame)){
            monitor.frame <<- frame_line
        } else{
            monitor.frame <<- rbind(monitor.frame, frame_line)
        }
    }
    ##check the frame is not too large after expension
    if(nrow(monitor.frame)>n*10){
        start = nrow(monitor.frame)-n*9
        monitor.frame <<- monitor.frame[start:nrow(monitor.frame), ]
    }
    monitor.frame <<- monitor.set.colnames(monitor.frame)
    monitor.frame
}

#########################################################################################################

script.run <- function() {
    ##Check input is provided
    if (is.null(options$input))
        stop("Input option is mandatory")
    
    if (options$dynamic) {
        repeat {
            monitor.plot.x11(monitor.stream())
            Sys.sleep(options$update)
        }
    } else {
        monitor = monitor.read()
        if (is.null(options$output)) {
            monitor.plot.x11(monitor)
            print("Press [enter] to close windows")
            readLines(con = "stdin", n = 1)
        } else {
            monitor.plot.pdf(monitor)
        }
    }
}

script.run()

test.run <- function(){
    options$input <<- "~/Documents/hmon/tests/output/cpu_load" #hmon/tests/hpccg/blob.out
### options$fast <<- TRUE
### options$model <<- "nnet:train"
### monitor <<- monitor.read()
### options$log <<- T
### options$split <<- T
### options$cluster <<- T
### options$title <<- "test_title"
### options$xaxis <<- 2
### options$yaxis <<- 7
  options$window <<- 100
  options$update <<- 0.5
  repeat {
    monitor.plot.x11(monitor.stream())
    Sys.sleep(options$update)
  }
### monitor.plot.pdf(monitor)
### monitor.plot.split(monitor)
### monitor.plot.merge(monitor)
}

