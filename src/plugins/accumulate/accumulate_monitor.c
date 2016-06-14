#include "../../hmon.h"

/**
 * Eventset: set of monitors on object of the same type, below a common location.
 * Event: hwloc_obj_type.
 **/

struct accumulate_eventset{
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
	    names[n++] = strdup(m->id);
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
    struct accumulate_eventset *  set;
    malloc_chk(set, sizeof(*set));
    set->location = location;
    set->child_events =  new_hmon_array(sizeof(struct monitor *), 4, NULL);
    *monitor_eventset = (void *)set;
    return 0;
}

int monitor_eventset_destroy(void * eventset){
    if(eventset == NULL)
	return 0;
    struct accumulate_eventset * set = (struct accumulate_eventset *) eventset;
    delete_hmon_array(set->child_events);
    free(set);
    return 1;
}

int monitor_eventset_add_named_event(void * monitor_eventset, char * event)
{
    struct accumulate_eventset * set = (struct accumulate_eventset *) monitor_eventset;
    struct monitor * child_event = NULL;
    hwloc_obj_t child_obj;
    
    if(event==NULL || monitor_eventset == NULL)
	return -1;

    /* Check eventset is empty */
    if(hmon_array_length(set->child_events) > 0){
        child_event = hmon_array_get(set->child_events,0);
	fprintf(stderr, "Event %s already exist at location %s:%d.\n", 
		child_event->id, hwloc_type_name(set->location->type), set->location->logical_index);
	fprintf(stderr, "Accumulate monitor does not allow to add several events.\n");
	return -1;
    }
    /* Look if event exists */
    child_obj = set->location->first_child;
    while(child_obj != NULL){
	child_event = child_obj->userdata;
	if(child_event != NULL && !strcmp(child_event->id, event))
	    break;
	child_obj = child_obj->first_child;
    }
    
    /* event does not exists*/
    if(child_obj == NULL || child_event==NULL || strcmp(child_event->id, event)){
	fprintf(stderr, "Unrecognized event name %s, expected defined monitor name, deeper than %s.\n", event, hwloc_type_name(set->location->type));
	return -1;
    }
    /* Event exists, add events */
    for(unsigned i = 0; i<hwloc_get_nbobjs_inside_cpuset_by_depth(monitors_topology, set->location->cpuset, child_obj->depth); i++){
	hmon_array_push(set->child_events,hwloc_get_obj_inside_cpuset_by_depth(monitors_topology, set->location->cpuset, child_obj->depth, i)->userdata);
    }
    
    return child_event->n_events;
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
    struct accumulate_eventset * set = (struct accumulate_eventset *) monitor_eventset;
    for(unsigned i = 0; i< hmon_array_length(set->child_events); i++){
	struct monitor * m  = hmon_array_get(set->child_events,i);
	monitor_reset(m);
    }
    return 0;
}

    
int monitor_eventset_read(void * monitor_eventset, long long * values){
    struct accumulate_eventset * set = (struct accumulate_eventset *) monitor_eventset;
    unsigned j,c;
    struct monitor * m = hmon_array_get(set->child_events,0);
    pthread_mutex_lock(&(m->available));
    c = m->current;
    for(j=0; j<m->n_events; j++){
	values[j] = m->events[c][j];
    }
    pthread_mutex_unlock(&(m->available));
    for(unsigned i = 1; i< hmon_array_length(set->child_events); i++){
	m  = hmon_array_get(set->child_events,i);
	pthread_mutex_lock(&(m->available));
	c = m->current;
	for(j=0; j<m->n_events; j++){
	    values[j] += m->events[c][j];
	}
	pthread_mutex_unlock(&(m->available));
    }

    return 0;
}

