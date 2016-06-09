#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "monitor.h"

#define ACTIVE   1
#define SLEEPING 0

struct hmon_array *             monitors;
static struct hmon_array *      monitors_to_print;
hwloc_topology_t           monitors_topology;
unsigned                   monitors_topology_depth;
static unsigned long       monitors_pid;
static FILE *              monitors_output_file;
static struct timespec     monitors_start_time, monitors_current_time;
hwloc_cpuset_t             monitors_running_cpuset;
static hwloc_obj_t         restrict_location;
static unsigned            monitor_thread_count;
static pthread_t *         monitor_threads;
static pthread_barrier_t   monitor_threads_barrier;
static volatile int        monitor_threads_stop = 0;

#define _monitors_do(ms, call, ...)					\
    do{for(unsigned i = 0; i< hmon_array_length(ms); i++){call(hmon_array_get(ms,i), ##__VA_ARGS__);}}while(0)

static hwloc_obj_t _monitor_find_core_host(hwloc_obj_t);
static void        _monitor_thread_cleanup(void * arg);
static void *      _monitor_thread        (void*);
static void        _monitor_delete        (struct monitor*);
static void        _monitor_reset         (struct monitor*);
static void        _monitor_remove        (struct monitor*);
static void        _monitor_read          (struct monitor*);
static int         _monitor_start         (struct monitor*);
static int         _monitor_stop          (struct monitor*);
static void        _monitor_read          (struct monitor*);
static void        _monitor_update_state  (struct monitor*);
static void        _monitor_restrict      (struct monitor*);
static void        _monitor_output_sample (struct monitor* , unsigned);
static int         _monitor_location_compare(void*, void*);


struct monitor *
new_monitor(hwloc_obj_t location,
	    void * eventset, 
	    unsigned n_events,
	    unsigned n_samples, 
	    const char * perf_plugin,
	    const char * events_to_sample,
	    const char * samples_to_value,
	    int silent)
{
    struct monitor * monitor;

    /* Look which core will host the monitor near location */ 
    hwloc_obj_t Core = _monitor_find_core_host(location); 

    malloc_chk(monitor, sizeof(*monitor));  
    
    /* push monitor to array holding monitor on Core */
    hmon_array_push(Core->userdata, monitor);
    
    /* record at least 3 samples */
    n_samples = 3 > n_samples ? 2 : n_samples; 
	

    /* Set default attributes */
    monitor->location = location;
    location->userdata = monitor;

    monitor->eventset = eventset;
    monitor->n_samples = n_samples;
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
    malloc_chk(monitor->events, n_samples*sizeof(*(monitor->events)));
    malloc_chk(monitor->samples, n_samples*sizeof(*(monitor->samples)));
    malloc_chk(monitor->timestamps, n_samples*sizeof(*(monitor->timestamps)));

    for(unsigned i = 0;i<monitor->n_samples;i++)
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

    /* Restrict topology to first group */
    restrict_location = location_parse(restrict_obj);
    
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
    for(i=0;i<monitor->n_samples;i++)
	free(monitor->events[i]);
    free(monitor->events);
    free(monitor->samples);
    free(monitor->timestamps);
    monitor->eventset_destroy(monitor->eventset);
    pthread_mutex_destroy(&(monitor->available));
    free(monitor);
}

/* 
 * Host must be a core where to spawn a thread 
 * Host must be into the restricted area, otherwise overhead of pthread_barrier is too high.
 */
static hwloc_obj_t _monitor_find_core_host(hwloc_obj_t near){
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
    /* if no thread have ever been hosted here, initialize location */
    if(host->userdata == NULL){
	host->userdata = new_hmon_array(sizeof(struct monitor *), monitors_topology_depth, NULL);
	monitor_thread_count++;
    }
    /* printf("will be hosted on %s:%d\n",hwloc_type_name(host->type), host->logical_index);	  */
    return host;
}

void monitors_start(){
    unsigned n = 0, n_leaves = hwloc_get_nbobjs_by_type(monitors_topology, HWLOC_OBJ_CORE);
    /* Sort monitors per location for update from leaves to root */
    hmon_array_sort(monitors, _monitor_location_compare);
    hmon_array_sort(monitors_to_print, _monitor_location_compare);

    /* Now we know the number of threads, we can initialize a barrier and an hmon_array of threads */
   pthread_barrier_init(&monitor_threads_barrier, NULL, monitor_thread_count+1);
    location_membind(hwloc_get_obj_by_type(monitors_topology, HWLOC_OBJ_NODE,0));	
    monitor_threads = malloc(sizeof(*monitor_threads) * monitor_thread_count);

    /* Then spawn threads on non NULL leaves userdata */
    for(unsigned i = 0; i<n_leaves; i++){
	hwloc_obj_t obj = hwloc_get_obj_by_type(monitors_topology, HWLOC_OBJ_CORE, i);
	if(obj->userdata != NULL){
	    hwloc_obj_t t_obj = obj;  
	    pthread_create(&(monitor_threads[n]),NULL,_monitor_thread, (void*)(t_obj));
	    n++;
	}
    }

    /* Wait threads initialization */
    pthread_barrier_wait(&monitor_threads_barrier);

    /* reset userdata at leaves with monitor */
    for(unsigned i = 0; i< hmon_array_length(monitors); i++){
	struct monitor * m = hmon_array_get(monitors,i);
	/* monitors are sorted from leaves to root */
	if(m->location->type != HWLOC_OBJ_PU)
	    break;
	m->location->userdata = m;
    }


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
    if(m->current+1 == m->n_samples){
	for(i=0;i<m->n_samples;i++){
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
    fprintf(monitors_output_file,"%s:%u ", hwloc_type_name(m->location->type), m->location->logical_index);
    fprintf(monitors_output_file,"%ld ", m->timestamps[i]);
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
   monitor->current = monitor->n_samples-1;
   monitor->stopped = 1;
   for(i=0;i<monitor->n_samples;i++){
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
	m->current = (m->current+1)%(m->n_samples);
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
	m->total = m->total+1; 
	/* Analyse samples */
	if(m->samples_to_value != NULL)
	    m->value = m->samples_to_value(m);
	else
	    m->value = m->samples[m->current];
	m->min  = MIN(m->min, m->value);
	m->max  = MAX(m->max, m->value);
    }
    pthread_mutex_unlock(&(m->available));
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

    struct hmon_array * _monitors = Core->userdata;
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
    _monitors_do(_monitors, _monitor_read);
    _monitors_do(_monitors, _monitor_analyse);
    goto monitor_start;

    pthread_cleanup_pop(1);
    return NULL;  
}

