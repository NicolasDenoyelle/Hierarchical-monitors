#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "monitor.h"
#include "performance.h"
#include "stats.h"

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

static unsigned            monitor_thread_count;
static pthread_t *         monitor_threads;
static pthread_barrier_t   monitor_threads_barrier;
static volatile int        monitor_threads_stop = 0;

#define _monitors_do(ms, call, ...)					\
    do{for(unsigned i = 0; i< hmon_array_length(ms); i++){call(hmon_array_get(ms,i), ##__VA_ARGS__);}}while(0)

static hwloc_obj_t _monitor_find_core_host(hwloc_obj_t);
static void        _monitor_thread_cleanup(void * arg)
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
	    struct monitor_perf_lib * perf_lib,
	    struct monitor_stats_lib * stat_evset_lib,
	    struct monitor_stats_lib * stat_samples_lib, 
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
    monitor->perf_lib = perf_lib;
    monitor->events_stat_lib = stat_evset_lib;
    monitor->samples_stat_lib = stat_samples_lib;
    monitor->events = NULL;
    monitor->timestamps = NULL;
    monitor->state_query = ACTIVE;
    monitor->userdata = NULL;
    pthread_mutex_init(&(monitor->available), NULL);
    
    /* allocate hmon_arrays */
    malloc_chk(monitor->events, n_samples*sizeof(*(monitor->events)));
    malloc_chk(monitor->samples, n_samples*sizeof(*(monitor->samples)));
    malloc_chk(monitor->timestamps, n_samples*sizeof(*(monitor->timestamps)));

    for(unsigned i = 0;i<monitor->n_samples;i++)
	malloc_chk(monitor->events[i], n_events*sizeof(*(monitor->events[i])));
    
    /* reset values */
    _monitor_reset(monitor);

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


int monitor_lib_init(hwloc_topology_t topo, char * output){
    /* initialize topology */
    if(topo == NULL){
	hwloc_topology_init(&monitors_topology); 
	hwloc_topology_set_icache_types_filter(monitors_topology, HWLOC_TYPE_FILTER_KEEP_ALL);
	hwloc_topology_load(monitors_topology);
    }
    else
	hwloc_topology_dup(&monitors_topology,topo);

    if(monitors_topology==NULL){
	return 1;
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
    monitor->perf_lib->monitor_eventset_destroy(monitor->eventset);
    pthread_mutex_destroy(&(monitor->available));
    free(monitor);
}

/* Host must be a core where to spawn a thread */
static hwloc_obj_t _monitor_find_core_host(hwloc_obj_t near){
    hwloc_obj_t host = NULL;
    /* host = hwloc_get_obj_by_type(monitors_topology, HWLOC_OBJ_CORE, 0); */
    /* if(host->userdata == NULL){ */
    /* 	host->userdata = new_hmon_array(sizeof(struct monitor *), monitors_topology_depth, NULL); */
    /* 	monitor_thread_count++; */
    /* } */
    /* return host; */

    /* If child of core then host is the parent core */
    if(near->type == HWLOC_OBJ_PU){
	host = near->parent;
    }
    /* If core then host is the location parameter */
    else if(near->type == HWLOC_OBJ_CORE){
	host = near;
    }
    /* Other wise we choose the least loaded child on Core leaves */
    else{
	hwloc_obj_t potential_host = NULL;
	hwloc_cpuset_t cpuset = near->cpuset;
	hwloc_obj_type_t CORE = HWLOC_OBJ_CORE;
	unsigned n_core = hwloc_get_nbobjs_inside_cpuset_by_type(monitors_topology, cpuset, CORE);
	for(unsigned i = 0; i< n_core; i++){
	    potential_host = hwloc_get_obj_inside_cpuset_by_type(monitors_topology, cpuset, CORE, i);
	    if(potential_host->userdata == NULL){
		host = potential_host;
		break;
	    }
	    if(host == NULL){host = potential_host;}
	    if(hmon_array_length(potential_host->userdata) < hmon_array_length(host->userdata)){host = potential_host;}
	}
    }
    if(host->userdata == NULL){
	/* allocate data near location */
	location_membind(host);	
	host->userdata = new_hmon_array(sizeof(struct monitor *), monitors_topology_depth, NULL);
	monitor_thread_count++;
    }
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
    if(m->samples_stat_lib){
	fprintf(monitors_output_file,"%lf ", m->value);
    }
    else if(m->events_stat_lib){
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
	m->perf_lib->monitor_eventset_start(m->eventset);
	m->stopped = 0;
    }
    return 0;
}

static int _monitor_stop(struct monitor * m){
    if(!m->stopped){
	m->perf_lib->monitor_eventset_stop(m->eventset);
	m->stopped = 1;
    }
    return 0;
}

static void _monitor_read(struct monitor * m){
    if(m->state_query == ACTIVE){
	m->current = (m->current+1)%(m->n_samples);
	/* Read events */
	if((m->perf_lib->monitor_eventset_read(m->eventset,m->events[m->current])) == -1){
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
	if(m->events_stat_lib)
	    m->samples[m->current] = m->events_stat_lib->call(m);
	m->total = m->total+1; 
	/* Analyse samples */
	if(m->samples_stat_lib)
	    m->value = m->samples_stat_lib->call(m);
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

