#include "../../monitor.h"
#include "../performance_plugin.h"
#include "monitor_trace_interface.h"
#include <string.h>

#define MONITOR_TRACE_COUNTER_MAX_LEN 16
#define MONITOR_TRACE_LINE_MAX_LEN 1024

void __attribute__((constructor)) monitor_events_global_init(){}
void __attribute__((destructor)) monitor_events_global_finalize(){}

struct monitor_trace_eventset{
    FILE * local_context;
    hwloc_obj_t location;
    int * event_field_indexes;
    int n_events, allocated;
    int accumulate;
};

char ** monitor_events_list(int * n_events){
    char ** event_list;
    *n_events=1;
    malloc_chk(event_list, sizeof(char *));
    *event_list = "integer value from first performance value field to last performance value field";
    return event_list;
}

int monitor_eventset_init(void ** monitor_eventset, hwloc_obj_t location, int accumulate){
    struct monitor_trace_eventset * evset;
    char * trace_path = getenv("MONITOR_TRACE_PATH");

    malloc_chk(evset, sizeof(*evset));
    *monitor_eventset = evset;
    if(trace_path == NULL){
	monitor_print_err("Set envirionment variable MONITOR_TRACE_PATH to load your trace with trace_monitor importer\n");
	return -1;
    }
    evset->local_context = fopen(trace_path,"r");
    if(evset->local_context == NULL){
	perror("fopen");
	return -1;
    }

    evset->accumulate = accumulate;
    evset->location = location;
    evset->n_events=0;
    evset->allocated = 4;
    malloc_chk(evset->event_field_indexes, sizeof(*evset->event_field_indexes)*evset->allocated);
    int i;
    for(i=0;i<evset->allocated;i++){
	evset->event_field_indexes[i] = 0;
    }
    monitor_trace_seek_begin(evset->local_context);
    return 0;
}

int monitor_eventset_destroy(void * eventset){
    struct monitor_trace_eventset * evset = (struct monitor_trace_eventset *)eventset;
    fclose(evset->local_context);
    free(evset->event_field_indexes);
    free(evset);
    return 1;
}

int monitor_eventset_add_named_event(void * monitor_eventset, char * event){
    struct monitor_trace_eventset * evset = (struct monitor_trace_eventset *)monitor_eventset;  
    int e = atoi(event);
    if(e >= evset->allocated){
	int old_allocated = evset->allocated;
	while(e>=evset->allocated) evset->allocated*=2;
	realloc_chk(evset->event_field_indexes, sizeof(*evset->event_field_indexes)*evset->allocated);
	for(; old_allocated<evset->allocated; old_allocated++)
	    evset->event_field_indexes[old_allocated]=0;
    }
    evset->event_field_indexes[e]=1;
    evset->n_events++;
    return 1;
}

int monitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){return 0;}
int monitor_eventset_start(__attribute__ ((unused)) void * monitor_eventset){return 0;}
int monitor_eventset_stop(__attribute__ ((unused)) void * monitor_eventset){return 0;}
int monitor_eventset_reset(__attribute__ ((unused)) void * monitor_eventset){return 0;}

int monitor_eventset_read(void * monitor_eventset, long long * values){
    struct monitor_trace_eventset * evset = (struct monitor_trace_eventset *)monitor_eventset;
    long timestamp;
    double vals[evset->n_events];
    char location[32]; memset(location,'a',32); location[31] = '\0';
    if(monitor_trace_parse_next_entry(evset->local_context, location, &timestamp, vals, evset->event_field_indexes)==-1)
	return -1; //end of trace
    if(monitor_trace_match_location_hwloc_obj(location, evset->location)){
    	if(evset->accumulate){
    	    int i;
    	    for(i=0;i<evset->n_events;i++)
    		values[i] += vals[i];
    	}
    	else
    	    memcpy(values, vals, sizeof(*values)*evset->n_events);
    	return 0;
    }
    else{
	return monitor_eventset_read(monitor_eventset,values);
    }
}

