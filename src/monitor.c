#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include "monitor.h"
#include "performance.h"
#include "stats.h"

struct array *             monitors;
static struct array *      monitors_to_print;
hwloc_topology_t           monitors_topology;
unsigned                   monitors_topology_depth;
static unsigned long       monitors_pid;
static FILE *              monitors_output;
static struct timespec     monitors_start_time, monitors_current_time;
hwloc_cpuset_t             monitors_running_cpuset;

static unsigned            monitor_thread_count;
static pthread_t *         monitor_threads;
static pthread_barrier_t   monitor_threads_barrier;
static volatile int        monitor_threads_stop = 0;

static void   _monitor_delete(struct monitor*);
static void * _monitor_thread(void*);
static void   _monitor_remove(struct monitor*);
static void   _monitor_read  (struct monitor*);
static void   _monitor_output (struct monitor*, int);
static int    _monitors_start(struct array*);
static int    _monitors_stop (struct array*);
static void   _monitors_read (struct array*);
static void   _monitors_output(int);
static int    _monitor_location_compare(void*, void*);

void monitor_reset(struct monitor * monitor){
unsigned i, j;
   monitor->value = 0;
   monitor->min_value = 0;
   monitor->max_value = 0;
   monitor->current = monitor->n_samples-1;
   monitor->stopped = 1;
   for(i=0;i<monitor->n_samples;i++){
       monitor->samples[i] = 0;
       for(j = 0; j < monitor->n_events; j++)
	   monitor->events[i][j] = 0;
   }
}

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
    /* Check if location is available to store this monitor */
    if(location->userdata != NULL)
	return NULL;

    /* record at least 3 samples */
    n_samples = 3 > n_samples ? 2 : n_samples; 
	
    unsigned i;
    struct monitor * monitor;

    malloc_chk(monitor, sizeof(*monitor));  

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
    monitor->userdata = NULL;
    pthread_mutex_init(&(monitor->available), NULL);
    
    /* allocate arrays */
    malloc_chk(monitor->events, n_samples*sizeof(*(monitor->events)));
    malloc_chk(monitor->samples, n_samples*sizeof(*(monitor->samples)));
    for(i=0;i<monitor->n_samples;i++)
	malloc_chk(monitor->events[i], n_events*sizeof(*(monitor->events[i])));
    
    /* reset values */
    monitor_reset(monitor);

    /* Add monitor to existing monitors*/
    array_push(monitors, monitor);
    if(!silent){array_push(monitors_to_print, monitor);}
    return monitor;
}

/* static void cpuset_print(hwloc_cpuset_t c){ */
/*     int i; */
/*     for(i=0; i<hwloc_bitmap_weight(c); i ++){ */
/* 	printf("%d", hwloc_bitmap_isset(c,i)); */
/*     } */
/*     printf("\n"); */
/* } */

int monitors_restrict(pid_t pid){
    struct monitor * m;
    /* If the process is not running, return 0 */
    if(kill(pid,0)!=0){
	return 0;
    }
    monitors_pid = pid;
    proc_get_allowed_cpuset(pid, monitors_running_cpuset);
    while((m = array_iterate(monitors)) != NULL){
	/* cpuset_print(m->location->cpuset); */
	/* cpuset_print(monitors_running_cpuset); */
	/* printf("\n"); */
	if(!hwloc_bitmap_intersects(m->location->cpuset, monitors_running_cpuset)){
	    array_remove(monitors_to_print, array_find(monitors_to_print, m, _monitor_location_compare));
	    array_remove(monitors,       array_find(monitors, m, _monitor_location_compare));
	}
    }
    return 1;
}

void monitors_register_threads(int recurse){
    if(monitors_pid == 0){
	monitors_pid = getpid();
	monitors_restrict(monitors_pid);
    }
    proc_get_running_cpuset(monitors_pid, monitors_running_cpuset, recurse);
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
	monitors_output = fopen(output, "w");
	if(monitors_output == NULL){
	    perror("fopen");
	    monitors_output = stdout;
	}
    }
    else
	monitors_output = stdout;


    if((monitors_running_cpuset = hwloc_bitmap_alloc()) == NULL){
	fprintf(stderr,"Allocation failed\n");
	exit(EXIT_FAILURE);
    }
    monitors_running_cpuset = hwloc_bitmap_dup(hwloc_get_root_obj(monitors_topology)->cpuset);

    /* create or monitor list */ 
    monitors = new_array(sizeof(struct monitor *), 32, (void (*)(void*))_monitor_delete);
    monitors_to_print = new_array(sizeof(struct monitor *), 32, NULL);

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
    for(i=0;i<monitor->n_events;i++)
	free(monitor->events[i]);
    free(monitor->events);
    free(monitor->samples);
    pthread_mutex_destroy(&(monitor->available));
    free(monitor);
}

