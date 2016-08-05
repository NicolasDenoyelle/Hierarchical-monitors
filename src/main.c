#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/timerfd.h>

#include "hmon.h"
#include "hmon_utils.h"

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
    const char * short_name;
    const char * arg;
    const char * desc;
    int type;
    union perf_option_value value;
    const char * def_val;
    int set;
};

static struct perf_option help_opt =    {.name = "--help",
					 .short_name = "-h",
					 .arg = "",
					 .desc = "Print this help message",
					 .type = OPT_TYPE_BOOL,
					 .value.int_value = 0,
					 .def_val = "0",
					 .set = 0};

static struct perf_option input_opt =   {.name = "--input",
					 .short_name = "-i",
					 .arg = "<monitor_file>",
					 .desc = "Input monitors description",
					 .type = OPT_TYPE_STRING,
					 .value.str_value = NULL,
					 .def_val = "NULL",
					 .set = 0};

static struct perf_option output_opt =  {.name = "--output",
					 .short_name = "-o",
					 .arg = "<output_file>",
					 .desc = "File where to write trace output",
					 .type = OPT_TYPE_STRING,
					 .value.str_value = NULL,
					 .def_val = "/dev/stdout",
					 .set = 0};

static struct perf_option restrict_opt =  {.name = "--restrict",
					   .short_name = "-r",
					   .arg = "<hwloc_obj:index>",
					   .desc = "Contain hmon on a subset of machine cores.",
					   .type = OPT_TYPE_STRING, .value.str_value = NULL,
					   .def_val = "Machine:0", .set = 0};

static struct perf_option pid_opt =     {.name = "--pid",
					 .short_name = "-p",
					 .arg = "<pid>",
					 .desc = "Restrict monitors on pid domain and stop monitoring when pid finished.",
					 .type = OPT_TYPE_INT,
					 .value.int_value = -1,
					 .def_val = "-1",
					 .set = 0};

static struct perf_option refresh_opt = {.name = "--frequency",
					 .short_name = "-f",
					 .arg = "<usec>",
					 .desc = "Update monitors every -f micro seconds.",
					 .type = OPT_TYPE_INT,
					 .value.int_value = 100000,
					 .def_val = "100000",
					 .set = 0};

static struct perf_option display_opt = {.name = "--display-topology",
					 .short_name = "-d",
					 .arg = "",
					 .desc = "Display first element output of last monitor on each object, on topology.",
					 .type = OPT_TYPE_BOOL,
					 .value.int_value = 0,
					 .def_val = "0",
					 .set = 0};

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


static void usage(char* argv0, struct perf_option ** options, unsigned n_opt)
{
    unsigned i;
    printf("%s <opt> (<opt_arg>) ... -- bin (<bin_args...>) \n\n",argv0);
    printf("Options:\n");
    
    for(i=0;i<n_opt;i++){
	printf("\t%s,%-20s %-20s",options[i]->short_name, options[i]->name, options[i]->arg);
	if(options[i]->type != OPT_TYPE_BOOL){printf(":default=%-16s", options[i]->def_val);}
	else{printf("%25s", "");}
	printf(" %s\n", options[i]->desc);
    }
    printf("\n");
}

int
main (int argc, char *argv[])
{
    const unsigned n_opt = 7;
    struct perf_option * options[n_opt];
    options[0] = &input_opt;
    options[1] = &output_opt;
    options[2] = &refresh_opt;
    options[3] = &help_opt;
    options[4] = &pid_opt;
    options[5] = &display_opt;
    options[6] = &restrict_opt;
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
	    if (!strcmp(argv[0], opt->name) || !strcmp(argv[0], opt->short_name)){
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
	usage(argv0,options, n_opt);
	exit(EXIT_SUCCESS);
    opt_continue:;
    }

    /* check for help */
    if(help_opt.set){
	usage(argv0,options, n_opt);
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
    monitors_import(input_opt.value.str_value);

    if(harray_length(monitors) == 0){
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

