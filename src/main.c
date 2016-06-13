#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/timerfd.h>

#include "monitor.h"

static int timerfd;
static fd_set in_fds, in_fds_original;
static int nfds;
static struct itimerspec interval;
static struct timeval timeout;

union perf_option_value{
    int    int_value;
    char * str_value;
};

#define OPT_TYPE_INT     2
#define OPT_TYPE_STRING  1
#define OPT_TYPE_BOOL    0

struct perf_option{
    const char * name;
    const char * arg;
    int type;
    union perf_option_value value;
    const char * def_val;
    int set;
};

static struct perf_option help_opt =    {.name = "--help" ,   .arg = "",                          .type = OPT_TYPE_BOOL,   .value.int_value = 0,      .def_val = "0",           .set = 0};
static struct perf_option input_opt =   {.name = "--input",   .arg = "<perf_group_file>",         .type = OPT_TYPE_STRING, .value.str_value = NULL,   .def_val = "NULL",        .set = 0};
static struct perf_option output_opt =  {.name = "--output",  .arg = "<output_file>",             .type = OPT_TYPE_STRING, .value.str_value = NULL,   .def_val = "/dev/stdout", .set = 0};
static struct perf_option restrict_opt =  {.name = "--restrict",  .arg = "<hwloc_obj:index>",     .type = OPT_TYPE_STRING, .value.str_value = NULL,   .def_val = "Machine:0", .set = 0};
static struct perf_option trace_opt =   {.name = "--trace",   .arg = "<perf_trace_file>",         .type = OPT_TYPE_STRING, .value.str_value = NULL,   .def_val = "NULL",        .set = 0};
static struct perf_option pid_opt =     {.name = "--pid",     .arg = "<pid (-1= whole-machine)>", .type = OPT_TYPE_INT,    .value.int_value = -1,     .def_val = "-1",          .set = 0};
static struct perf_option refresh_opt = {.name = "--refresh", .arg = "<refresh_usec>",            .type = OPT_TYPE_INT,    .value.int_value = 100000, .def_val = "100000",      .set = 0};
static struct perf_option display_opt = {.name = "--display-topology", .arg = "",                 .type = OPT_TYPE_INT,    .value.int_value = 0,      .def_val = "0",           .set = 0};

#define n_opt 8

static unsigned set_option(struct perf_option * opt, const char * val){
    switch(opt->type){
    case OPT_TYPE_INT:
	if(val) {
	    opt->value.int_value = atoi(val); 
	    opt->set = 1; 
	    return 1;
	}
	else{
	  opt->set = 1; 
	  return 1;
	}
	break;
    case OPT_TYPE_BOOL:
	opt->set = 1;
	opt->value.int_value = !opt->value.int_value; 
	break;
    case OPT_TYPE_STRING:
	if(val){
	    opt->set = 1;
	    opt->value.str_value = strdup(val);
	    if(opt->value.str_value == NULL){
		perror("strdup");
		exit(EXIT_FAILURE);
	    }
	    return 1; 
	}
	break;
    default:
	break;
    }
    return 0;
 }


