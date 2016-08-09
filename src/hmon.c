#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <float.h>

#include "hmon.h"
#include "hmon_utils.h"

#define ACTIVE   1
#define SLEEPING 0

harray                     monitors;                 /* The array of monitors */
static harray              monitors_to_print;        /* The array of monitors to print */
hwloc_topology_t           monitors_topology;        /* The topology with monitor on hwloc_obj_t->userdata */
unsigned                   monitors_topology_depth;  /* The topology depth */
static FILE *              monitors_output_file;     /* The output where to write the trace */
static struct timespec     monitors_start_time;      /* The timer of when monitors have been started */
static struct timespec     monitors_current_time;    /* The timer of current time */
static unsigned long       monitors_pid;             /* The pid to follow */
hwloc_cpuset_t             monitors_running_cpuset;  /* The cpuset where the monitors_pid is running, or whole machine */

/** Monitors threads **/
static hwloc_obj_t         restrict_location;        /* The location where to spawn threads */
static unsigned            ncores;                   /* Number of cores inside restrict location */
harray*                    core_monitors;            /* An array of monitor per core in restrict location */
static unsigned            monitor_thread_count;     /* Number of threads */
static volatile int        monitor_threads_stop = 0; /* Flag telling whether threads must stop */
static pthread_t *         monitor_threads;          /* Threads id */
static pthread_barrier_t   monitor_threads_barrier;  /* Common barrier between monitors' thread and main thread */


