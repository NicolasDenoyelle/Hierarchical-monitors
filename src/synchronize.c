#include <pthread.h>

#include <hmon/hmonitor.h>
#include <hmon.h>
#include "internal.h"


#define hmonitors_do(hmons, call, ...)					\
  do{for(unsigned i = 0; i< harray_length(hmons); i++){call(harray_get(hmons,i), ##__VA_ARGS__);}}while(0)

harray                     monitors;                 /* The array of monitors */
static harray              monitors_to_print;        /* The array of monitors to print */
static harray              monitors_to_display;      /* The array of monitors to display on topology */

hwloc_topology_t           topology;                 /* The topology with monitor on hwloc_obj_t->userdata */
static FILE *              output;                   /* The output where to write the trace */

/** Monitors threads **/
static hwloc_obj_t         restrict_location;        /* The location where to spawn threads */
static unsigned            ncores;                   /* Number of cores inside restrict location */
harray*                    core_monitors;            /* An array of monitor per core in restrict location */
static unsigned            thread_count;     /* Number of threads */
static pthread_t *         threads;          /* Threads id */
static pthread_barrier_t   barrier;  /* Common barrier between monitors' thread and main thread */
static void *              hmonitor_thread(void * arg);

static int hmon_compare(void* hmonitor_a, void* hmonitor_b){
  hmon a = *((hmon*) hmonitor_a);
  hmon b = *((hmon*) hmonitor_b);
  
  if(a->location->depth>b->location->depth){return -1;}
  if(a->location->depth<b->location->depth){return 1;}
  if(a->location->logical_index<b->location->logical_index){return -1;}
  if(a->location->logical_index<b->location->logical_index){return 1;}

  /* both monitors are at the same location */
  return strcmp(a->id, b->id);
}

static void hmonitor_restrict(hmon m, hwloc_cpuset_t allowed_cpuset, int delete){
  if(!hwloc_bitmap_intersects(m->location->cpuset, allowed_cpuset)){
    if(!m->stopped){
      m->eventset_stop(m->eventset);
      m->stopped = 1;
    }
    if(delete){
      harray_remove(monitors_to_print, harray_find(monitors_to_print, m, hmon_compare));
      harray_remove(monitors, harray_find(monitors, m, hmon_compare));
      delete_hmonitor(m);
    }  /* Sort monitors per location for update from leaves to root */
  harray_sort(monitors, hmon_compare);
  harray_sort(monitors_to_print, hmon_compare);

  }
}

void hmon_restrict_pid(pid_t pid){
  hwloc_cpuset_t allowed_cpuset = hwloc_bitmap_alloc();
  proc_get_allowed_cpuset(pid, allowed_cpuset);
  hmonitors_do(monitors, hmonitor_restrict, allowed_cpuset, 1);
  hwloc_bitmap_free(allowed_cpuset);
}

void hmon_restrict_pid_running_tasks(pid_t pid, int recurse){
  hwloc_cpuset_t running_cpuset = hwloc_bitmap_alloc();
  proc_get_running_cpuset(pid, running_cpuset, recurse);
  hmonitors_do(monitors, hmonitor_restrict, running_cpuset, 0);
  hwloc_bitmap_free(running_cpuset);
}

int hmon_lib_init(hwloc_topology_t topo, const char* restrict_obj, char * out){
  /* Check hwloc version */
  if(hwloc_check_version_mismatch() != 0){return -1;}

  /* initialize topology */
  if(topo == NULL){
    hwloc_topology_init(&topology); 
    hwloc_topology_set_icache_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(topology);
  }
  else
    hwloc_topology_dup(&topology,topo);

  if(topology==NULL){
    fprintf(stderr, "Failed to init topology\n");
    return -1;
  }
    
  if(out){
    output = fopen(out, "w");
    if(output == NULL){
      perror("fopen");
      output = stdout;
    }
  }
  else{output = stdout;}

  /* create or monitor list */ 
  monitors = new_harray(sizeof(hmon), 32, NULL);
  monitors_to_print = new_harray(sizeof(hmon), 32, NULL);
  monitors_to_display = new_harray(sizeof(hmon), 32, NULL);
  
  /* Restrict topology to first group and set an array of monitor per core */
  if(restrict_obj == NULL){restrict_location = hwloc_get_root_obj(topology);}
  else{restrict_location = location_parse(topology,restrict_obj);}
  if(restrict_location == NULL){
    fprintf(stderr, "Warning: restrict location %s cannot be set, root is set instead", restrict_obj);
    restrict_location = hwloc_get_root_obj(topology);
  }

  /* Create one thread per core */
  ncores = hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_CORE);
  malloc_chk(core_monitors, sizeof(*core_monitors) * ncores);
  for(unsigned i=0; i<ncores; i++){core_monitors[i] = new_harray(sizeof(hmon), hwloc_topology_get_depth(topology), NULL);}
  pthread_barrier_init(&barrier, NULL, ncores+1);
  threads = malloc(sizeof(*threads)*ncores);
  for(unsigned i = 0; i<ncores; i++){
    hwloc_obj_t core = hwloc_get_obj_inside_cpuset_by_type(topology, restrict_location->cpuset, HWLOC_OBJ_CORE, i);
    pthread_create(&(threads[i]), NULL, hmonitor_thread, (void*)(core));
  }
  /* Wait threads initialization */
  pthread_barrier_wait(&barrier);
  
  return 0;
}

/* 
 * Host must be a core where to spawn a thread 
 * Host must be into the restricted area, otherwise overhead of pthread_barrier is too high.
 */
static unsigned hmon_find_core_host(hmon m){
  hwloc_obj_t near = m->location;
  /* match near to be in restricted topology */
  int cousins = get_max_objs_inside_cpuset_by_type(topology, restrict_location->cpuset, near->type);
  /* near type is deeper than restricted topology root */
  if(cousins > 0)
    near = hwloc_get_obj_inside_cpuset_by_depth(topology, restrict_location->cpuset, near->depth, near->logical_index%cousins);
  /* near is above restricted topology */
  else 
    near = restrict_location;

  hwloc_obj_t host = NULL;
  /* If child of core then host is the parent core */
  if(near->type == HWLOC_OBJ_PU){host = near->parent;}
  /* If core then host is the matching core in group */
  else if(near->type == HWLOC_OBJ_CORE){host = near;}
  /* Other wise we choose the first child on Core leaves */
  else {host = hwloc_get_obj_inside_cpuset_by_type(topology, near->cpuset, HWLOC_OBJ_CORE, 0);}

  /* if no hmon.have ever been hosted here, a thread will be spawned */
  unsigned core_idx = host->logical_index%ncores;
  if(harray_length(core_monitors[core_idx]) == 0)
    thread_count++;
  /* printf("will be hosted on %s:%d\n",hwloc_type_name(host->type), host->logical_index); */
  return harray_insert_sorted(core_monitors[core_idx], m, hmon_compare);
}


void hmon_register_hmonitor(hmon m, int silent, int display){
  if(m==NULL){return;}
  
  /* push monitor to array holding monitor on Core */
  hmon_find_core_host(m);
  
  /* Add monitor to existing monitors*/
  harray_insert_sorted(monitors, m, hmon_compare);

  /* Add monitor to monitors to print list */
  if(!silent){harray_push(monitors_to_print, m);}

  /* Add monitor to monitors to display list */
  if(display){
    int key = harray_find(monitors_to_display, m, location_compare);
    if(key == -1)
      harray_insert_sorted(monitors_to_display, m, location_compare);
    else{
      hmon displayed = harray_get(monitors_to_display, key);
      fprintf(stderr, "Monitor %s required to be displayed on location %s:%d, but monitor %s is already displayed here",
	      m->id, hwloc_type_name(m->location->type), m->location->logical_index, displayed->id);
    }
  }

  /* Store monitor on topology */
  if(m->location->userdata == NULL){m->location->userdata = new_harray(sizeof(m), 4, NULL);}
  harray_push(m->location->userdata, m);
}

void hmon_start(){
  hmonitors_do(monitors, hmonitor_start);
}

void hmon_stop(){
  hmonitors_do(monitors, hmonitor_stop);
}

void hmon_update(){
  /* make Monitors busy */
  for(unsigned i = 0; i<harray_length(monitors); i++){
    hmon m = harray_get(monitors,i);
    pthread_mutex_lock(&m->available);
  }
  /* Trigger monitors */
  pthread_barrier_wait(&barrier);

  /* Output monitors */
  hmonitors_do(monitors_to_print, hmonitor_output, output, 1);
}

void monitor_lib_finalize(){
  unsigned i;
  /* Stop monitors */
  for(i=0;i<thread_count;i++)
    pthread_cancel(threads[i]);
  pthread_barrier_wait(&barrier);
  for(i=0;i<thread_count;i++)
    pthread_join(threads[i],NULL);
    
  /* Cleanup */
  fclose(output);
  free(core_monitors);
  delete_harray(monitors);
  delete_harray(monitors_to_print);
  delete_harray(monitors_to_display);
  free(threads);
  hwloc_topology_destroy(topology);
}


static void _monitor_read_then_reduce(harray a){
  unsigned i;
  /* Read monitors */
  hmonitors_do(a, hmonitor_read);
  /* Analyze monitors */
  hmonitors_do(a, hmonitor_reduce);
}

static void _monitor_thread_cleanup(void * arg){
  harray _monitors = (harray)arg;
  hmonitors_do(_monitors, hmonitor_stop);
  hmonitors_do(_monitors, delete_hmonitor);
  delete_harray(_monitors);    
}

static void * hmonitor_thread(void * arg)
{
  hwloc_obj_t Core = (hwloc_obj_t)(arg);
  /* Bind the thread */
  hwloc_obj_t PU = hwloc_get_obj_inside_cpuset_by_type(topology, Core->cpuset, HWLOC_OBJ_PU, hwloc_get_nbobjs_inside_cpuset_by_type(topology, Core->cpuset, HWLOC_OBJ_PU) -1);
  location_cpubind(topology, PU); 
  location_membind(topology, PU);
  harray _monitors = core_monitors[Core->logical_index%ncores];
  /* push clean method */
  pthread_cleanup_push(_monitor_thread_cleanup, _monitors);
  /* Wait other threads initialization */
  pthread_barrier_wait(&barrier);

  /* Collect events */
monitor_start:
  hmonitors_do(_monitors, hmonitor_start);
  pthread_barrier_wait(&barrier);
  pthread_testcancel();
  hmonitors_do(_monitors, hmonitor_stop);
  _monitor_read_then_reduce(_monitors);
  goto monitor_start;

  pthread_cleanup_pop(1);
  return NULL;  
}

