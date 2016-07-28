library("optparse")

#Parse options
inOpt = make_option(opt_str = c("-i", "--input"), type = "character", default = NULL, help = "Data set input file")
outOpt = make_option(opt_str = c("-o", "--output"), type = "character", default = NULL, help = "Output pdf file (static)")
titleOpt = make_option(opt_str = c("-t", "--title"), type = "character", default = NULL, help = "Plot title")
filterOpt = make_option(opt_str = c("-f", "--filter"), type = "character",default = NULL, help = "Filter input monitor")
logOpt = make_option(opt_str = c("-l", "--log"), type = "logical", default = FALSE, action = "store_true", help = "logscale graph")
colOpt = make_option(opt_str = c("-y", "--column"), type = "integer", default = 4, help = "Use column --column instead of column 4 as Y values to plot")
splitOpt = make_option(opt_str = c("-s", "--split"), type = "logical", default = FALSE,action = "store_true", help = "split one plot per monitor")
clusterOpt = make_option(opt_str = c("-c", "--cluster"), type = "integer", default = 1, help = "If split option is enabled, compute two clusters and colorize plot with a color per cluster")
histOpt = make_option(opt_str = c("-p", "--histogram"), type = "logical", default = FALSE,action = "store_true", help = "plot histogram of values distribution instead of values")
winOpt = make_option( opt_str = c("-w", "--window"), type = "integer", default = 1000, help = "number of points to plot (dynamic)")
updateOpt = make_option(opt_str = c("-u", "--update"), type = "numeric", default = 1, help = "frequency of read from trace file and plot in seconds. (dynamic)")
dynOpt = make_option(opt_str = c("-d", "--dynamic"), type = "logical", default = FALSE, action = "store_true", help = "Set if plot should be displayed as parts of a large trace or updated trace. In this case --window points will be plot and window will move by --window/4 steps ahead every --update seconds")
opt_parser = OptionParser(option_list = c(inOpt, outOpt, filterOpt, logOpt, splitOpt, winOpt, updateOpt, dynOpt, histOpt, colOpt, titleOpt, clusterOpt))

#Functions
lseq =function(from = 1, to = 100000, length.out = 6) {
    exp(seq(log(from), log(to), length.out = length.out))
}

read_monitors = function(frame, connexion) {
  lines = readLines(connexion, n = options$window/4)
  for (line in lines) {
    frame_line = read.table(textConnection(line), flush = T)
    if (is.null(options$filter) || frame_line[1, 1] == options$filter) {
      frame = rbind(frame, frame_line)
    }
  }
  if (options$dynamic && nrow(frame) > options$window) {
    frame = frame[(nrow(frame)-options$window):nrow(frame), ]
  }
  frame
}

get_title <- function(){
 if(!is.null(options$title)){options$title} else if(!is.null(options$restrict)){options$restrict} else {options$input}
}

plot_monitor_histogram = function(frame, ymin = NULL, ymax = NULL, title = NULL, col = 1){
  if(is.null(title)){
    title = get_title()   
  }
  if(is.null(ymin)||is.null(ymax)){
    ymax = max(frame[, options$column],  na.rm = TRUE)
    ymin = min(frame[, options$column],  na.rm = TRUE)
  }
  obj = frame[1,2];  monitor = frame[1,1];
  plot(hist(frame[, options$column], 100, main = t, xlab="monitor", xlim = c(ymin,ymax)), col = col)
}

