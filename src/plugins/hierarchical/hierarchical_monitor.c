#include "../performance_plugin.h"
#include "../../monitor.h"

/**
 * Eventset: set of monitors on object of the same type, below a common location.
 * Event: hwloc_obj_type.
 **/

struct hierarchical_eventset{
    hwloc_obj_t location;
    struct hmon_array * child_events;
};


char ** monitor_events_list(int * n_events){
    unsigned n = 0, depth = monitors_topology_depth;
    char ** names = malloc(sizeof(*names) * depth); 
    *n_events=0;

    for(unsigned i = 0; i< hmon_array_length(monitors); i++){
	struct monitor * m  = hmon_array_get(monitors,i);
	if(m->location->depth < depth){
	    depth = m->location->depth;
	    names[n++] = strdup(hwloc_type_name(m->location->type));
	}
    }

    *n_events = n;
    if(n==0){
	free(names);
	return NULL;
    }
    return names;
}

int monitor_eventset_init(void ** monitor_eventset, hwloc_obj_t location, __attribute__ ((unused)) int accumulate){
    struct hierarchical_eventset *  set;
    malloc_chk(set, sizeof(*set));
    set->location = location;
    set->child_events =  new_hmon_array(sizeof(struct monitor *), 4, NULL);
    *monitor_eventset = (void *)set;
    return 0;
}

int monitor_eventset_destroy(void * eventset){
    if(eventset == NULL)
	return 0;
    struct hierarchical_eventset * set = (struct hierarchical_eventset *) eventset;
    delete_hmon_array(set->child_events);
    free(set);
    return 1;
}

int monitor_eventset_add_named_event(void * monitor_eventset, char * event)
{
    struct hierarchical_eventset * set = (struct hierarchical_eventset *) monitor_eventset;
    hwloc_obj_type_t type;

    if(event==NULL || monitor_eventset == NULL)
	return -1;
    if(hwloc_type_sscanf(event, &type, NULL, 0) == -1){
	fprintf(stderr, "Unrecognized event name %s\n", event);
	return -1;
    }
    
    if(hmon_array_length(set->child_events) > 0){
	struct monitor * child_event = hmon_array_get(set->child_events,0);
	fprintf(stderr, "Event %s already exist at location %s:%d.\n", 
		hwloc_type_name(child_event->location->type), hwloc_type_name(set->location->type), set->location->logical_index);
	fprintf(stderr, "Hierarchical monitor does not allow to add several events.\n");
	return -1;
    }

    unsigned nbobjs = hwloc_get_nbobjs_inside_cpuset_by_type(monitors_topology, set->location->cpuset, type);
    for(unsigned i = 0; i < nbobjs; i++){
	hwloc_obj_t obj = hwloc_get_obj_inside_cpuset_by_type(monitors_topology, set->location->cpuset, type, i);
	hmon_array_push(set->child_events,obj->userdata);
    }
    return nbobjs;
}

int monitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){
    return 0;
}

/* 
 * There is nothing to do to start the eventset. 
 * Child monitor are not started here to avoid start twice if the user already did it 
 */
 int monitor_eventset_start(__attribute__ ((unused)) void * monitor_eventset){
    return 0;
}

/* 
 * There is nothing to do to stop the eventset. 
 * Child monitor are not started here to avoid start twice if the user already did it 
 */
int monitor_eventset_stop(__attribute__ ((unused)) void * monitor_eventset){  
  return 0;
}


/**
 * Reset to default events, samples, values ...
 **/
extern void monitor_reset(struct monitor *);

int monitor_eventset_reset(void * monitor_eventset){
    struct hierarchical_eventset * set = (struct hierarchical_eventset *) monitor_eventset;
    for(unsigned i = 0; i< hmon_array_length(set->child_events); i++){
	struct monitor * m  = hmon_array_get(set->child_events,i);
	monitor_reset(m);
    }
    return 0;
}

int monitor_eventset_read(void * monitor_eventset, long long * values){
    struct hierarchical_eventset * set = (struct hierarchical_eventset *) monitor_eventset;
    /* Child are updated before parents */
    /* pthread_mutex_lock(&(m->available)); */
    for(unsigned i = 0; i< hmon_array_length(set->child_events); i++){
	values[i] = ((struct monitor *)hmon_array_get(set->child_events,i))->value;
    }
    /* Child are updated before parents */
    /* pthread_mutex_unlock(&(m->available)); */
    return 0;
}

