library("optparse")

#Globals
df=data.frame(name =character(0), obj = character(0), nano = numeric(0), val = numeric(0))

#Parse options
fileOpt = make_option(opt_str=c("-f", "--file"), type="character", default=NULL, help="Data set file name")
outOpt = make_option(opt_str=c("-o", "--output"), type="character", default=NULL, help="Output pdf file (statique)")
restrictOpt = make_option(opt_str=c("-r", "--restrict"), type="character", default=NULL, help="Restrict input monitor")
logOpt = make_option(opt_str=c("-l", "--log"), type="logical", default=FALSE, action="store_true", help="logscale graph")
winOpt = make_option(opt_str=c("-w", "--window"), type="integer", default=10000, help="number of points to plot (interactive)")
updateOpt = make_option(opt_str=c("-u", "--update"), type="numeric", default=1, help="frequency of read from trace file and plot in seconds. (interactive)")
interactiveOpt = make_option(opt_str=c("-i", "--interactive"), type="logical", default=FALSE, action="store_true", help="Set if plot shouldbe interactive")
opt_parser = OptionParser(option_list=c(fileOpt, outOpt, restrictOpt, logOpt, winOpt, updateOpt, interactiveOpt))
options = parse_args(opt_parser, args = commandArgs(trailingOnly = TRUE), print_help_and_exit = TRUE, positional_arguments = FALSE)

#Ask input if not provided in options
if (is.null(options$file) || options$file == ""){
  options$file = readline(prompt = "Input file: ")
} else {
  print(sprintf("input file = %s", options$file))
}

#Check output
if(!is.null(options$output) && options$interactive){
  stop("Options --output and --interactive cannot be set simultaneously. Whether you plot statically the whole file, or you plot updated file dynamically.", call. = FALSE)
}
if(is.null(options$output) && !options$interactive){
  options$output = paste(options$file, "pdf", sep = ".")
  print(sprintf("Output to %s", options$output))
}

#Functions
lseq <- function(from=1, to=100000, length.out = 6) {exp(seq(log(from), log(to), length.out = length.out))}
read_monitors=function(df,connexion){
  lines = readLines(connexion)
  frame = data.frame(name =character(0), obj = character(0), nano = numeric(0), val = numeric(0))
  for (line in lines){
    df_line = read.table(textConnection(line), col.names = c("name", "obj", "nano", "val"))
    if(is.null(options$restrict)){
      options$restrict = as.character(df_line[1,1])
    }
    if(df_line[1,1] == options$restrict){
      frame = rbind(frame, df_line)
    }
  }
  df = rbind(df,frame)
  n = nrow(df)
  if(options$interactive && n>options$window){
    rm = n-options$window
    df = df[rm:n,]
  }
  df
}
plot_monitors = function(frame){
  ymax = max(frame[,4],  na.rm=TRUE)
  ymin = min(frame[,4],  na.rm=TRUE)
  xmin=  min(frame[,3], na.rm=TRUE)
  xmax = max(frame[,3], na.rm=TRUE)
  xticks = seq(xmin, xmax, (xmax-xmin)/10)
  if(options$log == FALSE){
    yticks = seq(from = ymin, to = ymax, by = (ymax-ymin)/10)
    ylabels = yticks
  } else {
    yticks = lseq(from = ymin, to = ymax, length.out = log10(ymax/ymin))
    ylabels = sapply(yticks, function(i) as.expression(bquote(10^ .(round(log10(i))))))
  }
  objs = unique(frame[,2])
  for(i in 1:length(objs)){
    obj = as.character(objs[i])
    data = subset(frame, frame[,2]==objs[i])
    color=as.integer(i)
    if(options$log == FALSE){
      plot(x = data$nano, y = data$val, xaxt="n", yaxt="n", xlab="", ylab = "", cex=1, type='p', pch=1, col=color, xlim=c(xmin,xmax), ylim=c(ymin,ymax), lty=1, axes=FALSE, panel.first=abline(h=yticks, v=xticks,col = "darkgray", lty = 3))
    } else {
      plot(x = data$nano, y = data$val, xaxt="n", yaxt="n", xlab="", ylab = "", log="y", cex=1, type='p', pch=1, col=color, xlim=c(xmin,xmax), ylim=c(ymin,ymax), lty=1, axes=FALSE, panel.first=abline(h=yticks, v=xticks,col = "darkgray", lty = 3))
    }
    par(new=TRUE, ann=FALSE)
  }
  title(main = options$file, xlab = "nanoseconds", ylab=options$restrict)
  legend("topright", legend=objs, pch = 1, cex=.7, lty=1, col=1:length(objs))
  axis(1, at = xticks)
  axis(2, at = yticks, labels=ylabels)
}

#Script
stream = fifo(options$file, open = "r")
if(!is.null(options$output)){
  pdf(options$output, family = "Helvetica", width=10, height=5)
  plot_monitors(read.table(options$file))
  graphics.off()
} else { 
    resolution = as.numeric(unlist(strsplit(system("xdpyinfo  | awk /dimensions/'{print $2}'", intern = TRUE), "x"))) / c(37.79527559055, 37.79527559055)
    x11(width = resolution[1], height = resolution[2])
    repeat{
    df = read_monitors(df,stream)
    plot_monitors(df)
    Sys.sleep(options$update)
  }
}
