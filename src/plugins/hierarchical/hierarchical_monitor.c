#include "../../hmon.h"
#include "../../internal.h"
#include <string.h>

/**
 * Eventset: set of monitors on object of the same type, below a common location.
 * Event: hwloc_obj_type.
 **/

struct hierarchical_eventset{
  hwloc_obj_t location;
  harray child_events;
};


char ** hmonitor_events_list(int * n_events){
  unsigned i, j, n = 0, depth = hwloc_topology_get_depth(hmon_topology);
  char ** names = malloc(sizeof(*names) * depth * 100); 
  *n_events=0;

  for(i=0; i<depth; i++){
    hwloc_obj_t obj = hwloc_get_obj_by_depth(hmon_topology, i, 0);
    harray _monitors = obj->userdata;
    if(_monitors != NULL){
      for(j = 0; j<harray_length(_monitors); j++){
	hmon m  = harray_get(_monitors,j);
	names[n++] = strdup(m->id);
      }
    }
  }
  *n_events = n;

  if(n==0){
    free(names);
    return NULL;
  }
  return names;
}

int hmonitor_eventset_init(void ** monitor_eventset, hwloc_obj_t location){
  struct hierarchical_eventset *  set;
  malloc_chk(set, sizeof(*set));
  set->location = location;
  set->child_events =  new_harray(sizeof(hmon), 4, NULL);
  *monitor_eventset = (void *)set;
  return 0;
}

int hmonitor_eventset_destroy(void * eventset){
  if(eventset == NULL)
    return 0;
  struct hierarchical_eventset * set = (struct hierarchical_eventset *) eventset;
  delete_harray(set->child_events);
  free(set);
  return 1;
}

int hmonitor_eventset_add_named_event(void * monitor_eventset, const char * event)
{
  struct hierarchical_eventset * set = (struct hierarchical_eventset *) monitor_eventset;
  harray child_events = NULL;
  hwloc_obj_t obj;
  unsigned depth,index,i, n_events = 0;
  hmon m;
  
  if(event==NULL || monitor_eventset == NULL)
    return -1;

  /* Decend depth by depth to look if event exists */
  int topo_depth = hwloc_topology_get_depth(hmon_topology);
  for(depth=set->location->depth; depth<topo_depth; depth++){
    /* Traverse depth to find matching event */
    for(index=0; index<hwloc_get_nbobjs_inside_cpuset_by_depth(hmon_topology, set->location->cpuset, depth); index++){
      obj = hwloc_get_obj_inside_cpuset_by_depth(hmon_topology, set->location->cpuset, depth, index);
      child_events = obj->userdata;
      if(child_events != NULL){
	/* Walk monitor list at this location to find matching event */
	for(i=0; i<harray_length(child_events); i++){
	  m  = harray_get(child_events,i);
	  if(!strcmp(m->id, event)){
	    harray_push(set->child_events, m);
	    n_events+=m->n_samples;
	  }
	}
      }
    }
  }
  
  if(n_events  == 0){
    /* Exit failure */
    fprintf(stderr, "Unrecognized event name %s, expected defined monitor name, deeper than %s.\n", event, hwloc_type_name(set->location->type));
    return -1;
  }

  return n_events;
}

int hmonitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){
  return 0;
}

/* 
 * There is nothing to do to start the eventset. 
 * Child monitor are not started here to avoid start twice if the user already did it 
 */
int hmonitor_eventset_start(__attribute__ ((unused)) void * monitor_eventset){
  return 0;
}

/* 
 * There is nothing to do to stop the eventset. 
 * Child monitor are not started here to avoid start twice if the user already did it 
 */
int hmonitor_eventset_stop(__attribute__ ((unused)) void * monitor_eventset){  
  return 0;
}

int hmonitor_eventset_reset(void * monitor_eventset){
  unsigned i;
  struct hierarchical_eventset * set = (struct hierarchical_eventset *) monitor_eventset;
  for(i = 0; i< harray_length(set->child_events); i++){
    hmon m  = harray_get(set->child_events,i);
    hmonitor_reset(m);
  }
  return 0;
}
    
int hmonitor_eventset_read(void * monitor_eventset, double * values){
  struct hierarchical_eventset * set = (struct hierarchical_eventset *) monitor_eventset;
  unsigned i,j, offset = 0;
  hmon m;
  for(j=0; j<harray_length(set->child_events); j++){
    m = harray_get(set->child_events, j);
    /* make sure m is up to date */
    if(hmonitor_trylock(m, 1) == 1){
      hmonitor_read(m);
      hmonitor_reduce(m);
      hmonitor_release(m);
    }
    for(i=0;i<m->n_samples;i++){values[i+offset] = m->samples[i];}
    offset+=i;
  }
  return 0;
}