static void usage(char* argv0, struct perf_option ** options)
{
    unsigned i;
    printf("%s <opt> <opt_arg> -- bin <bin_args...> \n",argv0);
    printf("Available options:\n");
    
    for(i=0;i<n_opt;i++){
	printf("\t%s %s: default=%s\n",options[i]->name, options[i]->arg, options[i]->def_val);
    }

    printf("Plugins:\n");
    printf("You can use plugins both for event collection and monitor analysis\n");
    printf("Plugins file must have a pattern name: <plugin>.monitor_plugin.so\n");
    printf("Providing <plugin> or pattern name, or full path will search the plugin in location findable by dlopen\n");
    printf("There are two kinds of plugins:\n");
    printf("\t* Event collection plugins defined in PERF_LIB field of the monitors.\n");
    printf("\t* And stat library to aggregate events into samples and samples into value, defined in a double colon separated list in MONITOR_STAT_PLUGINS environment. The default stat_default plugin is automatically loaded\n");
    
    printf("Monitor input:\n");
    printf("\t$>%s --input my_perf_group.txt\n",argv0);
    printf("\tWill output to stdout the monitoring of the whole machine using the events settings in \"my_perf_group.txt\"\n"); 
    printf("\n");
    printf("Input file syntaxe:\n");
    printf("\tMONITOR_NAME{\n");
    printf("\t\tOBJ:= PU;                         # Depth where to map monitors. One monitor per obj at this depth is created.\n");
    printf("\t\tPERF_LIB:=papi;                   # Performance library to read counters. Can be a file of type path/<name>.monitor_plugin.so or <name> if plugin <name>.monitor_plugin.so is findable by dlopen.\n");
    printf("\t\tEVSET:= PAPI_L1_DCM, PAPI_L1_DCA; # A list of events defined by PERF_LIB.\n");     
    printf("\t\tN_SAMPLE:=128;                    # The buffer size for timestamps, samples and events. Default to 32.\n");
    printf("\t\tEVSET_REDUCE:=$0/$1;              # An arithmetic expression of events in EVSET, or a function name loadable in a stat plugin.\n");
    printf("\t\tSAMPLES_REDUCE:=monitor_samples_last; # A function loadable in a stat plugin.\n");
    printf("\t\tMAX:=0;                           # Preset a maximum monitor value to keep in monitor structure. Default to 0.\n");
    printf("\t\tMIN:=0;                           # Preset a minimum monitor value to keep in monitor structure. Default to 0.\n");
    printf("\t\tACCUMULATE:=1;                    # Set if PERF_LIB should accumulate events values along time. Default to 0 (false).\n");
    printf("\t\tOUTPUT:=/dev/stdout;              # Set a specific output for this monitor (default to --output option).\n\t}\n\n");
    printf("\tMONITOR_REDUCE{\n");
    printf("\t\tOBJ:= L3;\n");
    printf("\t\tPERF_LIB:= hierarchical;\n");
    printf("\t\tEVSET:=MONITOR_NAME;              # The monitors MONITOR_NAME child of each monitor MONITOR_REDUCE are selected and there eventset will be accumulated hierarchically\n");
    printf("\t\tEVSET_REDUCE:=$0/$1;              # Reduce the same way as children\n\t}\n\n");
    printf("/!\\ Though monitors' thread and memory are mapped to the specified hwloc object, it is the responsibility of the library PERF_LIB to make sure that eventset initialization on this object will lead to value relative to this object. As an example, you can check default PAPI implementation papi.monitor_plugin.so which performs cpu binding.\n");
}