void monitors_start(){
    unsigned i, n = 0, n_leaves = hwloc_get_nbobjs_by_type(monitors_topology, HWLOC_OBJ_PU);
    hwloc_obj_t obj, leaf;
    struct monitor * m;

    /* Sort monitors per location for update from leaves to root */
    array_sort(monitors, _monitor_location_compare);
    array_sort(monitors_to_print, _monitor_location_compare);

    /* Clear leaves */
    for(i=0;i<n_leaves; i++){
	hwloc_get_obj_by_type(monitors_topology, HWLOC_OBJ_PU, i)->userdata = NULL;
    }
	
    /* Count required threads and balance monitors on their child leaves */
    while((m = array_iterate(monitors)) != NULL){
	obj = hwloc_get_obj_inside_cpuset_by_type(monitors_topology, m->location->cpuset, HWLOC_OBJ_PU, 0);
	leaf = NULL;
	while((leaf = hwloc_get_next_obj_inside_cpuset_by_type(monitors_topology, m->location->cpuset, HWLOC_OBJ_PU, leaf)) != NULL){
	    if(leaf->userdata == NULL){
		leaf->userdata = new_array(sizeof(m), monitors_topology_depth, NULL);
		monitor_thread_count++;
		break;
	    }
	    if(array_length(leaf->userdata) < array_length(obj->userdata))
		obj = leaf;
	}
	array_push(obj->userdata,m);
    }

    /* Now we know the number of threads, we can initialize a barrier and an array of threads */
    pthread_barrier_init(&monitor_threads_barrier, NULL, monitor_thread_count+1);
    monitor_threads = malloc(sizeof(*monitor_threads) * monitor_thread_count);

    /* Then spawn threads on non NULL leaves userdata */
    for(i=0; i<n_leaves; i++){
	obj = hwloc_get_obj_by_type(monitors_topology, HWLOC_OBJ_PU, i);
	if(obj->userdata != NULL){
	    hwloc_obj_t t_obj = obj;  
	    pthread_create(&(monitor_threads[n]),NULL,_monitor_thread, (void*)(t_obj));
	    n++;
	}
    }

    /* Wait threads initialization */
    pthread_barrier_wait(&monitor_threads_barrier);

    /* reset userdata at leaves with monitor */
    while((m = array_iterate(monitors)) != NULL){
	/* monitors are sorted from leaves to root */
	if(m->location->type != HWLOC_OBJ_PU)
	    break;
	m->location->userdata = m;
    }


    /* Finally start the timer */
    clock_gettime(CLOCK_MONOTONIC, &monitors_start_time);
}

static void _monitor_remove(struct monitor * monitor){
    array_remove(monitors, array_find(monitors, monitor, _monitor_location_compare));
    _monitor_delete(monitor);
}

void monitors_update(int mark){
    struct monitor * m;
    /* Make all monitors unavailable */
    while((m = array_iterate(monitors)) != NULL)
	pthread_mutex_lock(&(m->available));
    /* Trigger monitors */
    pthread_barrier_wait(&monitor_threads_barrier);
    /* Syna and fetch stop flag */
    pthread_barrier_wait(&monitor_threads_barrier);
    /* Print */
    clock_gettime(CLOCK_MONOTONIC, &monitors_current_time);
    _monitors_output(mark);
}

void monitor_lib_finalize(){
    unsigned i;
    /* Stop monitors */
    __sync_or_and_fetch(&monitor_threads_stop , 1);
    pthread_barrier_wait(&monitor_threads_barrier);
	
    for(i=0;i<monitor_thread_count;i++){
	pthread_join(monitor_threads[i],NULL);
    }

    /* Cleanup */
    delete_array(monitors);
    delete_array(monitors_to_print);
    free(monitor_threads);
    hwloc_bitmap_free(monitors_running_cpuset);
    hwloc_topology_destroy(monitors_topology);
}

