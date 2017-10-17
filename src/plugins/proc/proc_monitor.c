#include "../performance_plugin.h"
#include "../../internal.h"
#include <unistd.h>
#include <sys/types.h>
#include <proc.h>

char * machine_events[9] = {"cpuload","minor_faults","major_faults","vsize","rss","blkio_ticks"};
char * node_events[5]    = {"memtotal","memfree","memused","local_hit","remote_hit"};
char * pu_events[4]      = {"cpuload","iowait","irq","softirq"};

#define MAX_EVENTS 16

struct proc_eventset{
  struct proc_stat* ps[3];    /* [old, current, diff] */
  struct proc_cpu*  pc[3];    /* [old, current, diff] */
  struct proc_mem*  pm;
  struct proc_numa* pn[3];    /* [old, current, diff] */
  double            cpu_load; /* Computed from ps or pc */
  double            min_flt ; /* Computed from ps */
  double            maj_flt ; /* Computed from ps */  
  unsigned          n_events;
  double *          events[MAX_EVENTS];   /* @dress of fields in proc_*[2] structs */
  hwloc_obj_t       location;
};
  

char ** hmonitor_events_list(int * n_counters){
  int i0,i1,i2;
  *n_counters = sizeof(machine_events)/sizeof(char*) + sizeof(node_events)/sizeof(char*) + sizeof(pu_events)/sizeof(char*);
  char ** events; malloc_chk(events, sizeof(*events)*(*n_counters));
  for(i0=0;i0<sizeof(machine_events)/sizeof(char*);i0++){
    events[i0] = strdup(machine_events[i0]);
  }
  for(i1=0;i1<sizeof(node_events)/sizeof(char*);i1++){
    events[i0+i1] = strdup(node_events[i1]);
  }
  for(i2=0;i2<sizeof(pu_events)/sizeof(char*);i2++){
    events[i0+i1+i2] = strdup(pu_events[i2]);
  }
  return events;
}

int hmonitor_eventset_init(void ** monitor_eventset, hwloc_obj_t location){
  struct proc_eventset * evset; malloc_chk(evset, sizeof(*evset));
  unsigned i;
  pid_t pid = getpid();
  evset->location = location;
  evset->ps[0] = evset->ps[1] = evset->ps[2] = NULL;
  evset->pc[0] = evset->pc[1] = evset->pc[2] = NULL;
  evset->pm = NULL;
  evset->pn[0] = evset->pn[1] = evset->pn[2] = NULL;
  evset->n_events = 0;
  for(i=0;i<MAX_EVENTS;i++){evset->events[i] = NULL;}
  evset->cpu_load = evset->min_flt = evset->maj_flt = 0;
  
  if(location->type == HWLOC_OBJ_PU){
    evset->pc[0] = new_proc_cpu(location->os_index);
    evset->pc[1] = new_proc_cpu(location->os_index);
    evset->pc[2] = new_proc_cpu(location->os_index);    
  } else if(location->type == HWLOC_OBJ_NUMANODE){
    
    evset->pm = new_proc_mem(location->logical_index);
    evset->pn[0] = new_proc_numa(location->logical_index);
    evset->pn[1] = new_proc_numa(location->logical_index);
    evset->pn[2] = new_proc_numa(location->logical_index);    
  } else if(location->type == HWLOC_OBJ_MACHINE){
    evset->ps[0] = new_proc_stat(pid);
    evset->ps[1] = new_proc_stat(pid);
    evset->ps[2] = new_proc_stat(pid);    
  } else {
    monitor_print_err("Bad monitor location %s provided. Plugin proc only accepts Machine, NUMANode, and PU locations.\n",
		      hwloc_type_name(location->type));
    return -1;
  }
  
  *monitor_eventset = evset;
  return 0;  
}


int hmonitor_eventset_destroy(void * eventset){
  struct proc_eventset * evset = (struct proc_eventset *)(eventset);
  if(evset->ps[0] != NULL){ delete_proc_stat(evset->ps[0]); evset->ps[0] = NULL; }
  if(evset->ps[1] != NULL){ delete_proc_stat(evset->ps[1]); evset->ps[1] = NULL; }
  if(evset->ps[2] != NULL){ delete_proc_stat(evset->ps[2]); evset->ps[2] = NULL; }  

  if(evset->pc[0] != NULL){ delete_proc_cpu(evset->pc[0]); evset->pc[0] = NULL; }
  if(evset->pc[1] != NULL){ delete_proc_cpu(evset->pc[1]); evset->pc[1] = NULL; }
  if(evset->pc[2] != NULL){ delete_proc_cpu(evset->pc[2]); evset->pc[2] = NULL; }  

  if(evset->pn[0] != NULL){ delete_proc_numa(evset->pn[0]); evset->pn[0] = NULL; }
  if(evset->pn[1] != NULL){ delete_proc_numa(evset->pn[1]); evset->pn[1] = NULL; }
  if(evset->pn[2] != NULL){ delete_proc_numa(evset->pn[2]); evset->pn[2] = NULL; }  

  if(evset->pm != NULL){ delete_proc_mem(evset->pm); evset->pm = NULL; }
  return 0;
}