#define _monitors_do(ms, call, ...)					\
    do{for(unsigned i = 0; i< harray_length(ms); i++){call(harray_get(ms,i), ##__VA_ARGS__);}}while(0)

static unsigned    _monitor_find_core_host(hwloc_obj_t);
static void        _monitor_thread_cleanup(void * arg);
static void *      _monitor_thread        (void*);
static void        _monitor_read_then_reduce(harray);
static void        _monitor_update_state  (hmon);
static void        _monitor_restrict      (hmon);
static void        _monitor_output_sample (hmon , unsigned);
static int         _monitor_location_compare(void*, void*);

static void print_avail_events(struct monitor_plugin * lib){
    char ** (* events_list)(int *) = monitor_plugin_load_fun(lib, "monitor_events_list", 1);
    if(events_list == NULL)
	return;

    int n = 0;
    char ** avail = events_list(&n);
    printf("List of available events:\n");
    while(n--){
	printf("\t%s\n",avail[n]);
	free(avail[n]);
    }
    free(avail);
}

hmon
new_monitor(const char * id,
	    hwloc_obj_t location,
	    harray event_names,
	    unsigned window,
	    unsigned n_samples,
	    const char * perf_plugin,
	    const char * model_plugin,
	    int silent)
{
    hmon monitor;
   
    /* Look which core will host the monitor near location */ 
    unsigned core_idx = _monitor_find_core_host(location);
    
    malloc_chk(monitor, sizeof(*monitor));  
    
    /* push monitor to array holding monitor on Core */
    harray_push(core_monitors[core_idx], monitor);
    
    /* record at least 1 samples */
    window = 1 > window ? 1 : window;     

    /* Set default attributes */
    monitor->id = strdup(id);
    monitor->location = location;
    monitor->window = window;
    monitor->userdata = NULL;
    
    /* Load perf plugin functions */
    struct monitor_plugin * plugin = monitor_plugin_load(perf_plugin, MONITOR_PLUGIN_PERF);
    if(plugin == NULL){
	fprintf(stderr, "Monitor on %s:%d, performance initialization failed\n", hwloc_type_name(location->type), location->logical_index);
	exit(EXIT_FAILURE);
    }
    monitor->eventset_start    = monitor_plugin_load_fun(plugin, "monitor_eventset_start",   1);
    monitor->eventset_stop     = monitor_plugin_load_fun(plugin, "monitor_eventset_stop",    1);
    monitor->eventset_reset    = monitor_plugin_load_fun(plugin, "monitor_eventset_reset",   1);
    monitor->eventset_read     = monitor_plugin_load_fun(plugin, "monitor_eventset_read",    1);
    monitor->eventset_destroy  = monitor_plugin_load_fun(plugin, "monitor_eventset_destroy", 1);
    if(monitor->eventset_start   == NULL ||
       monitor->eventset_stop    == NULL ||
       monitor->eventset_reset   == NULL ||
       monitor->eventset_destroy == NULL ||
       monitor->eventset_read    == NULL){
	fprintf(stderr, "Monitor on %s:%d, performance initialization failed\n",
		hwloc_type_name(location->type), location->logical_index);
	exit(EXIT_FAILURE);
    }

    /* Initialize eventset */
    int err;
    unsigned added_events = 0;
    int (* eventset_init)(void **, hwloc_obj_t);
    int (* eventset_init_fini)(void*);
    int (* add_named_event)(void*, char*);
    eventset_init      = monitor_plugin_load_fun(plugin, "monitor_eventset_init"           , 1);
    eventset_init_fini = monitor_plugin_load_fun(plugin, "monitor_eventset_init_fini"      , 1);
    add_named_event    = monitor_plugin_load_fun(plugin, "monitor_eventset_add_named_event", 1);

    if(eventset_init(&monitor->eventset, location)){
	monitor_print_err("%s failed to initialize eventset\n", id);
	exit(EXIT_FAILURE);
    }
    for(unsigned i=0; i<harray_length(event_names); i++){
	err = add_named_event(monitor->eventset,(char*)harray_get(event_names,i));
	if(err == -1){
	    monitor_print_err("failed to add event %s to %s eventset\n", (char*)harray_get(event_names,i), id);
	    print_avail_events(plugin);	    
	    exit(EXIT_FAILURE);
	}
	added_events += err;
    }
    eventset_init_fini(monitor->eventset);
    monitor->events = new_hmatrix(window, added_events+1);

    /* Events reduction */
    if(model_plugin){
	monitor->samples = malloc(sizeof(double) * n_samples+1);
	monitor->max = malloc(sizeof(double) * n_samples+1);
	monitor->min = malloc(sizeof(double) * n_samples+1);	
	monitor->n_samples = n_samples;
	monitor->model = monitor_stat_plugins_lookup_function(model_plugin);
    }
    else{
	monitor->samples = malloc(sizeof(double) * added_events+1);
	monitor->max = malloc(sizeof(double) * added_events+1);
	monitor->min = malloc(sizeof(double) * added_events+1);
	monitor->n_samples = added_events;
	monitor->model = NULL;
    }
    
    /* reset values */
    _monitor_reset(monitor);

    pthread_mutex_init(&(monitor->available), NULL);
    
    /* Store monitor on topology */
    if(location->userdata == NULL){location->userdata = new_harray(sizeof(monitor), 4, NULL);}
    harray_push(location->userdata, monitor);
    
    /* Add monitor to existing monitors*/
    harray_push(monitors, monitor);
    if(!silent){harray_push(monitors_to_print, monitor);}

    /* Initialization succeed */
    return monitor;
}

int monitors_restrict(pid_t pid){
    /* If the process is not running, return 0 */
    if(kill(pid,0)!=0){
	return 0;
    }
    monitors_pid = pid;
    proc_get_allowed_cpuset(pid, monitors_running_cpuset);
    _monitors_do(monitors, _monitor_restrict);
    return 1;
}

void monitors_register_threads(int recurse){
    if(monitors_pid == 0){
	monitors_pid = getpid();
	monitors_restrict(monitors_pid);
    }
    proc_get_running_cpuset(monitors_pid, monitors_running_cpuset, recurse);
    _monitors_do(monitors, _monitor_update_state);
}


int monitor_lib_init(hwloc_topology_t topo, char * restrict_obj, char * output){
    /* Check hwloc version */
#if HWLOC_API_VERSION >= 0x0020000
    /* Header uptodate for monitor */
    if(hwloc_get_api_version() < 0x20000){
	fprintf(stderr, "hwloc version mismatch, required version 0x20000 or later, found %#08x\n", hwloc_get_api_version());
	return -1;
    }
#else
    fprintf(stderr, "hwloc version too old, required version 0x20000 or later\n");
    return -1;    
#endif

    /* initialize topology */
    if(topo == NULL){
	hwloc_topology_init(&monitors_topology); 
	hwloc_topology_set_icache_types_filter(monitors_topology, HWLOC_TYPE_FILTER_KEEP_ALL);
	hwloc_topology_load(monitors_topology);
    }
    else
	hwloc_topology_dup(&monitors_topology,topo);

    if(monitors_topology==NULL){
	fprintf(stderr, "Failed to init topology\n");
	return -1;
    }
    monitors_topology_depth = hwloc_topology_get_depth(monitors_topology);
    
    if(output){
	monitors_output_file = fopen(output, "w");
	if(monitors_output_file == NULL){
	    perror("fopen");
	    monitors_output_file = stdout;
	}
    }
    else
	monitors_output_file = stdout;

    /* Restrict topology to first group and set an array of monitor per core */
    if(restrict_obj == NULL){restrict_location = hwloc_get_root_obj(monitors_topology);}
    else{restrict_location = location_parse(restrict_obj);}
    
    ncores = hwloc_get_nbobjs_by_type(monitors_topology, HWLOC_OBJ_CORE);
    malloc_chk(core_monitors, sizeof(*core_monitors) * ncores);

    for(unsigned i=0; i<ncores; i++)
	core_monitors[i] = new_harray(sizeof(hmon), monitors_topology_depth, NULL);
    
    /* Create cpuset for running monitors */
    monitors_running_cpuset = hwloc_bitmap_dup(hwloc_get_root_obj(monitors_topology)->cpuset); 

    /* create or monitor list */ 
    monitors = new_harray(sizeof(hmon), 32, (void (*)(void*))_monitor_delete);
    monitors_to_print = new_harray(sizeof(hmon), 32, NULL);

    monitor_threads_stop = 0;
    monitor_thread_count = 0;
    monitor_threads = NULL;
    return 0;
}

static int _monitor_location_compare(void* a, void* b){
    hmon monitor_a = *((hmon*) a);
    hmon monitor_b = *((hmon*) b);
    int lcomp = location_compare(&(monitor_a->location),&(monitor_b->location));
    if(lcomp == 0){
	int a_index = -1, b_index = -1;
	harray _monitors = monitor_a->location->userdata;
	for(unsigned i=0; i<harray_length(_monitors);i++){
	    if(harray_get(_monitors, i) == a){a_index = i;}
	    if(harray_get(_monitors, i) == b){b_index = i;}
	    if(a_index != -1 && b_index != -1){
		if(a_index > b_index){return -1;}
		else if(a_index > b_index){return 1;}
		else return 0;
	    }
	}
    }
    return lcomp;
}


void _monitor_delete(hmon monitor){
    delete_hmatrix(monitor->events);
    free(monitor->samples);
    free(monitor->max);
    free(monitor->min);
    free(monitor->id);
    monitor->eventset_destroy(monitor->eventset);
    pthread_mutex_destroy(&(monitor->available));
    free(monitor);
}

/* 
 * Host must be a core where to spawn a thread 
 * Host must be into the restricted area, otherwise overhead of pthread_barrier is too high.
 */
static unsigned _monitor_find_core_host(hwloc_obj_t near){
    /* printf("monitor located on %s:%d ", hwloc_type_name(near->type), near->logical_index); */
    /* match near to be in restricted topology */
    int cousins = get_max_objs_inside_cpuset_by_type(restrict_location->cpuset, near->type);
    /* near type is found inside cpuset of restricted topology */
    if(cousins > 0)
	near = hwloc_get_obj_inside_cpuset_by_depth(monitors_topology, restrict_location->cpuset, near->depth, near->logical_index%cousins);
    /* near is above restricted topology */
    else 
	near = restrict_location;

    hwloc_obj_t host = NULL;
    /* If child of core then host is the parent core */
    if(near->type == HWLOC_OBJ_PU){host = near->parent;}
    /* If core then host is the matching core in group */
    else if(near->type == HWLOC_OBJ_CORE){host = near;}
    /* Other wise we choose the first child on Core leaves */
    else {host = hwloc_get_obj_inside_cpuset_by_type(monitors_topology, near->cpuset, HWLOC_OBJ_CORE, 0);}

    /* allocate data near location */
    location_membind(host);
    /* if no hmon.have ever been hosted here, a thread will be spawned */
    unsigned core_idx = host->logical_index%ncores;
    if(harray_length(core_monitors[core_idx]) == 0)
	monitor_thread_count++;
    /* printf("will be hosted on %s:%d\n",hwloc_type_name(host->type), host->logical_index);	  */
    return core_idx;
}

void monitors_start(){
    unsigned n = 0;
    /* Sort monitors per location for update from leaves to root */
    harray_sort(monitors, _monitor_location_compare);
    harray_sort(monitors_to_print, _monitor_location_compare);

    /* Now we know the number of threads, we can initialize a barrier and an harray of threads */
    pthread_barrier_init(&monitor_threads_barrier, NULL, monitor_thread_count+1);
    location_membind(hwloc_get_obj_by_type(monitors_topology, HWLOC_OBJ_NODE,0));	
    monitor_threads = malloc(sizeof(*monitor_threads) * monitor_thread_count);

    /* Then spawn threads on non NULL leaves userdata */
    for(unsigned i = 0; i<ncores; i++){
	if(harray_length(core_monitors[i]) > 0){
	    hwloc_obj_t core = hwloc_get_obj_inside_cpuset_by_type(monitors_topology, restrict_location->cpuset, HWLOC_OBJ_CORE, i);
	    pthread_create(&(monitor_threads[n]),NULL,_monitor_thread, (void*)(core));
	    n++;
	}
    }

    /* Wait threads initialization */
    pthread_barrier_wait(&monitor_threads_barrier);

    /* Finally start the timer */
    clock_gettime(CLOCK_MONOTONIC, &monitors_start_time);
}


void monitors_update(){
    /* make Monitors busy */
    for(unsigned i = 0; i<harray_length(monitors); i++){
	hmon m = harray_get(monitors,i);
	pthread_mutex_lock(&m->available);
    }
    /* Get timestamp */
    clock_gettime(CLOCK_MONOTONIC, &monitors_current_time);
    /* Trigger monitors */
    pthread_barrier_wait(&monitor_threads_barrier);
}


void monitor_lib_finalize(){
    unsigned i;
    /* Stop monitors */
    for(i=0;i<monitor_thread_count;i++)
	pthread_cancel(monitor_threads[i]);
    pthread_barrier_wait(&monitor_threads_barrier);
    for(i=0;i<monitor_thread_count;i++)
	pthread_join(monitor_threads[i],NULL);
    
    /* Cleanup */
    fclose(monitors_output_file);
    free(core_monitors);
    delete_harray(monitors);
    delete_harray(monitors_to_print);
    free(monitor_threads);
    hwloc_bitmap_free(monitors_running_cpuset);
    hwloc_topology_destroy(monitors_topology);
}


void monitor_buffered_output(hmon m, int force){
    unsigned i;
    /* Really output when buffer is full to avoid IO */
    pthread_mutex_lock(&(m->available));
    if(m->last+1 == m->window){
	for(i=0;i<m->window;i++){
	    _monitor_output_sample(m,i);
	}
    }
    else if(force){
	for(i=0;i<=m->last;i++){
	    _monitor_output_sample(m,i);
	}
    }
    pthread_mutex_unlock(&(m->available));
}


void monitor_output(hmon m, int wait){
    if(wait){
	pthread_mutex_lock(&(m->available));
	_monitor_output_sample(m, m->last);
	pthread_mutex_unlock(&(m->available));
    }
    else
	_monitor_output_sample(m, m->last);
}


void monitors_output(void (* monitor_output_method)(hmon, int), int flag){
    _monitors_do(monitors_to_print, monitor_output_method, flag);
}


static void _monitor_output_sample(hmon m, unsigned i){
    unsigned j;
    fprintf(monitors_output_file,"%-16s ",    m->id);
    fprintf(monitors_output_file,"%8s:%u ", hwloc_type_name(m->location->type), m->location->logical_index);
    fprintf(monitors_output_file,"%14ld ",   (long)hmat_get(m->events, i, m->events.cols-1));
    for(j=0;j<m->n_samples;j++){fprintf(monitors_output_file,"%-16.6lf ", m->samples[j]);}
    fprintf(monitors_output_file,"\n");
}

static void _monitor_restrict(hmon m){
    if(!hwloc_bitmap_intersects(m->location->cpuset, monitors_running_cpuset)){
	harray_remove(monitors_to_print, harray_find(monitors_to_print, m, _monitor_location_compare));
	harray_remove(monitors, harray_find(monitors, m, _monitor_location_compare));
    }
}

static void _monitor_update_state(hmon m){
    if(hwloc_bitmap_intersects(monitors_running_cpuset, m->location->cpuset)){m->state_query = ACTIVE;}
    else{m->state_query=SLEEPING;}
}


void _monitor_reset(hmon m){
    m->last = 0;
    m->total = 0;
    m->last = m->window-1;
    m->stopped = 1;
    m->state_query = ACTIVE;
    hmat_zero(m->events);
    for(unsigned i=0;i<m->n_samples;i++){
	m->samples[i]=0;
	m->max[i]=DBL_MIN;
	m->min[i]=DBL_MAX;
    }
}


void _monitor_remove(hmon monitor){
    /* Remove on location */
    harray_remove(monitor->location->userdata, harray_find_unsorted(monitor->location->userdata, monitor));
    /* Remove from monitors */
    harray_remove(monitors, harray_find(monitors, monitor, _monitor_location_compare));
    /* Delete */
    _monitor_delete(monitor);
}


int _monitor_start(hmon m){
    if(m->stopped && m->state_query==ACTIVE){
	m->eventset_start(m->eventset);
	m->stopped = 0;
    }
    return 0;
}

int _monitor_stop(hmon m){
    if(!m->stopped){
	m->eventset_stop(m->eventset);
	m->stopped = 1;
    }
    return 0;
}

void _monitor_read(hmon m){
    int err = pthread_mutex_trylock(&m->available);
    /* read only if monitor is not up to date and if monitor active */
    if(m->state_query == ACTIVE && err == EBUSY){
	m->last = (m->last+1)%(m->window);
	/* Read events */
	if((m->eventset_read(m->eventset, hmat_get_row(m->events, m->last))) == -1){
	    fprintf(stderr, "Failed to read counters from monitor on obj %s:%d\n",
		    hwloc_type_name(m->location->type), m->location->logical_index);
	    _monitor_remove(m);
	}
	/* Save timestamp */
	hmat_set(tspec_diff((&monitors_start_time), (&monitors_current_time)), m->events, m->last, m->events.cols-1);
	m->total = m->total+1;	
    } else {pthread_mutex_unlock(&m->available);}
}

void _monitor_reduce(hmon m){
    int err = pthread_mutex_trylock(&m->available);
    if(m->state_query == ACTIVE && err == EBUSY){
	/* Reduce events */
	if(m->model!=NULL){m->model(m->events, m->last, m->samples, m->n_samples);}
	else{memcpy(m->samples, hmat_get_row(m->events, m->last), sizeof(double)*(m->n_samples));}
	for(unsigned i=0;i<m->n_samples;i++){
	    m->max[i] = (m->max[i] > m->samples[i]) ? m->max[i] : m->samples[i];
	    m->min[i] = (m->min[i] < m->samples[i]) ? m->min[i] : m->samples[i];
	}
    }
    pthread_mutex_unlock(&(m->available));
}

static void _monitor_read_then_reduce(harray a){
    unsigned i;
    /* Read monitors */
    for(i=0;i<harray_length(a); i++){_monitor_read(harray_get(a,i));}
    /* Analyze monitors */
    for(i=0;i<harray_length(a); i++){_monitor_reduce(harray_get(a,i));}
}

static void _monitor_thread_cleanup(void * arg){
    harray _monitors = (harray)arg;
    _monitors_do(_monitors, _monitor_stop);
    delete_harray(_monitors);    
}

void * _monitor_thread(void * arg)
{
    hwloc_obj_t Core = (hwloc_obj_t)(arg);
    /* Bind the thread */
    hwloc_obj_t PU = hwloc_get_obj_inside_cpuset_by_type(monitors_topology, Core->cpuset, HWLOC_OBJ_PU, hwloc_get_nbobjs_inside_cpuset_by_type(monitors_topology, Core->cpuset, HWLOC_OBJ_PU) -1);
    location_cpubind(PU); 
    location_membind(PU);

    harray _monitors = core_monitors[Core->logical_index%ncores];
    /* Sort monitors to update leaves first */
    harray_sort(_monitors, _monitor_location_compare);
    /* push clean method */
    pthread_cleanup_push(_monitor_thread_cleanup, _monitors);

    /* Wait other threads initialization */
    pthread_barrier_wait(&monitor_threads_barrier);

    /* Collect events */
 monitor_start:
    _monitors_do(_monitors, _monitor_start);
    pthread_barrier_wait(&monitor_threads_barrier);
    pthread_testcancel();
    _monitors_do(_monitors, _monitor_stop);
    _monitor_read_then_reduce(_monitors);
    goto monitor_start;

    pthread_cleanup_pop(1);
    return NULL;  
}