int
main (int argc, char *argv[])
{
    struct perf_option * options[n_opt];
    options[0] = &input_opt;
    options[1] = &output_opt;
    options[2] = &refresh_opt;
    options[3] = &help_opt;
    options[4] = &pid_opt;
    options[5] = &trace_opt;
    options[6] = &display_opt;
    options[7] = &restrict_opt;
    char * runnable = NULL;
    char ** run_args = NULL;

    /* read options */
    unsigned i;
    char * argv0 = argv[0];
    while(argc > 1){      
	argc--;
	argv++;
	for(i=0;i<n_opt;i++){
	    struct perf_option * opt = options[i];
	    if (!strcmp(argv[0], opt->name)){
		int optarg = set_option(opt, argv[1]);
		argc-=(optarg);
		argv+=(optarg);
		goto opt_continue;
	    }
	}
	/* Runnable option */
	if(!strcmp(argv[0],"--")){
	    runnable = argv[1];
	    run_args = &argv[1];
	    break;
	}
	/* no option was recognized: exit */
	monitor_print_err( "Bad option %s\n\n", argv[0]);
	usage(argv0,options);
	exit(EXIT_SUCCESS);
    opt_continue:;
    }

    /* check for help */
    if(help_opt.set){
	usage(argv0,options);
	exit(EXIT_SUCCESS);
    }

    /* Translate pid_opt to pid value */
    pid_t pid = 0;
    if(pid_opt.set){
	pid = (pid_t) pid_opt.value.int_value;
    }

    /* Set monitors output */
    if(!output_opt.set){
	set_option((&output_opt), output_opt.def_val);
	output_opt.set = 0;
    }

    /* Restrict monitors */
    if(!restrict_opt.set){
	set_option((&restrict_opt), restrict_opt.def_val);
    }

    /* check if configuration file was provided */
    if(!input_opt.set){
	monitor_print_err("--input option required\n");
	exit(EXIT_SUCCESS);
    }
  
    /* Set timer to read monitors */
    timerfd = timerfd_create(CLOCK_REALTIME,0);
    if(timerfd==-1){
	perror("itimer");
	exit(1);
    }
    FD_SET(timerfd, &in_fds_original);
    nfds = timerfd+1;

    /* set timer interval for update */
    interval.it_value.tv_sec  = refresh_opt.value.int_value / 1000000;
    interval.it_value.tv_nsec = 1000 * (refresh_opt.value.int_value % 1000000);
    interval.it_interval.tv_sec  = interval.it_value.tv_sec;
    interval.it_interval.tv_nsec  = interval.it_value.tv_nsec;
    char buf[sizeof(uint64_t)]; /* To read timerfd */

    /* Monitors initialization */
    monitor_lib_init(NULL, restrict_opt.value.str_value, output_opt.value.str_value);

    if(trace_opt.set)
	setenv("MONITOR_TRACE_PATH",trace_opt.value.str_value,1);

    monitors_import(input_opt.value.str_value);

    if(hmon_array_length(monitors) == 0){
	monitor_print_err( "No monitor defined. Leaving.\n");
	monitor_lib_finalize();
	exit(EXIT_SUCCESS);
    }


    /* start timer */
    timerfd_settime(timerfd,0,&interval,NULL);
    /* start monitoring */
    monitors_start();

    /* Start executable and pause */
    if(runnable){
	pid = start_executable(runnable,run_args); 
    }

    
    /* while executable is running, or forever if there is no executable */
    if(pid>0){
	int err;
	int status;
	monitors_restrict(pid);
	if(!refresh_opt.set){
	    monitors_update();
	    monitors_output(monitor_output, 1);
	    waitpid(pid, &status, 0);
	    monitors_update();
	    monitors_output(monitor_output, 1);
	    if(display_opt.set)
		monitor_display_all(1);
	}
	else{
	    while((err = waitpid(pid, &status, WNOHANG)) == 0){
		in_fds = in_fds_original;
		timeout.tv_sec=1;
		timeout.tv_usec=0;
		if(select(nfds, &in_fds, NULL, NULL,&timeout)>0 &&
		   FD_ISSET(timerfd,&in_fds)){
		    if(read(timerfd,&buf,sizeof(uint64_t))==-1){
			perror("read");
		    } 
		    monitors_update();
		    monitors_output(monitor_buffered_output, 0);
		    if(display_opt.set)
			monitor_display_all(1);
		}
	    }
	    if(err < 0){
		perror("waitpid");
	    }
	    monitors_output(monitor_buffered_output, 1);
	}
    }
    else while(1){
	in_fds = in_fds_original;
	timeout.tv_sec=1;
	timeout.tv_usec=0;
	if(select(nfds, &in_fds, NULL, NULL,&timeout)>0 &&
	   FD_ISSET(timerfd,&in_fds)){
	    if(read(timerfd,&buf,sizeof(uint64_t))==-1){
		perror("read");
	    } 
	    monitors_update();
	    if(output_opt.set)
		monitors_output(monitor_buffered_output,0);
	    else
		monitors_output(monitor_output, 1);
	    if(display_opt.set){
 	      monitor_display_all(1);
	    }
	}
    }

    /* clean up */
    free(output_opt.value.str_value);
    free(restrict_opt.value.str_value);
    monitor_lib_finalize();
    return EXIT_SUCCESS;
}