static void _monitor_output(struct monitor * m, int mark){
    unsigned j, c;
    c = m->current;
    fprintf(monitors_output,"%s:%u ", hwloc_type_name(m->location->type), m->location->logical_index);
    fprintf(monitors_output,"%ld ", tspec_diff((&monitors_start_time), (&monitors_current_time)));
    pthread_mutex_lock(&(m->available));
    for(j=0;j<m->n_events;j++)
	fprintf(monitors_output,"%lf ", m->events[c][j]);
    if(m->events_stat_lib){
	fprintf(monitors_output,"%lf ", m->samples[c]);
	fprintf(monitors_output,"%lf ", m->min_value);
	fprintf(monitors_output,"%lf ", m->value);
	fprintf(monitors_output,"%lf ", m->max_value);
    }
    pthread_mutex_unlock(&(m->available));
    if(mark>=0)
	fprintf(monitors_output,"%d ",mark);
    fprintf(monitors_output,"\n");
}

static void _monitors_output(int mark){
    struct monitor * m;
    while((m = array_iterate(monitors_to_print)) != NULL)
	_monitor_output(m, mark);
}

static int _monitors_start(struct array * _monitors){
    struct monitor * m;
    while((m = array_iterate(_monitors)) != NULL){
	if(m->stopped && hwloc_bitmap_intersects(monitors_running_cpuset, m->location->cpuset)){
	    m->perf_lib->monitor_eventset_start(m->eventset);
	    m->stopped = 0;
	}
    }
    return 0;
}

static int _monitors_stop(struct array * _monitors){
    struct monitor * m;
    while((m = array_iterate(_monitors)) != NULL){
	if(!m->stopped){
	    m->perf_lib->monitor_eventset_stop(m->eventset);
	    m->stopped = 1;
	}
    }
    return 0;
}

static void _monitor_read(struct monitor * m){
    unsigned c;
    if(m==NULL){
	fprintf(stderr, "Read NULL monitor\n");
	return;
    }
    /* Check whether we have to update */
    if(hwloc_bitmap_intersects(monitors_running_cpuset, m->location->cpuset)){
	c = m->current = (m->current+1)%(m->n_samples);
	if((m->perf_lib->monitor_eventset_read(m->eventset,m->events[c])) == -1){
	    fprintf(stderr, "Failed to read counters from monitor on obj %s:%d\n",
		    hwloc_type_name(m->location->type), m->location->logical_index);
	    _monitor_remove(m);
	}
	if(m->events_stat_lib)
	    m->samples[c] = m->events_stat_lib->call(m);
	if(m->samples_stat_lib)
	    m->value = m->samples_stat_lib->call(m);
	else
	    m->value = m->samples[c];

	m->min_value  = MIN(m->min_value, m->value);
	m->max_value  = MAX(m->max_value, m->value);
	m->total      = m->total+1; 
    }
}

static void _monitors_read(struct array * _monitors){
    struct monitor * m;
    while((m = array_iterate(_monitors)) != NULL){
	_monitor_read(m);
	pthread_mutex_unlock(&(m->available));
    }
}

void * _monitor_thread(void * arg)
{
    hwloc_obj_t PU = (hwloc_obj_t)(arg);
    /* Bind the thread */
    location_cpubind(PU); 
    location_membind(PU); 
    struct array * _monitors = PU->userdata;

    /* Wait other threads initialization */
    pthread_barrier_wait(&monitor_threads_barrier);
    /* Collect events */
 monitor_start:
    _monitors_start(_monitors);
    pthread_barrier_wait(&monitor_threads_barrier);
    /* Check whether we have to finnish */
    if(__sync_fetch_and_and(&monitor_threads_stop,1)){
	_monitors_stop(_monitors);
	delete_array(_monitors);
	pthread_exit(NULL);
    }
    pthread_barrier_wait(&monitor_threads_barrier);
    _monitors_stop(_monitors);
    _monitors_read(_monitors);
    goto monitor_start;
    return NULL;  
}

