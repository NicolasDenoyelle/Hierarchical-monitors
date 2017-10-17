#include <string.h>
#include "../../internal.h"
#include "../performance_plugin.h"

enum event_type {error, cpuload, memload, memused, numa_local, numa_remote};
const enum event_type allowed_events[] = {cpuload, memload, memused, numa_local, numa_remote};

struct proc_cpu;
struct proc_cpu * new_proc_cpu   (hwloc_obj_t location);
void              delete_proc_cpu(struct proc_cpu * p);
int               proc_cpu_read  (struct proc_cpu * p);
double            proc_cpu_load  (struct proc_cpu * p);

struct proc_mem;
struct proc_mem * new_proc_mem   (hwloc_obj_t location);
void              delete_proc_mem(struct proc_mem * p);
int               proc_mem_read  (struct proc_mem * p);
double            proc_mem_load  (struct proc_mem * p);
double            proc_mem_used  (struct proc_mem * p);

struct proc_numa;
struct proc_numa * new_proc_numa   (hwloc_obj_t location);
void               delete_proc_numa(struct proc_numa * p);
int                proc_numa_read  (struct proc_numa * p);
double             proc_numa_local (struct proc_numa * p);
double             proc_numa_remote(struct proc_numa * p);

struct event{
  enum event_type type;
  void * proc_info;
};
  
struct eventset{
  hwloc_obj_t location;
  unsigned n_events;
  struct event * events;
};
  
const char * event_name(const enum event_type event){
  switch(event){
  case cpuload:
    return "cpuload";
  case memload:
    return "memload";
  case memused:
    return "memused";
  case numa_local:
    return "numa_local";
  case numa_remote:
    return "numa_remote";
  default:
    return NULL;
  }
}

enum event_type event_type(const char * name){
  enum event_type event = error;
  if(name == NULL) return event;
  if(!strcmp(name, "cpuload")) event = cpuload;
  if(!strcmp(name, "memload")) event = memload;
  if(!strcmp(name, "memused")) event = memused;
  if(!strcmp(name, "numa_local")) event = numa_local;
  if(!strcmp(name, "numa_remote")) event = numa_remote;
  return event;
}

char ** hmonitor_events_list(int * n_events){
    char ** ret;
    int i;
    *n_events = sizeof(allowed_events)/sizeof(*allowed_events);
    malloc_chk(ret, sizeof(char*) * (*n_events));
    for(i=0; i<(*n_events); i++)
      ret[i] = strdup(event_name(allowed_events[i]));
    return ret;
}

int hmonitor_eventset_init(void ** monitor_eventset, __attribute__ ((unused)) hwloc_obj_t location){
  int i, max_events = sizeof(allowed_events)/sizeof(*allowed_events);
  struct eventset * evset; malloc_chk(evset, sizeof(*evset));
  evset->n_events = 0;
  evset->location = location;
    
  malloc_chk(evset->events, sizeof(*evset->events)*max_events);
  for(i=0; i<max_events; i++){ evset->events[i].type = 0; evset->events[i].proc_info = NULL; }

  *monitor_eventset = evset;
  return 0;
}

int hmonitor_eventset_destroy(void * eventset){
  if(eventset == NULL) return -1;
  int i;
  struct eventset * evset = (struct eventset *) eventset;

  /* Delete events proc_info according to its type */
  for(i=0; i<evset->n_events; i++){
    switch(evset->events[i].type){
    case cpuload:
      delete_proc_cpu((struct proc_cpu *) evset->events[i].proc_info);
      break;
    case memload:
      delete_proc_mem((struct proc_mem *) evset->events[i].proc_info);
      break;
    case memused:
      delete_proc_mem((struct proc_mem *) evset->events[i].proc_info);
      break;
    case numa_local:
      delete_proc_numa((struct proc_numa *) evset->events[i].proc_info);
      break;
    case numa_remote:
      delete_proc_numa((struct proc_numa *) evset->events[i].proc_info);
      break;
    default:
      break;
    }
  }
  
  free(evset->events);
  free(evset);
  return 0;
}

int hmonitor_eventset_add_named_event(void * monitor_eventset, const char * event)
{
  struct eventset * evset = (struct eventset *) monitor_eventset;
  struct event * new_event = &(evset->events[evset->n_events]);
  new_event->type = event_type(event);
  if(new_event->type == error) return -1;
  
  switch(new_event->type){
  case cpuload:
    if((new_event->proc_info = new_proc_cpu(evset->location)) == NULL){return -1;} 
    break;
  case memload:
    if((new_event->proc_info = new_proc_mem(evset->location)) == NULL){return -1;} 
    break;
  case memused:
    if((new_event->proc_info = new_proc_mem(evset->location)) == NULL){return -1;} 
    break;
  case numa_local:
    if((new_event->proc_info = new_proc_numa(evset->location)) == NULL){return -1;} 
    break;
  case numa_remote:
    if((new_event->proc_info = new_proc_numa(evset->location)) == NULL){return -1;} 
    break;
  default:
    break;
  }
  
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
  for(i=0;i<evset->n_events;i++){
    void * proc_info = evset->events[i].proc_info;
    switch(evset->events[i].type){
    case cpuload:
      proc_cpu_read((struct proc_cpu*)proc_info);
      values[i] = proc_cpu_load((struct proc_cpu*)proc_info);
      break;
    case memload:
      proc_mem_read((struct proc_mem*)proc_info);
      values[i] = proc_mem_load((struct proc_mem*)proc_info);
      break;
    case memused:
      proc_mem_read((struct proc_mem*)proc_info);
      values[i] = proc_mem_used((struct proc_mem*)proc_info);
      break;
    case numa_local:
      proc_numa_read((struct proc_numa*)proc_info);
      values[i] = proc_numa_local((struct proc_numa*)proc_info);
      break;
    case numa_remote:
      proc_numa_read((struct proc_numa*)proc_info);
      values[i] = proc_numa_remote((struct proc_numa*)proc_info);
      break;
    default:
      values[i] = 0;
      break;
    }
  }
  return 0;
}

