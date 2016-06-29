library("optparse")

#Globals
df = data.frame(name = character(0), obj = character(0), nano = numeric(0), val = numeric(0))

#Parse options
inOpt = make_option(opt_str = c("-i", "--input"), type = "character", default = NULL, help = "Data set input file")
outOpt = make_option(opt_str = c("-o", "--output"), type = "character", default = NULL, help = "Output pdf file (static)")
filterOpt = make_option(opt_str = c("-f", "--filter"), type = "character",default = NULL, help = "Filter input monitor")
logOpt = make_option(opt_str = c("-l", "--log"), type = "logical", default = FALSE, action = "store_true", help = "logscale graph")
histOpt = make_option(opt_str = c("-p", "--histogram"), type = "logical", default = FALSE,action = "store_true", help = "plot histogram of values distribution instead of values")
winOpt = make_option( opt_str = c("-w", "--window"), type = "integer", default = 1000, help = "number of points to plot (dynamic)")
updateOpt = make_option(opt_str = c("-u", "--update"), type = "numeric", default = 1, help = "frequency of read from trace file and plot in seconds. (dynamic)")
dynOpt = make_option(opt_str = c("-d", "--dynamic"), type = "logical", default = FALSE, action = "store_true", help = "Set if plot should be displayed as parts of a large trace or updated trace. In this case --window*4 points will be plot and window will move by --window steps ahead every --update seconds")
opt_parser = OptionParser(option_list = c(inOpt, outOpt, filterOpt, logOpt, winOpt, updateOpt, dynOpt, histOpt))
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
  lines = readLines(connexion, n = options$window)
  for (line in lines) {
    frame_line = read.table(textConnection(line), col.names = c("name", "obj", "nano", "val"), flush = T)
    if (is.null(options$filter) || frame_line[1, 1] == options$filter) {
      frame = rbind(frame, frame_line)
    }
  }
  if (options$dynamic && nrow(frame) > options$window*4) {
    frame = frame[(nrow(frame)-4*options$window):nrow(frame), ]
  }
  frame
}
plot_monitors = function(frame) {
  ymax = max(frame[, 4],  na.rm = TRUE)
  ymin = min(frame[, 4],  na.rm = TRUE)
  if (!options$histogram) {
    xmin =  min(frame[, 3], na.rm = TRUE)
    xmax = max(frame[, 3], na.rm = TRUE)
    xticks = seq(xmin, xmax, (xmax - xmin) / 10)
    if (options$log == FALSE) {
      yticks = seq(from = ymin, to = ymax, by = (ymax - ymin) / 10)
      ylabels = yticks
    } else {
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
        plot(hist(data[, 4], 100, main = paste(options$input, obj, sep=":"), xlab="", xlim = c(ymin,ymax)), col = i)
      } else {
        plot(hist(data[, 4], 100, main = paste(options$input, obj, sep=":"), xlab="", xlim = c(ymin,ymax)), col = i, add=T)
      }
    } else{
        l = ""
        if(options$log){l="y"}
        plot(
          x = data[,3],
          y = data[,4],
          main = options$title,
          log = l,
          type = 'p',
          pch = 1,
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
  dev.flush()
}

#Script
if (!options$dynamic) {
  pdf(options$output, family = "Helvetica", width = 10, height = 5)
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
