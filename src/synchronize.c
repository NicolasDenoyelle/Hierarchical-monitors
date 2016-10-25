#include <pthread.h>
#include "./hmon/hmonitor.h"
#include "./hmon.h"
#include "./internal.h"


#define hmonitors_do(hmons, call, ...)					\
  do{unsigned i; for(i = 0; i< harray_length(hmons); i++){call(harray_get(hmons,i), ##__VA_ARGS__);}}while(0)

harray                     monitors;                 /* The array of monitors */
hwloc_topology_t           hmon_topology;            /* The topology with monitor on hwloc_obj_t->userdata */
static FILE *              output;                   /* The output where to write the trace */
hwloc_cpuset_t             allowed_cpuset;           /* The domain monitored */

/** Monitors threads **/
static unsigned            ncores;                   /* Number of cores inside restrict location */
static int                 uptodate;                 /* Number of threads uptodate */
static unsigned            thread_count;             /* Number of threads */
static pthread_t *         threads;                  /* Threads id */
static int                 threads_stop = 0;
static pthread_barrier_t   barrier;                 /* Common barrier between monitors' thread and main thread */
static void *              hmonitor_thread(void * arg);

int hmon_import_hmonitors(const char * path){
  return hmon_import(path, allowed_cpuset);
}

int hmon_compare(void* hmonitor_a, void* hmonitor_b){
  hmon a = *((hmon*) hmonitor_a);
  hmon b = *((hmon*) hmonitor_b);
  
  if(a->location->depth>b->location->depth){return -1;}
  if(a->location->depth<b->location->depth){return 1;}
  if(a->location->logical_index<b->location->logical_index){return -1;}
  if(a->location->logical_index<b->location->logical_index){return 1;}
  if(a->display && !b->display){return -1;}
  if(!a->display && b->display){return 1;}
  if(a->silent && !b->silent){return -1;}
  if(!a->silent && b->silent){return 1;}
  
  /* both monitors are at the same location and have same properties */
  return strcmp(a->id, b->id);
}

static void hmonitor_unregister_location(hwloc_obj_t location){
  unsigned i;
  for(i=0; i<harray_length(location->userdata); i++){
    harray_remove(monitors, harray_find(monitors, harray_get(location->userdata,i), hmon_compare));    
  }
  delete_harray(location->userdata);
  location->userdata = NULL;
}

void hmon_restrict(hwloc_cpuset_t domain){
  if(domain == NULL){return;}
  if(!hwloc_bitmap_isincluded(domain, allowed_cpuset)){
    fprintf(stderr, "forbidden restriction to a domain that is not included in current domain.\n");
    return;
  }
  hwloc_bitmap_and(allowed_cpuset, allowed_cpuset, domain);
  /* Now domain contain the cpuset to remove */
  hwloc_bitmap_andnot(domain, domain, allowed_cpuset);
  
  unsigned i,nobj;
  hwloc_obj_t remove_obj;
  harray remove_monitors;
  for(i=0;i<hwloc_topology_get_depth(hmon_topology);i++){
    nobj = hwloc_get_nbobjs_inside_cpuset_by_depth(hmon_topology, domain, i);
    while(nobj--){
      remove_obj = hwloc_get_obj_inside_cpuset_by_depth(hmon_topology, domain, i, nobj);
      if(remove_obj == NULL || remove_obj->userdata == NULL){continue;}
      hmonitor_unregister_location(remove_obj);
    }
  }
}

void hmon_restrict_pid(pid_t pid){
  hwloc_cpuset_t pid_domain = hwloc_bitmap_alloc();
  proc_get_allowed_cpuset(pid, pid_domain);
  hmon_restrict(pid_domain);
  hwloc_bitmap_free(pid_domain);
}

void hmon_restrict_pid_taskset(pid_t pid, int recurse){
  unsigned i;
  hmon m;
  hwloc_cpuset_t running_cpuset = hwloc_bitmap_alloc();
  proc_get_running_cpuset(pid, running_cpuset, recurse);
  for(i=0; i< harray_length(monitors); i++){
    m = harray_get(monitors, i);
    if(!hwloc_bitmap_intersects(m->location->cpuset, running_cpuset) && !m->stopped){
      m->eventset_stop(m->eventset);
      m->stopped = 1;
    }
  }
  hwloc_bitmap_free(running_cpuset);
}

int hmon_lib_init(hwloc_topology_t topo, char * out){
  unsigned i;
  /* Check hwloc version */
  if(hwloc_check_version_mismatch() != 0){return -1;}

  /* initialize topology */
  if(topo == NULL){
    hwloc_topology_init(&hmon_topology); 
    hwloc_topology_set_icache_types_filter(hmon_topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(hmon_topology);
  }
  else
    hwloc_topology_dup(&hmon_topology,topo);

  if(hmon_topology==NULL){
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
  monitors = new_harray(sizeof(hmon), 32, (void (*)(void *))delete_hmonitor);
  allowed_cpuset = hwloc_bitmap_dup(hwloc_topology_get_complete_cpuset((hmon_topology)));

  /* Create one thread per core */
  ncores = hwloc_get_nbobjs_by_type(hmon_topology, HWLOC_OBJ_CORE);
  pthread_barrier_init(&barrier, NULL, ncores+1);
  threads = malloc(sizeof(*threads)*ncores);
  for(i = 0; i<ncores; i++){
    hwloc_obj_t core = hwloc_get_obj_by_type(hmon_topology, HWLOC_OBJ_CORE, i);
    pthread_create(&(threads[i]), NULL, hmonitor_thread, (void*)(core));
  }
  
  return 0;
}


int hmon_register_hmonitor(hmon m, int silent, int display){
  if(m==NULL){return -1;}
  if(!hwloc_bitmap_isincluded(m->location->cpuset, allowed_cpuset)){return -1;}
  /* Make monitor printable (or not) */
  m->silent = silent;
  m->display = display;
  
  /* Add monitor to existing monitors*/
  harray_insert_sorted(monitors, m, hmon_compare);
  
  /* Store monitor on topology */
  if(m->location->userdata == NULL){m->location->userdata = new_harray(sizeof(m), 4, NULL);}
  harray_insert_sorted(m->location->userdata, m, hmon_compare);

  return 0;
}

void hmon_update(){
  /* Check that all monitors or uptodate */
  if(__sync_bool_compare_and_swap(&uptodate, 0, ncores)){
    /* Trigger monitors */
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);
  }
}

harray hmon_get_monitors_by_depth(unsigned depth, unsigned logical_index){
  hwloc_obj_t obj =  hwloc_get_obj_by_depth(hmon_topology, depth, logical_index);
  if(obj != NULL){return obj->userdata;}
  return NULL;
}

void hmon_lib_finalize(){
  unsigned i, j;
  /* Stop monitors */
  threads_stop = 1;
  pthread_barrier_wait(&barrier);
  pthread_barrier_wait(&barrier);
  for(i=0;i<thread_count;i++){pthread_join(threads[i],NULL);}
  /* Cleanup */
  fclose(output);
  free(threads);
  for(i=0; i<hwloc_topology_get_depth(hmon_topology); i++){
    for(j=0;j<hwloc_get_nbobjs_by_depth(hmon_topology,i);j++){
      hwloc_obj_t obj = hwloc_get_obj_by_depth(hmon_topology, i, j);
      if(obj->userdata){delete_harray(obj->userdata);}
    }
  }
  delete_harray(monitors);
  hwloc_bitmap_free(allowed_cpuset);
  hwloc_topology_destroy(hmon_topology);
}



static void hmon_stop_hmonitor(hmon m){
  /* Lock monitors. If lock cannot be acquired, it is beeing updated, then giveup */
  if(hmonitor_trylock(m, 0) == 1){
    /* Update only if necessary */
    hmonitor_stop(m);
    hmonitor_release(m);
  }
}

void hmon_start(){
  hmonitors_do(monitors, hmonitor_start);
}

void hmon_stop(){
  hmonitors_do(monitors, hmonitor_stop);
}

int hmon_is_uptodate(){
  return __sync_fetch_and_and(&uptodate, 0);
}

static inline int hmon_output_hmonitor(hmon m){
  hmonitor_output(m,output);
  return 0;
}

/* Update monitors from a location from children to */
static void hmon_update_location(hwloc_obj_t location, int recurse_down, int recurse_up, int (*update)(hmon)){
  if(location == NULL){return;}
  harray _monitors = location->userdata;
  if(_monitors != NULL){hmonitors_do(_monitors, update);}
  
  if(recurse_down){
    unsigned i;
    for(i=0; i<location->arity; i++){hmon_update_location(location->children[i], 1, 0, update);}
  }
  if(recurse_up){
    hmon_update_location(location->parent, 0, 1, update);
  }
}


static void * hmonitor_thread(void * arg)
{
  hwloc_obj_t Core = (hwloc_obj_t)(arg);
  /* Bind the thread */
  hwloc_obj_t PU = hwloc_get_obj_inside_cpuset_by_type(hmon_topology,
						       Core->cpuset,
						       HWLOC_OBJ_PU,
						       hwloc_get_nbobjs_inside_cpuset_by_type(hmon_topology,
											      Core->cpuset,
											      HWLOC_OBJ_PU) -1);
  location_cpubind(hmon_topology, PU); 

  /* Collect events */
hmon_thread_loop:
  /* Enter thread safe area */
  pthread_barrier_wait(&barrier);
  /* check for stop */
  if(threads_stop){goto hmon_thread_exit;}
  /* Sleep until next update */
  pthread_barrier_wait(&barrier);
    
  /* Stop event collection */
  hmon_update_location(Core, 1, 1, hmonitor_stop);
  /* Read monitors */
  hmon_update_location(Core, 1, 1, hmonitor_read);
  /* Analyze monitors */
  hmon_update_location(Core, 1, 1, hmonitor_reduce);
  /* output monitors */
  hmon_update_location(Core, 1, 1, hmon_output_hmonitor);
  /* Signal we are uptodate */
  __sync_fetch_and_sub(&uptodate, 1);
  /* Restart event collection */
  hmon_update_location(Core, 1, 1, hmonitor_start);
  goto hmon_thread_loop;

hmon_thread_exit:
  pthread_barrier_wait(&barrier);
  return NULL;  
}

