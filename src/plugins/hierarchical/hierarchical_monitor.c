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
  unsigned n = 0, depth = hwloc_topology_get_depth(topology);
  char ** names = malloc(sizeof(*names) * depth); 
  *n_events=0;

  for(unsigned i=0; i<depth; i++){
    hwloc_obj_t obj = hwloc_get_obj_by_depth(topology, i, 0);
    harray _monitors = obj->userdata;
    if(_monitors != NULL){
      for(unsigned j = 0; j<harray_length(_monitors); j++){
	hmon m  = harray_get(_monitors,i);
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
  hwloc_obj_t child_obj;
  unsigned i, n_events = 0, nbobjs;
  hmon m;
  if(event==NULL || monitor_eventset == NULL)
    return -1;

  /* Decend fist children to look if event exists */
  child_obj = set->location;
  while(child_obj != NULL){
    child_events = child_obj->userdata;
    if(child_events != NULL){
      for(i = 0; i<harray_length(child_events); i++){
	m  = harray_get(child_events,i);
	if(!strcmp(m->id, event)){
	  goto add_events;
	}
      }
    }
    child_obj = child_obj->first_child;
  }

  /* Exit failure */
  fprintf(stderr, "Unrecognized event name %s, expected defined monitor name, deeper than %s.\n", event, hwloc_type_name(set->location->type));
  return -1;

  /* Add events to eventset */
add_events:;
  nbobjs = hwloc_get_nbobjs_inside_cpuset_by_depth(topology, set->location->cpuset, child_obj->depth);
  for(unsigned j = 0; j<nbobjs; j++){
    child_events = hwloc_get_obj_inside_cpuset_by_depth(topology, set->location->cpuset, child_obj->depth, j)->userdata;
    m = harray_get(child_events, i);
    harray_push(set->child_events, m);
    n_events+=m->n_samples;
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
  struct hierarchical_eventset * set = (struct hierarchical_eventset *) monitor_eventset;
  for(unsigned i = 0; i< harray_length(set->child_events); i++){
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
    hmonitor_read(m);
    hmonitor_reduce(m);
    for(i=0;i<m->n_samples;i++){values[i+offset] = m->samples[i];}
    offset+=i;
  }
  return 0;
}