int hmonitor_eventset_add_named_event(void * monitor_eventset, const char * counter){
  struct proc_eventset * evset = (struct proc_eventset *)(monitor_eventset);
  double * event = NULL;

  /* Check if number event is not exceeding max */
  if(evset->n_events >= MAX_EVENTS){
    monitor_print_err("Unable to add counter %s to monitor because it would exceed the maximu(%d) of allowed events\n", MAX_EVENTS);
    return -1;
  }
  
  if(evset->location->type == HWLOC_OBJ_PU){
    if(!strcmp(counter, "cpuload")){
      event = &(evset->cpu_load);
    } else if(!strcmp(counter, "iowait")){
      event = &(evset->pc[2]->values[5]);
    } else if(!strcmp(counter, "irq")){
      event = &(evset->pc[2]->values[6]);
    } else if(!strcmp(counter, "softirq")){
      event = &(evset->pc[2]->values[7]);
    }
    if(event == NULL){
      monitor_print_err("Event %s does not match any in proc plugin for depth %s\n",
			counter, hwloc_type_name(evset->location->type));
      return -1;
    }    
  } else if(evset->location->type == HWLOC_OBJ_NUMANODE){
    if(!strcmp(counter, "memtotal")){
      event = &(evset->pm->values[1]);
    } else if(!strcmp(counter, "memused")){
      event = &(evset->pm->values[3]);
    } else if(!strcmp(counter, "memfree")){
      event = &(evset->pm->values[2]);
    } else if(!strcmp(counter, "local_hit")){
      event = &(evset->pn[2]->values[1]);
    } else if(!strcmp(counter, "remote_hit")){
      event = &(evset->pn[2]->values[2]);
    }
    if(event == NULL){
      monitor_print_err("Event %s does not match any in proc plugin for depth %s\n",
			counter, hwloc_type_name(evset->location->type));
      return -1;
    }    
  } else if(evset->location->type == HWLOC_OBJ_MACHINE){
    if(!strcmp(counter, "cpuload")){
      event = &(evset->cpu_load);
    } else if(!strcmp(counter, "minor_faults")){
      event = &(evset->min_flt);
    } else if(!strcmp(counter, "major_faults")){
      event = &(evset->maj_flt);
    } else if(!strcmp(counter, "vsize")){
      event = &(evset->ps[2]->values[10]);
    } else if(!strcmp(counter, "rss")){
      event = &(evset->ps[2]->values[11]);
    } else if(!strcmp(counter, "blkio_ticks")){
      event = &(evset->ps[2]->values[14]);
    }
    if(event == NULL){
      monitor_print_err("Event %s does not match any in proc plugin for depth %s\n",
			counter, hwloc_type_name(evset->location->type));
      return -1;
    }
  }

  evset->events[evset->n_events] = event;
  evset->n_events++;
  return 1;
}

int hmonitor_eventset_init_fini(void * monitor_eventset){return 0;}

int hmonitor_eventset_start(void * monitor_eventset){
  struct proc_eventset * evset = (struct proc_eventset *)(monitor_eventset);
  if(evset->ps[0]) proc_stat_update(evset->ps[0]);
  if(evset->pc[0]) proc_cpu_update(evset->pc[0]);
  if(evset->pm) proc_mem_update(evset->pm);
  if(evset->pn[0]) proc_numa_update(evset->pn[0]);
}

int hmonitor_eventset_stop(void * monitor_eventset){/*Nothing to do*/}

int hmonitor_eventset_reset(void * monitor_eventset){/*TODO*/}

int hmonitor_eventset_read(void * monitor_eventset, double * values){
  struct proc_eventset * evset = (struct proc_eventset *)(monitor_eventset);

  /* swap old an new */
  struct proc_stat* ps = evset->ps[0]; evset->ps[0] = evset->ps[1]; evset->ps[1] = ps;
  struct proc_cpu*  pc = evset->pc[0]; evset->pc[0] = evset->pc[1]; evset->pc[1] = pc;
  struct proc_numa* pn = evset->pn[0]; evset->pn[0] = evset->pn[1]; evset->pn[1] = pn;

  /* update new */
  if(evset->ps[1]){
    proc_stat_update(evset->ps[1]);
    proc_stat_diff(evset->ps[2], evset->ps[1], evset->ps[0]);    
  }
  if(evset->pc[1]){ proc_cpu_update(evset->pc[1]); }  
  if(evset->pm){ proc_mem_update(evset->pm); }
  if(evset->pn[1]){
    proc_numa_update(evset->pn[1]);
    proc_numa_diff(evset->pn[2], evset->pn[1], evset->pn[0]);    
  }

  /* Store variations */
  if(evset->location->type == HWLOC_OBJ_MACHINE){
    evset->cpu_load = proc_stat_cpuload(evset->ps[0], evset->ps[1]);
    evset->min_flt  = (evset->ps[2]->min_flt + evset->ps[2]->cmin_flt);
    evset->maj_flt  = (evset->ps[2]->maj_flt + evset->ps[2]->cmaj_flt);
  } else if(evset->location->type == HWLOC_OBJ_PU){
    evset->cpu_load = proc_cpu_load(evset->pc[0], evset->pc[1]);
  }
  
  /* Output values */
  unsigned i; for(i=0; i<evset->n_events; i++){ values[i] = *(evset->events[i]); }  
  return 0;
}

