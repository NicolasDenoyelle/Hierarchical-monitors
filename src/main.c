#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "./hmon.h"
#include "internal.h"
#include "display.h"

volatile int hmonitor_utility_stop;
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

static struct perf_option plugins_opt = {.name = "--list-plugins",
					 .short_name = "-l",
					 .arg = "",
					 .desc = "List peformance and statistic plugins.",
					 .type = OPT_TYPE_BOOL,
					 .value.int_value = 0,
					 .def_val = "0",
					 .set = 0};

static struct perf_option perf_opt =  {.name = "--perf-event",
				       .short_name = "-pe",
				       .arg = "<perf plugin>",
				       .desc = "List a performance plugin events.",
				       .type = OPT_TYPE_STRING, .value.str_value = NULL,
				       .def_val = "NULL", .set = 0};

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
					 .desc = "Update monitors every -f microseconds.",
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
    opt->set = 1;	          
    if(val){
      opt->value.str_value = strdup(val);
      if(opt->value.str_value == NULL){
	perror("strdup");
	exit(EXIT_FAILURE);
      }
      return 1; 
    } else fprintf(stderr, "String option %s without argument.\n", opt->name);
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

static void finalize_handler(int sig){
  if(sig == SIGINT || sig == SIGQUIT || sig == SIGTERM){
    hmonitor_utility_stop = 1;
  }
}

int
main (int argc, char *argv[])
{
  const unsigned n_opt = 8;
  struct perf_option * options[n_opt];
  options[0] = &input_opt;
  options[1] = &refresh_opt;
  options[2] = &help_opt;
  options[3] = &pid_opt;
  options[4] = &display_opt;
  options[5] = &restrict_opt;
  options[6] = &plugins_opt;
  options[7] = &perf_opt;        
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

  /* check plugin list option */
  if(plugins_opt.set){
    printf("Statistic plugins (REDUCTION field): ");
    hmon_stat_plugins_list();
    printf("Performance plugins (PERF_LIB field): ");
    hmon_perf_plugins_list();
    exit(EXIT_SUCCESS);
  }

  /* check list perf events option */
  if(perf_opt.set){
    if(perf_opt.value.str_value == NULL){
      fprintf(stderr, "Option %s requires an argument.\n", perf_opt.name);
    }
    else {
      struct hmon_plugin * plugin = hmon_plugin_lookup(perf_opt.value.str_value, HMON_PLUGIN_PERF);
      char ** (*events_list)(int*) = NULL;
      events_list = hmon_plugin_load_fun(plugin, "hmonitor_events_list", 1);
      if(events_list != NULL){
	int i,n;
	char ** events = events_list(&n);
	printf("%s plugin events:\n", perf_opt.value.str_value);	
	for(i=0;i<n;i++){
	  printf("\t%s\n", events[i]);
	  free(events[i]);
	}
	printf("\n");
	free(events);
      } else {
	fprintf(stderr, "Could not find perf plugin %s\n", perf_opt.value.str_value);
      }
    }
    exit(EXIT_SUCCESS);
  }
    
  hwloc_cpuset_t restrict_domain = hwloc_bitmap_alloc();
    
  /* Translate pid_opt to pid value */
  pid_t pid = 0;
  if(pid_opt.set){
    pid = (pid_t) pid_opt.value.int_value;
  }

  /* Monitors initialization */
  hmon_lib_init(NULL);

  /* Restrict monitors */
  if(restrict_opt.set){
    hwloc_obj_t obj_domain = location_parse(hmon_topology, restrict_opt.value.str_value);
    if(obj_domain!=NULL){
      hwloc_bitmap_copy(restrict_domain, obj_domain->cpuset);
      hmon_restrict(restrict_domain);
      hwloc_bitmap_copy(restrict_domain, obj_domain->cpuset);
    }
  }

  hmon_import_hmonitors(input_opt.value.str_value);

  /* check if configuration file was provided */
  if(!input_opt.set){
    monitor_print_err("--input option required\n");
    exit(EXIT_SUCCESS);
  }

  /* Set timer delay */
  if(!refresh_opt.set){refresh_opt.value.int_value = atoi(refresh_opt.def_val);}

  if(harray_length(monitors) == 0){
    monitor_print_err( "No monitor defined. Leaving.\n");
    hmon_lib_finalize();
    exit(EXIT_SUCCESS);
  }

  /* Prepare display */
  if(display_opt.set){hmon_display_init(hmon_topology);}

  /* start monitoring */
  hmon_start();

  /* Start executable */
  if(runnable){
    pid = start_executable(runnable,run_args);
    if(restrict_opt.set){
      if(hwloc_set_proc_cpubind(hmon_topology, pid, restrict_domain, HWLOC_CPUBIND_PROCESS|HWLOC_CPUBIND_STRICT))
	perror("hwloc_set_proc_cpubind");
    }
  }
    
  /* while executable is running, or until user input if there is no executable */
  if(pid == 0){
    /* And register signal handler */
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = finalize_handler;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGQUIT, &sa, NULL) == -1){perror("sigaction"); return -1;}
    if(sigaction(SIGINT, &sa, NULL) == -1){perror("sigaction"); return -1;}
    if(sigaction(SIGTERM, &sa, NULL) == -1){perror("sigaction"); return -1;}

    /* monitor topology */ 
    while(!hmonitor_utility_stop){
      hmon_update(1);
      if(display_opt.set){
	if(hmon_display_refresh(0) == -1) break;
      }
      usleep(refresh_opt.value.int_value);
    }
  }
    
  if(pid>0){
    int err;
    int status;
    if(!refresh_opt.set){
      hmon_update(1);
      waitpid(pid, &status, 0);
      hmon_update(1);
      if(display_opt.set){hmon_display_refresh(1);}
    }
    else{
      hmon_sampling_start(refresh_opt.value.int_value);
      if(display_opt.set){hmon_periodic_display_start(hmon_display_refresh, 1);}
    hmon_wait_child:
      err = waitpid(pid, &status, 0);
      if(err < 0){
	if(errno == EINTR){goto hmon_wait_child;}
	perror("waitpid");
      }
      hmon_sampling_stop();
      if(display_opt.set){hmon_periodic_display_stop();}
    }
  }
    
  /* cleanup */
out_with_lib:
  free(restrict_opt.value.str_value);
  hwloc_bitmap_free(restrict_domain);
  if(display_opt.set) hmon_display_finalize();
  hmon_lib_finalize();
  return EXIT_SUCCESS;
}

