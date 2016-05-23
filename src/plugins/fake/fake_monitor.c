#include "../../utils.h"

#define FAKE_NAME "FAKE_MONITOR"
typedef int* fake_eventset;

char ** monitor_events_list(int * n_events){
    char ** ret;
    *n_events = 1;
    malloc_chk(ret, sizeof(char*));
    *ret = strdup(FAKE_NAME);
    return ret;
}

int monitor_eventset_init(void ** monitor_eventset, __attribute__ ((unused)) hwloc_obj_t location, __attribute__ ((unused)) int accumulate){
    int * n_fake_events;
    malloc_chk(n_fake_events, sizeof(int));
    *n_fake_events = 0;
    *monitor_eventset = n_fake_events;
    return 0;
}

int monitor_eventset_destroy(void * eventset){free(eventset); return 0;}

int monitor_eventset_add_named_event(void * monitor_eventset, char * event)
{
    if(strcmp(event,FAKE_NAME))    
	return -1;
    *((int *) monitor_eventset) += 1;
    return 1;
}

int monitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){return 0;}
int monitor_eventset_start(__attribute__ ((unused)) void * monitor_eventset){return 1;}
int monitor_eventset_stop(__attribute__ ((unused)) void * monitor_eventset){return 1;}
int monitor_eventset_reset(__attribute__ ((unused)) void * monitor_eventset){return 1;}


int monitor_eventset_read(void * monitor_eventset, double * values){
  int i;
  for(i=0;i<*((int *)monitor_eventset);i++)
    values[i] = 1;
  return 0;
}

