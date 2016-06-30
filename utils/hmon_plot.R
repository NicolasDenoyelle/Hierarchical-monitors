library("optparse")

#Globals
df = data.frame(name = character(0), obj = character(0), nano = numeric(0), val = numeric(0))

#Parse options
inOpt = make_option(opt_str = c("-i", "--input"), type = "character", default = NULL, help = "Data set input file")
outOpt = make_option(opt_str = c("-o", "--output"), type = "character", default = NULL, help = "Output pdf file (static)")
titleOpt = make_option(opt_str = c("-t", "--title"), type = "character", default = NULL, help = "Plot title")
filterOpt = make_option(opt_str = c("-f", "--filter"), type = "character",default = NULL, help = "Filter input monitor")
logOpt = make_option(opt_str = c("-l", "--log"), type = "logical", default = FALSE, action = "store_true", help = "logscale graph")
colOpt = make_option(opt_str = c("-y", "--column"), type = "integer", default = 4, help = "Use column --column instead of column 4 as Y values to plot")
histOpt = make_option(opt_str = c("-p", "--histogram"), type = "logical", default = FALSE,action = "store_true", help = "plot histogram of values distribution instead of values")
winOpt = make_option( opt_str = c("-w", "--window"), type = "integer", default = 1000, help = "number of points to plot (dynamic)")
updateOpt = make_option(opt_str = c("-u", "--update"), type = "numeric", default = 1, help = "frequency of read from trace file and plot in seconds. (dynamic)")
dynOpt = make_option(opt_str = c("-d", "--dynamic"), type = "logical", default = FALSE, action = "store_true", help = "Set if plot should be displayed as parts of a large trace or updated trace. In this case --window points will be plot and window will move by --window/4 steps ahead every --update seconds")
opt_parser = OptionParser(option_list = c(inOpt, outOpt, filterOpt, logOpt, winOpt, updateOpt, dynOpt, histOpt, colOpt))
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
plot_monitors = function(frame) {
  if(!is.null(options$title)){t = options$title} else if(!is.null(options$restrict)){t = options$restrict} else {t = options$input}
  ymax = max(frame[, options$column],  na.rm = TRUE)
  ymin = min(frame[, options$column],  na.rm = TRUE)
  if (!options$histogram) {
    xmin =  min(frame[, 3], na.rm = TRUE)
    xmax = max(frame[, 3], na.rm = TRUE)
    xticks = seq(xmin, xmax, (xmax - xmin) / 10)
    if (options$log == FALSE) {
      yticks = seq(from = ymin, to = ymax, by = (ymax - ymin) / 10)
      ylabels = yticks
    } else {
      ymin = max(c(ymin,1))
      yticks = lseq(from = ymin, to = ymax, length.out = log10(ymax / ymin))
      ylabels = sapply(yticks, function(i) as.expression(bquote(10 ^ .(round(log10(i))))))
    }
  }
  objs = unique(frame[, 2])
  for (i in 1:length(objs)) {
    obj = as.character(objs[i])
    data = subset(frame, frame[, 2] == objs[i])
    if (options$histogram) {
      if(i==1){
        plot(hist(data[, options$column], 100, main = paste(t, obj, sep=":"), xlab="", xlim = c(ymin,ymax)), col = i)
      } else {
        plot(hist(data[, options$column], 100, main = paste(t, obj, sep=":"), xlab="", xlim = c(ymin,ymax)), col = i, add=T)
      }
    } else{

        plot(
          x = data[,3],
          y = data[,options$column],
	  main = t,
          log = if(options$log){"y"} else {""},
          type = 'p',
          col = i,
          xlab = "nanoseconds",
          ylab = data[1,1],
          xlim = c(xmin, xmax),
          ylim = c(ymin, ymax),
          lty = 1,
          panel.first = abline(h = yticks, v = xticks, col = "darkgray",lty = 3)
        )
        if(options$dynamic){
          legend("topright", legend = objs, col = 1:length(objs), pch = 1)
        } else {
          legend("topright", legend = c(obj), col = c(i))
        }
    }
    if(options$dynamic){
      if(i!=length(objs)){par(new=TRUE, ann=FALSE, xaxt="n", yaxt="n")}
      else{par(xaxt="s", yaxt="s", ann=TRUE)}
    }
  }
}

#Script
if (!options$dynamic) {
  pdf(options$output, family = "Helvetica", width = 10, height = 5, title=options$file)
  plot_monitors(read.table(options$input))
  graphics.off()
} else {
  stream = fifo(options$input, open = "r")
  x11(xpos = 0, ypos = 0)
  repeat {
    df = read_monitors(df, stream)
    plot_monitors(df)
    Sys.sleep(options$update)
  }
}
