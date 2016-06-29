#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "hmon.h"

#define ACTIVE   1
#define SLEEPING 0

struct hmon_array *        monitors;                 /* The array of monitors */
static struct hmon_array * monitors_to_print;        /* The array of monitors to print */
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
struct hmon_array **       core_monitors;            /* An array of monitor per core in restrict location */
static unsigned            monitor_thread_count;     /* Number of threads */
static volatile int        monitor_threads_stop = 0; /* Flag telling whether threads must stop */
static pthread_t *         monitor_threads;          /* Threads id */
static pthread_barrier_t   monitor_threads_barrier;  /* Common barrier between monitors' thread and main thread */


#define _monitors_do(ms, call, ...)					\
    do{for(unsigned i = 0; i< hmon_array_length(ms); i++){call(hmon_array_get(ms,i), ##__VA_ARGS__);}}while(0)

static unsigned    _monitor_find_core_host(hwloc_obj_t);
static void        _monitor_thread_cleanup(void * arg);
static void *      _monitor_thread        (void*);
static void        _monitor_delete        (struct monitor*);
static void        _monitor_reset         (struct monitor*);
static void        _monitor_remove        (struct monitor*);
static void        _monitor_read          (struct monitor*);
static void        _monitor_analyse       (struct monitor*);
static int         _monitor_start         (struct monitor*);
static int         _monitor_stop          (struct monitor*);
static void        _monitor_read          (struct monitor*);
static void        _monitor_read_analyse_per_depth(struct hmon_array*);
static void        _monitor_update_state  (struct monitor*);
static void        _monitor_restrict      (struct monitor*);
static void        _monitor_output_sample (struct monitor* , unsigned);
static int         _monitor_location_compare(void*, void*);


struct monitor *
new_monitor(const char * id,
	    hwloc_obj_t location,
	    void * eventset, 
	    unsigned n_events,
	    unsigned window, 
	    const char * perf_plugin,
	    const char * events_to_sample,
	    const char * samples_to_value,
	    int silent)
{
    struct monitor * monitor;

    /* Check if location is available */
    if(location->userdata != NULL){
	fprintf(stderr, "Cannot create monitor %s on %s:%d, because location is not empty\n",
		id, hwloc_type_name(location->type), location->logical_index);
	return NULL;
    }
    
    /* Look which core will host the monitor near location */ 
    unsigned core_idx = _monitor_find_core_host(location);
    
    malloc_chk(monitor, sizeof(*monitor));  
    
    /* push monitor to array holding monitor on Core */
    hmon_array_push(core_monitors[core_idx], monitor);

    location->userdata = monitor;
    monitor->location = location;
    
    /* record at least 1 samples */
    window = 1 > window ? 1 : window; 
    

    /* Set default attributes */
    monitor->id = strdup(id);
    monitor->location = location;
    location->userdata = monitor;

    monitor->eventset = eventset;
    monitor->window = window;
    monitor->n_events = n_events;
    monitor->events = NULL;
    monitor->timestamps = NULL;
    monitor->state_query = ACTIVE;
    monitor->userdata = NULL;

    /* Load perf plugin functions */
    struct monitor_plugin * plugin = monitor_plugin_load(perf_plugin, MONITOR_PLUGIN_PERF);
    if(plugin == NULL){
	fprintf(stderr, "Monitor on %s:%d, performance initialization failed\n", hwloc_type_name(location->type), location->logical_index);
	free(monitor);
	return NULL;
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
	fprintf(stderr, "Monitor on %s:%d, performance initialization failed\n", hwloc_type_name(location->type), location->logical_index);
	free(monitor);
	return NULL;
    }

    if(events_to_sample != NULL)
	monitor->events_to_sample = monitor_stat_plugins_lookup_function(events_to_sample);
    else
	monitor->events_to_sample = NULL;

    if(samples_to_value != NULL)
	monitor->samples_to_value = monitor_stat_plugins_lookup_function(samples_to_value);
    else
	monitor->samples_to_value = NULL;

    /* allocate arrays */
    malloc_chk(monitor->events, window*sizeof(*(monitor->events)));
    malloc_chk(monitor->samples, window*sizeof(*(monitor->samples)));
    malloc_chk(monitor->timestamps, window*sizeof(*(monitor->timestamps)));

    for(unsigned i = 0;i<monitor->window;i++)
	malloc_chk(monitor->events[i], n_events*sizeof(*(monitor->events[i])));
    
    /* reset values */
    _monitor_reset(monitor);

    pthread_mutex_init(&(monitor->available), NULL);

    /* Add monitor to existing monitors*/
    hmon_array_push(monitors, monitor);
    if(!silent){hmon_array_push(monitors_to_print, monitor);}
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
	core_monitors[i] = new_hmon_array(sizeof(struct monitor *), monitors_topology_depth, NULL);
    
    /* Create cpuset for running monitors */
    monitors_running_cpuset = hwloc_bitmap_dup(hwloc_get_root_obj(monitors_topology)->cpuset); 

    /* create or monitor list */ 
    monitors = new_hmon_array(sizeof(struct monitor *), 32, (void (*)(void*))_monitor_delete);
    monitors_to_print = new_hmon_array(sizeof(struct monitor *), 32, NULL);

    monitor_threads_stop = 0;
    monitor_thread_count = 0;
    monitor_threads = NULL;
    return 0;
}


static int _monitor_location_compare(void* a, void* b){
    struct monitor * monitor_a = *((struct monitor **) a);
    struct monitor * monitor_b = *((struct monitor **) b);
    return location_compare(&(monitor_a->location),&(monitor_b->location));
}


static void _monitor_delete(struct monitor * monitor){
    unsigned i;
    for(i=0;i<monitor->window;i++)
	free(monitor->events[i]);
    free(monitor->events);
    free(monitor->samples);
    free(monitor->timestamps);
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
    if(hmon_array_length(core_monitors[core_idx]) == 0)
	monitor_thread_count++;
    /* printf("will be hosted on %s:%d\n",hwloc_type_name(host->type), host->logical_index);	  */
    return core_idx;
}

void monitors_start(){
    unsigned n = 0;
    /* Sort monitors per location for update from leaves to root */
    hmon_array_sort(monitors, _monitor_location_compare);
    hmon_array_sort(monitors_to_print, _monitor_location_compare);

    /* Now we know the number of threads, we can initialize a barrier and an hmon_array of threads */
    pthread_barrier_init(&monitor_threads_barrier, NULL, monitor_thread_count+1);
    location_membind(hwloc_get_obj_by_type(monitors_topology, HWLOC_OBJ_NODE,0));	
    monitor_threads = malloc(sizeof(*monitor_threads) * monitor_thread_count);

    /* Then spawn threads on non NULL leaves userdata */
    for(unsigned i = 0; i<ncores; i++){
	if(hmon_array_length(core_monitors[i]) > 0){
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
    /* Make monitors unavailable */
    for(unsigned i = 0; i< hmon_array_length(monitors); i++)
	pthread_mutex_lock(&(((struct monitor *)hmon_array_get(monitors,i))->available));
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
    delete_hmon_array(monitors);
    delete_hmon_array(monitors_to_print);
    free(monitor_threads);
    hwloc_bitmap_free(monitors_running_cpuset);
    hwloc_topology_destroy(monitors_topology);
}


void monitor_buffered_output(struct monitor * m, int force){
    unsigned i;
    /* Really output when buffer is full to avoid IO */
    pthread_mutex_lock(&(m->available));
    if(m->current+1 == m->window){
	for(i=0;i<m->window;i++){
	    _monitor_output_sample(m,i);
	}
    }
    else if(force){
	for(i=0;i<=m->current;i++){
	    _monitor_output_sample(m,i);
	}
    }
    pthread_mutex_unlock(&(m->available));
}


void monitor_output(struct monitor * m, int wait){
    if(wait){
	pthread_mutex_lock(&(m->available));
	_monitor_output_sample(m, m->current);
	pthread_mutex_unlock(&(m->available));
    }
    else
	_monitor_output_sample(m, m->current);
}


void monitors_output(void (* monitor_output_method)(struct monitor*, int), int flag){
    _monitors_do(monitors_to_print, monitor_output_method, flag);
}


static void _monitor_output_sample(struct monitor * m, unsigned i){
    unsigned j;
    fprintf(monitors_output_file,"%s ",    m->id);
    fprintf(monitors_output_file,"%s:%u ", hwloc_type_name(m->location->type), m->location->logical_index);
    fprintf(monitors_output_file,"%ld ",   m->timestamps[i]);
    if(m->samples_to_value != NULL){
	fprintf(monitors_output_file,"%lf ", m->value);
    }
    else if(m->events_to_sample != NULL){
	fprintf(monitors_output_file,"%lf ", m->samples[i]);
    }
    else{
	for(j=0;j<m->n_events;j++)
	    fprintf(monitors_output_file,"%lld ", m->events[i][j]);
    }
    fprintf(monitors_output_file,"\n");
}

static void _monitor_restrict(struct monitor * m){
    if(!hwloc_bitmap_intersects(m->location->cpuset, monitors_running_cpuset)){
	hmon_array_remove(monitors_to_print, hmon_array_find(monitors_to_print, m, _monitor_location_compare));
	hmon_array_remove(monitors,       hmon_array_find(monitors, m, _monitor_location_compare));
    }
}

static void _monitor_update_state(struct monitor * m){
    if(hwloc_bitmap_intersects(monitors_running_cpuset, m->location->cpuset)){m->state_query = ACTIVE;}
    else{m->state_query=SLEEPING;}
}


static void _monitor_reset(struct monitor * monitor){
unsigned i, j;
   monitor->value = 0;
   monitor->min = 0;
   monitor->max = 0;
   monitor->mu = 0;
   monitor->current = monitor->window-1;
   monitor->stopped = 1;
   for(i=0;i<monitor->window;i++){
       monitor->samples[i] = 0;
       monitor->timestamps[i] = 0;
       for(j = 0; j < monitor->n_events; j++)
	   monitor->events[i][j] = 0;
   }
}


static void _monitor_remove(struct monitor * monitor){
    /* print remaining events */    
    hmon_array_remove(monitors, hmon_array_find(monitors, monitor, _monitor_location_compare));
    _monitor_delete(monitor);
}


static int _monitor_start(struct monitor * m){
    if(m->stopped && m->state_query==ACTIVE){
	m->eventset_start(m->eventset);
	m->stopped = 0;
    }
    return 0;
}

static int _monitor_stop(struct monitor * m){
    if(!m->stopped){
	m->eventset_stop(m->eventset);
	m->stopped = 1;
    }
    return 0;
}

static void _monitor_read(struct monitor * m){
    if(m->state_query == ACTIVE){
	m->current = (m->current+1)%(m->window);
	/* Read events */
	if((m->eventset_read(m->eventset,m->events[m->current])) == -1){
	    fprintf(stderr, "Failed to read counters from monitor on obj %s:%d\n",
		    hwloc_type_name(m->location->type), m->location->logical_index);
	    _monitor_remove(m);
	}
    }
}

static void _monitor_analyse(struct monitor * m){
    if(m->state_query == ACTIVE){
	/* Save timestamp */
	m->timestamps[m->current] = tspec_diff((&monitors_start_time), (&monitors_current_time));
	/* Reduce events */
	if(m->events_to_sample != NULL)
	    m->samples[m->current] = m->events_to_sample(m);
	else if(m->n_events == 1)
	    m->samples[m->current] = m->events[m->current][0];
	double sample = m->samples[m->current];
	/* Analyse samples */
	if(m->samples_to_value != NULL)
	    m->value = m->samples_to_value(m);
	else
	    m->value = sample;
	m->min  = MIN(m->min, sample);
	m->max  = MAX(m->max, sample);
	m->mu   = (sample + m->total*m->mu) / (m->total+1);
	m->total = m->total+1; 
    }
    pthread_mutex_unlock(&(m->available));
}

/* Assumes monitor array is sorted per depth */
static void _monitor_read_analyse_per_depth(struct hmon_array * a){
    struct monitor * m, * child;
    unsigned i, j = 0, depth;

    m = hmon_array_get(a,0);
    depth = m->location->depth;

    for(i=0;i<hmon_array_length(a); i++){
	m = hmon_array_get(a,i);
	if(m->location->depth < depth){
	    /* Roll back to analyze previous depth */
	    for(; j<i; j++){
		child = hmon_array_get(a,j);
		_monitor_analyse(child);
	    }
	}
	depth = m->location->depth;
	/* Read monitor */
	_monitor_read(m);
    }
    /* Analyze remaining monitors */
    for(; j<i; j++)
	_monitor_analyse(hmon_array_get(a,j));
}

static void _monitor_thread_cleanup(void * arg){
    struct hmon_array * _monitors = (struct hmon_array *)arg;
    _monitors_do(_monitors, _monitor_stop);
    delete_hmon_array(_monitors);    
}

void * _monitor_thread(void * arg)
{
    hwloc_obj_t Core = (hwloc_obj_t)(arg);
    /* Bind the thread */
    hwloc_obj_t PU = hwloc_get_obj_inside_cpuset_by_type(monitors_topology, Core->cpuset, HWLOC_OBJ_PU, hwloc_get_nbobjs_inside_cpuset_by_type(monitors_topology, Core->cpuset, HWLOC_OBJ_PU) -1);
    location_cpubind(PU); 
    location_membind(PU);

    struct hmon_array * _monitors = core_monitors[Core->logical_index%ncores];
    /* Sort monitors to update leaves first */
    hmon_array_sort(_monitors, _monitor_location_compare);
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
    _monitor_read_analyse_per_depth(_monitors);
    goto monitor_start;

    pthread_cleanup_pop(1);
    return NULL;  
}