plot_monitor = function(frame, ymin = NULL, ymax = NULL, xmin = NULL, xmax = NULL, ann = FALSE, logscale = FALSE, col = 1){
  if(is.null(ymin) || is.null(ymax)){
    ymax = max(frame[, options$column],  na.rm = TRUE)
    ymin = min(frame[, options$column],  na.rm = TRUE)
  }
  if(is.null(xmin)||is.null(xmax)){
    xmin = min(frame[, 3], na.rm = TRUE)
    xmax = max(frame[, 3], na.rm = TRUE)
  }
  xticks = seq(xmin, xmax, (xmax - xmin) / 10)
  if (logscale == FALSE) {
    yticks = seq(from = ymin, to = ymax, by = (ymax - ymin) / 10)
  } else {
    ymin = max(c(ymin,1))
    yticks = lseq(from = ymin, to = ymax, length.out = log10(ymax / ymin))
  }
  if(options$cluster>1 && options$split){
    points = scale(frame[,3:ncol(frame)], center=TRUE, scale=TRUE)
    km = kmeans(x = points, centers = options$cluster, iter.max = 20)
    col = km$cluster
  }

  obj = frame[1,2];  monitor = frame[1,1];
  plot(x = frame[,3],
       y = frame[,options$column],
       main = title,
       log = if(logscale){"y"} else {""},
       type = 'p',
       col = col,
       xlim = c(xmin, xmax),
       ylim = c(ymin, ymax),
       axes = FALSE,
       ann=FALSE,
       pch = col,
       panel.first = abline(h = yticks, v = xticks, col = "darkgray",lty = 3))
  if(ann){
     axis(1, at=xticks, labels=xticks)
     axis(2, at=yticks, labels=yticks)
     title(main = paste(frame[1,1], frame[1,2], sep=" "), xlab = "nanoseconds", ylab = frame[1,1])
  }
}

plot_monitors <- function(frame){
  par(ann=FALSE)
  if(!options$split){
    ymax = max(frame[, options$column],  na.rm = TRUE)
    ymin = min(frame[, options$column],  na.rm = TRUE)
    xmin = min(frame[, 3], na.rm = TRUE)
    xmax = max(frame[, 3], na.rm = TRUE)
    xticks = seq(xmin, xmax, (xmax - xmin) / 10)
    if (options$log == FALSE) {
      yticks = seq(from = ymin, to = ymax, by = (ymax - ymin) / 10)
    } else {
      ymin = max(c(ymin,1))
      yticks = lseq(from = ymin, to = ymax, length.out = log10(ymax / ymin))
    } 
    title = get_title()
    ann=FALSE
  } else {
    ymax = NULL
    ymin = NULL
    xmin = NULL
    xmax = NULL
    title = NULL
    ann=TRUE
  }
  monitors = unique(frame[, c(1,2)])
  monitors = monitors[order(monitors[,2]),]
  objs = monitors[,2]
  names = monitors[,1]
  for (i in 1:length(objs)) {
    data = subset(frame, frame[, 2] == objs[i])
    plot_monitor(data, xmin=xmin, xmax=xmax, ymin=ymin ,ymax=ymax, ann=ann, logscale = options$log, col = i)
    if(!options$split){
      par(new = TRUE)
    }
  }
  if(!options$split){
     legend("topleft", legend=paste(objs,names,sep=" "), cex=.7, col=1:length(objs), pch=1:length(objs))
     title(main=get_title(), ylab="monitors value", xlab="nanoseconds")
     axis(1, at=xticks)
     axis(2, at=yticks)
  }
}

#Script
options = parse_args(opt_parser, args = commandArgs(trailingOnly = TRUE), print_help_and_exit = TRUE, positional_arguments = FALSE)
#Ask input if not provided in options
if (is.null(options$input) || options$input == "") {
  options$input = readline(prompt = "Input file: ")
}
#Check output
if (!is.null(options$output) && options$dynamic) {
  stop(
    "Options --output and --dynamic cannot be set simultaneously. Whether you plot statically the whole file, or you plot updated file dynamically.",
    call. = FALSE
  )
}
if (is.null(options$output) && !options$dynamic) {
  options$output = paste(options$input, "pdf", sep = ".")
  print(sprintf("Output to %s", options$output))
}

if (!options$dynamic) {
  pdf(options$output, family = "Helvetica", width = 10, height = 5, title=options$file)
  frame = read.table(options$input)
  plot_monitors(frame);
  graphics.off()
} else {
  df = data.frame(name = character(0), obj = character(0), nano = numeric(0), val = numeric(0))
  stream = fifo(options$input, open = "r")
  x11(xpos = 0, ypos = 0)
  repeat {
    df = read_monitors(df, stream)
    if(options$split){
       par(mfrow = c(length(unique(df[,2])),1))
    }
    plot_monitors(df)
    Sys.sleep(options$update)
  }
}
