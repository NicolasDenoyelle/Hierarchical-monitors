library("optparse")

#########################################################################################################
#                                              Handle Options                                           #
#########################################################################################################

#Parse options
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

grepOpt = make_option(
  opt_str = c("-g", "--grep"),
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

fitOpt = make_option(
  opt_str = c("-m", "--model"),
  type = "character",
  default = NULL,
  help = "fit y data according to model argument.
  If (--model = \"linear\"), then use a linear model of events (except y) to fit y,
  and output absolute error of cross validation points.
  If (--model = \"periodic\"), then fit y columns with a fourier serie of time column
  If (--model = \"gaussian\"), then apply kmeans clustering on xaxis and events (except y)"
  )

winOpt = make_option(
  opt_str = c("-w", "--window"),
  type = "integer",
  default = 0,
  help = "number of points to plot. If > 0, then plot by part dynamicaly"
)

freqOpt = make_option(
  opt_str = c("-f", "--frequency"),
  type = "numeric",
  default = 0.5,
  help = "frequency of read from trace file and plot in seconds. Plot should be displayed as parts of a large or updated trace.
  In this case -w points will be plot and window will move by -w/4 steps ahead every -f seconds"
)

option_list = c(
  inOpt,
  outOpt,
  titleOpt,
  grepOpt,
  xOpt,
  yOpt,
  splitOpt,
  fitOpt,
  winOpt,
  freqOpt
)

opt_parser = OptionParser(option_list = option_list)
options = parse_args(
  opt_parser,
  args = commandArgs(trailingOnly = TRUE),
  print_help_and_exit = TRUE,
  positional_arguments = FALSE
)

#Check input is provided
# if (is.null(options$input))
#   stop("Input option is mandatory")

##
#Provide a title according to options
##
get_title <- function(str) {
  if (!is.null(options$title)) {
    options$title
  } else {
    str
  }
}


#########################################################################################################
#                                             monitor modeling                                          #
#########################################################################################################

# Function that returns Root Mean Squared Error
rmse <- function(error) sapply(error, function(x) sqrt(mean(x^2)))

##
# Compute linear model of monitor output given monitor other events.
# Output cross validation set Root Mean Squared Error
##
monitor.linear.fit <- function(monitor){
  if(ncol(monitor<=4)){
    print("linear plot cannot be applied on a monitor with a single event")
    return()
  }
  fit.range = sample(1:nrow(monitor), round(0.5 * nrow(monitor)))
  fit.range = fit.range[order(fit.range)]
  fit.x = monitor[fit.range, setdiff(4:ncol(monitor), options$yaxis)]
  fit.y = monitor[fit.range, options$yaxis]
  if(ncol(monitor==5)){
    fit.lm = lm(fit.y ~ fit.x)  
  } else {
    fit.lm = lm(fit.y ~ ., data = fit.x)  
  }
  
  pred.lm = predict(fit.lm, newdata = monitor[-fit.range, setdiff(4:ncol(monitor), options$yaxis)])
  pred.y  = monitor[-fit.range, options$yaxis]
  pred.x = monitor[-fit.range, options$xaxis]
  likelihood = rmse(pred.lm - pred.y)
  list(pred.x, likelihood)
}

# Credit to http://www.di.fc.ul.pt/~jpn/r/fourier/fourier.html 
# returns the x.n time series for a given time sequence (ts) and
# a vector with the amount of frequencies k in the signal (X.k)
get.trajectory <- function(X.k,ts,acq.freq) {
  N   <- length(ts)
  i   <- complex(real = 0, imaginary = 1)
  x.n <- rep(0,N)           # create vector to keep the trajectory
  ks  <- 0:(length(X.k)-1)
  for(n in 0:(N-1)) {       # compute each time point x_n based on freqs X.k
    x.n[n+1] <- sum(X.k * exp(i*2*pi*ks*n/N)) / N
  }
  x.n * acq.freq 
}


##
# Compute fourier transform of monitor output given monitor other events.
# Event must be uniform accross time.
# Output cross validation set Root Mean Squared Error
##
monitor.frequency.fit <- function(monitor){
  fit.range = 1:(nrow(monitor)/4)
  fit.y = monitor[fit.range, options$yaxis]
  acq.freq = 10^-6*(monitor[nrow(monitor),id.time] - monitor[1,id.time])/nrow(monitor)
  X.k = fft(fit.y)
  fit.pred = get.trajectory(X.k, monitor[-fit.range,id.time], acq.freq)
  likelihood = rmse(fit.pred - monitor[-fit.range, options$yaxis])
  #acq.freq*fft(X.k, inverse = TRUE)/length(X.k)
  list(monitor[-fit.range, id.time], likelihood)
}


##
# Plot monitor fit
##
monitor.plot.fit <-
  function(monitor,
           type = "linear",
           n = 6,
           pch = 1,
           col = 1) {
    if (type == "linear") {
      fit = monitor.linear.fit(monitor)
      points(fit[[1]], fit[[2]], pch = pch, col = col)
    } else if(type == "periodic"){
      fit = monitor.frequency.fit(monitor)
      points(fit[[1]], fit[[2]], pch = pch, col = col)
    } else if (type == "gaussian" && nrow(monitor)>=2*n){
      p = monitor[, 4:ncol(monitor)]
      p = scale(p, center = TRUE, scale = TRUE)
      model = kmeans(x = p, centers = n)
      for (i in 1:n) {
        m = monitor[model$cluster == i,]
        points(
          x = m[, options$xaxis],
          y = m[, options$yaxis],
          col = i,
          pch = pch
        )
      }
    }
    
    #neural network fit
    # library(neuralnet)
    # scaleX=scale(X.fit)
    # X.fit.scale=as.data.frame(scaleX)
    # Y.fit.scale=scale(Y.fit)
    # X.pred.scale=(X.pred - attr(scaleX, "scaled:center")) / attr(scaleX, "scaled:scale")
    # n <- names(X.fit.scale)
    # f <- as.formula(paste("Y.fit.scale ~", paste(n, collapse="+")))
    # nn <- neuralnet(f, data=X.fit.scale, hidden=c(2, 1), linear.output=T)
    # nn.pred=compute(nn, X.pred.scale)
    # Y.pred.scale=nn.pred$net.result
    # nn.pred=Y.pred.scale * attr(Y.fit.scale, "scaled:scale") + attr(Y.fit.scale, "scaled:center")
  }

#########################################################################################################
#                                              plot monitor                                             #
#########################################################################################################


##
# Split a monitor into several object (column 2)
##
monitor.split <- function(monitor) {
  obj.list = unique(monitor[, id.obj])
  split.set = list()
  for (i in 1:length(obj.list)){
    split.set[[i]] = subset(monitor, monitor[, id.obj] == obj.list[i])
  }
  split.set
}

##
# Get a monitor mapping object
##
monitor.obj <- function(monitor)
  as.character(monitor[1, id.obj])

##
# Get a monitor id
##
monitor.id <- function(monitor) 
  monitor[1, id.id]

##
# Plot a unsplitted monitor
##
monitor.plot.merge <- function(monitor) {
  xmin = min(monitor[, options$xaxis])
  xmax = max(monitor[, options$xaxis])
  xlim = c(xmin, xmax)
  xticks=seq(xlim[1], xlim[2], (xlim[2] - xlim[1]) / 10)
  ymin = min(monitor[, options$yaxis])
  ymax = max(monitor[, options$yaxis])
  ylim = c(ymin, ymax)
  yticks=seq(from=ylim[1], to=ylim[2], by=(ylim[2] - ylim[1]) / 10)
  
  monitor.list = monitor.split(monitor)
  for (i in 1:length(monitor.list)) {
    m = monitor.list[[i]]
    plot(
      x = m[, options$xaxis],
      y = m[, options$yaxis],
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
        lty = 3
      )
    )
    if (length(monitor.list) > 1)
      par(new = TRUE)
  }
  title(
    main = get_title(monitor.id(m)),
    ylab = monitor.id(m),
    xlab = colnames(monitor)[options$xaxis]
  )
  axis(1, at = seq(xlim[1], xlim[2], (xlim[2] - xlim[1]) / 10))
  axis(2, at = seq(
    from = ylim[1],
    to = ylim[2],
    by = (ylim[2] - ylim[1]) / 10
  ))
  sequence = 1:length(monitor.list)
  legend.text = sapply(sequence, function(i) monitor.obj(monitor.list[[i]]), simplify = "array")
  legend.col = sequence
  legend.pch = sequence
  if (!is.null(options$fit)){
    monitor.plot.fit(monitor, type=options$fit, pch=length(monitor.list)+1, col=length(monitor.list)+1)
    legend.text = c(legend.text, sprintf("%s fit likelihood",options$fit))
    legend.col = c(legend.col, length(monitor.list)+1)
    legend.pch = c(legend.pch, length(monitor.list)+1)
  }
  legend(
    "bottomright",
    legend = legend.text,
    col = legend.col,
    pch = legend.pch,
    bg = "white"
  )
}


##
# split plot window
##
plot.split <- function(n) {
  if (n <= 4) {
    split = c(n, 1)
  } else {
    split = c(4, 1 + n / 4)
  }
  par(mfrow = split)
}

##
# Split a monitor into parts and plot these parts in a split device
##
monitor.plot.split <- function(monitor) {
  monitor.list = monitor.split(monitor)
  plot.split(length(monitor.list))
  for (i in 1:length(monitor.list)) {
    m = monitor.list[[i]]
    plot(
      x = m[, options$xaxis],
      y = m[, options$yaxis],
      type = 'p',
      col = i,
      pch = i,
      main = get_title(monitor.id(monitor)),
      xlab = colnames(m)[options$xaxis],
      ylab = monitor.id(monitor)
    )
    legend.text = monitor.obj(m)
    legend.col = i
    legend.pch = i
    if (!is.null(options$fit)){
      monitor.plot.fit(m, options$fit, pch=i+1, col=i+1) 
      legend.text = c(legend.text, sprintf("%s fit likelihood",options$fit))
      legend.col = c(legend.col, i+1)
      legend.pch = c(legend.pch, i+1)
    }
    legend("bottomright", legend = legend.text, bg="white", pch=legend.pch, col=legend.col)
  }
}


monitor.plot <- function(monitor) {
    if (options$split)
      monitor.plot.split(monitor)
    if (!options$split)
      monitor.plot.merge(monitor)
}
#########################################################################################################
#                                          Static monitors handling                                     #
#########################################################################################################

monitors.frame = NULL
id.time = 3
id.obj = 2
id.id  = 1


##
# list monitors
##
monitors.list <- function(){
  ids = unique(monitors.frame[, id.id])
  list = list()
  for(i in 1:length(ids)){
    list[[i]] = subset(monitors.frame, monitors.frame[, id.id] == id[i])
  }
  list
}

monitors.set.colnames <-function(){
  names = colnames(monitors.frame)
  names[1] = "id"
  names[2] = "hwloc_obj"
  names[3] = "nanoseconds"
  colnames(monitors.frame) <<- names
}

monitors.read <- function(){
  monitors.frame <<-
    read.table(options$input, stringsAsFactors=F)
  monitors.set.colnames()
  if (!is.null(options$grep)) {
    monitors.frame <<-
      monitors.frame[grep(options$grep, monitors.frame[, id.id], ignore.case = TRUE),]
  }
  monitors.list()
}


##
# Plot all monitors in pdf file
##
monitors.plot.pdf <- function(monitors) {
  pdf(
    options$output,
    family = "Helvetica",
    width = 10,
    height = 5,
    title = get_title(options$file)
  )
  for(i in 1: length(monitors)){
    monitor.plot(monitors[[i]]) 
  }
  graphics.off()
}


#########################################################################################################
#                                              X11 handlers                                             #
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

# screen.dim <- function(){
#   #get screen size
#   scrn = system("xrandr  | fgrep '*'", wait = FALSE, intern = TRUE)
#   sc.dim = as.numeric(unlist(regmatches(scrn, regexec(
#     "(\\d+)x(\\d+)", scrn
#   )))[-1])
#   res = system("xdpyinfo  | grep 'resolution:'",
#                wait = FALSE,
#                intern = TRUE)
#   dpi = as.numeric(unlist(regmatches(res, regexec(
#     "(\\d+)x(\\d+)",  res
#   )))[-1])
#   c(sc.dim[1] / dpi[1], sc.dim[2] / dpi[2])
# }

monitor.create.x11 <- function(monitor) {
  dev = x11(type = "Xlib")
  #set event handlers for this device
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

monitors.plot.x11 <- function(monitors) {
  for(i in 1: length(monitors)){
    monitor.plot.x11(monitors[[i]]) 
  }
}

#########################################################################################################
#                                          Streaming monitors                                           #
#########################################################################################################

##
# Stream monitor trace.
# Can be called several times.
##
monitors.stream = function() {
  #Open a fifo if the trace is to be read by parts
  if (is.null(monitors.connexion)) {
    monitors.connexion <<- fifo(options$input, open = "r")
  }
  #read by part
  lines = readLines(monitors.connexion, n = options$window)
  for (line in lines) {
    if(is.null(monitors.frame)){
      frame_line = read.table(textConnection(line), flush = T)  
    } else {
      frame_line = read.table(textConnection(line), flush = T,col.names = colnames(monitors.frame))
    }
    if (is.null(options$grep) || grep(options$grep, frame_line[1, 1], ignore.case = TRUE)) {
      if(is.null(monitors.frame)){
        monitors.frame <<- frame_line
      } else{
        monitors.frame <<- rbind(monitors.frame, frame_line)
      }
    }
  }
  if(nrow(monitors.frame) > options$window){
    monitors.frame <<-
      monitors.frame[(nrow(monitors.frame) - options$window):nrow(monitors.frame), ]
  }
  monitors.set.colnames()
  monitors.list()
}

#########################################################################################################

  # setwd(dir = "~/Documents/hmon/utils/")
  # options$input="./hpccg/hpccg.out"
  # options$output="./test.pdf"
  # options$split=TRUE
  # options$title="test_title"
  # options$xaxis=3
  # options$yaxis=4
  # options$fit="gaussian"
  # options$window=100
  # options$frequency=0.5
  
  #monitors = monitors.read()
  #monitor = monitors[[1]]
  #monitor.create.x11(monitor)
  #monitors.plot.x11()
  #monitor.plot.split(monitor)
  #monitor.plot.merge(monitor)
  #monitor = monitors.stream()[[2]]
  #monitor.plot.x11(monitor)


if(options$window>0){
  options(timeout=options$frequency)
  repeat {
    monitors.plot.x11(monitors.stream())
    Sys.sleep(options$frequency)
  }
} else {
  monitors = monitors.read()
  if(is.null(options$output)){
    monitors.plot.x11(monitors)
    print("Press [enter] to close windows")
    readLines(con="stdin", n= 1)
  } else {
    monitors.plot.pdf(monitors)
  }
}

