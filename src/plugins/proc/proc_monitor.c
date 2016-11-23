#include <string.h>
#include "../../internal.h"
#include "../performance_plugin.h"

struct eventset{
  enum event_type * types;
  unsigned n_events;
  hwloc_obj_t location;
  struct proc_cpu * pcpu;
};
  
enum event_type {error, load_avg};
const enum event_type events[] = {load_avg};

const char * event_name(const enum event_type event){
  switch(event){
  case load_avg:
    return "load_avg";
  default:
    return NULL;
  }
}

enum event_type event_id(const char * name){
  enum event_type event = error;
  if(name == NULL) return event;
  if(!strcmp(name, "load_avg")) event = load_avg;
  return event;
}

char ** hmonitor_events_list(int * n_events){
    char ** ret;
    int i;
    
    *n_events = sizeof(events)/sizeof(*events);
    malloc_chk(ret, sizeof(char*) * (*n_events));
    for(i=0; i<(*n_events); i++)
      ret[i] = strdup(event_name(events[i]));
    return ret;
}

int hmonitor_eventset_init(void ** monitor_eventset, __attribute__ ((unused)) hwloc_obj_t location){
  int i, n_events = sizeof(events)/sizeof(*events);
  struct eventset * evset; malloc_chk(evset, sizeof(*evset));
  malloc_chk(evset->types, sizeof(*evset->types)*n_events);
  
  for(i=0; i<n_events; i++){ evset->types[i] = 0; }
  evset->n_events = 0;
  evset->location = location;
  evset->pcpu = new_proc_cpu(location);
  *monitor_eventset = evset;
  return 0;
}

int hmonitor_eventset_destroy(void * eventset){
  if(eventset == NULL) return -1;
  struct eventset * evset = (struct eventset *) eventset;
  free(evset->types);
  delete_proc_cpu(evset->pcpu);
  free(evset);
  return 0;
}

int hmonitor_eventset_add_named_event(void * monitor_eventset, const char * event)
{
  struct eventset * evset = (struct eventset *) monitor_eventset;
  evset->types[evset->n_events] = event_id(event);
  if(evset->types[evset->n_events] == error) return -1;
  evset->n_events++;
  return 1;
}

int hmonitor_eventset_init_fini(__attribute__ ((unused)) void * monitor_eventset){return 0;}
int hmonitor_eventset_start(__attribute__ ((unused)) void * monitor_eventset){return 1;}
int hmonitor_eventset_stop(__attribute__ ((unused)) void * monitor_eventset){return 1;}
int hmonitor_eventset_reset(__attribute__ ((unused)) void * monitor_eventset){return 1;}


int hmonitor_eventset_read(void * monitor_eventset, double * values){
  int i;
  struct eventset * evset = (struct eventset *) monitor_eventset;
  proc_cpu_read(evset->pcpu);
  for(i=0;i<evset->n_events;i++){
    switch(evset->types[i]){
    case load_avg:
      values[i] = proc_cpu_load(evset->pcpu);
      break;
    default:
      values[i] = 0;
      break;
    }
  }
  return 0;
}

