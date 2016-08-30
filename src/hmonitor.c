#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <time.h>
#include "./internal.h"
#include "./hmon/hmonitor.h"

static void print_avail_events(struct hmon_plugin * lib){
  char ** (* events_list)(int *) = hmon_plugin_load_fun(lib, "hmonitor_events_list", 1);
  if(events_list == NULL)
    return;

  int n = 0;
  char ** avail = events_list(&n);
  printf("List of available events:\n");
  while(n--){
    printf("\t%s\n",avail[n]);
    free(avail[n]);
  }
  free(avail);
}

static void hmonitor_set_timestamp(hmon m, long timestamp){
  m->events[m->last*(m->n_events+1)+m->n_events] = timestamp;
}

hmon
new_hmonitor(const char *id, const hwloc_obj_t location, const char ** event_names, const unsigned n_events,
	     const unsigned window, const unsigned n_samples, const char * perf_plugin, const char * model_plugin)
{
  if(window == 0 || id == NULL || location == NULL || perf_plugin == NULL){return NULL;}

  hmon monitor = malloc(sizeof(*monitor));
        
  /* Set default attributes */
  monitor->id = strdup(id);
  monitor->location = location;
  monitor->window = window;
  monitor->userdata = NULL;
  monitor->silent = 0;
    
  /* Load perf plugin functions */
  struct hmon_plugin * plugin = hmon_plugin_load(perf_plugin, HMON_PLUGIN_PERF);
  if(plugin == NULL){
    fprintf(stderr, "Monitor on %s:%d, performance initialization failed\n", hwloc_type_name(location->type), location->logical_index);
    exit(EXIT_FAILURE);
  }
  monitor->eventset_start    = hmon_plugin_load_fun(plugin, "hmonitor_eventset_start",   1);
  monitor->eventset_stop     = hmon_plugin_load_fun(plugin, "hmonitor_eventset_stop",    1);
  monitor->eventset_reset    = hmon_plugin_load_fun(plugin, "hmonitor_eventset_reset",   1);
  monitor->eventset_read     = hmon_plugin_load_fun(plugin, "hmonitor_eventset_read",    1);
  monitor->eventset_destroy  = hmon_plugin_load_fun(plugin, "hmonitor_eventset_destroy", 1);
  if(monitor->eventset_start   == NULL ||
     monitor->eventset_stop    == NULL ||
     monitor->eventset_reset   == NULL ||
     monitor->eventset_destroy == NULL ||
     monitor->eventset_read    == NULL){
    fprintf(stderr, "Monitor on %s:%d, performance initialization failed\n",
	    hwloc_type_name(location->type), location->logical_index);
    exit(EXIT_FAILURE);
  }

  /* Initialize eventset */
  int err;
  unsigned added_events = 0;
  int (* eventset_init)(void **, hwloc_obj_t);
  int (* eventset_init_fini)(void*);
  int (* add_named_event)(void*, const char*);
  eventset_init      = hmon_plugin_load_fun(plugin, "hmonitor_eventset_init"           , 1);
  eventset_init_fini = hmon_plugin_load_fun(plugin, "hmonitor_eventset_init_fini"      , 1);
  add_named_event    = hmon_plugin_load_fun(plugin, "hmonitor_eventset_add_named_event", 1);

  if(eventset_init(&monitor->eventset, location)){
    monitor_print_err("%s failed to initialize eventset\n", id);
    exit(EXIT_FAILURE);
  }
  for(unsigned i=0; i<n_events; i++){
    err = add_named_event(monitor->eventset, event_names[i]);
    if(err == -1){
      monitor_print_err("failed to add event %s to %s eventset\n", event_names[i], id);
      print_avail_events(plugin);	    
      exit(EXIT_FAILURE);
    }
    added_events += err;
  }
  eventset_init_fini(monitor->eventset);
  monitor->events = malloc(window*(added_events+1)*sizeof(double));
  monitor->n_events = added_events;
	
  /* Events reduction */
  if(model_plugin){
    monitor->samples = malloc(sizeof(double) * n_samples+1);
    monitor->max = malloc(sizeof(double) * n_samples+1);
    monitor->min = malloc(sizeof(double) * n_samples+1);	
    monitor->n_samples = n_samples;
    monitor->model = hmon_stat_plugins_lookup_function(model_plugin);
  }
  else{
    monitor->samples = malloc(sizeof(double) * added_events+1);
    monitor->max = malloc(sizeof(double) * added_events+1);
    monitor->min = malloc(sizeof(double) * added_events+1);
    monitor->n_samples = added_events;
    monitor->model = NULL;
  }
    
  /* reset values */
  hmonitor_reset(monitor);
  pthread_mutex_init(&(monitor->available), NULL);
        
  /* Initialization succeed */
  return monitor;
}

void delete_hmonitor(hmon monitor){
  free(monitor->events);
  free(monitor->samples);
  free(monitor->max);
  free(monitor->min);
  free(monitor->id);
  monitor->eventset_destroy(monitor->eventset);
  pthread_mutex_destroy(&(monitor->available));
  free(monitor);
}

void hmonitor_reset(hmon m){
  m->last = 0;
  m->total = 0;
  m->last = m->window-1;
  m->stopped = 1;
  for(unsigned i=0;i<m->window*m->n_events+1;i++){m->events[i] = 0;}
  for(unsigned i=0;i<m->n_samples;i++){
    m->samples[i]=0;
    m->max[i]=DBL_MIN;
    m->min[i]=DBL_MAX;
  }
  m->eventset_reset(m->eventset);
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  m->ref_time = 1000000000 * tp.tv_sec + tp.tv_nsec;
}

void hmonitor_output(hmon m, FILE* out, int wait){
  if(!m->silent){
    unsigned j;
    if(wait){pthread_mutex_lock(&(m->available));}
    fprintf(out,"%-16s ",  m->id);
    fprintf(out,"%8s:%u ", hwloc_type_name(m->location->type), m->location->logical_index);
    fprintf(out,"%14ld ",  hmonitor_get_timestamp(m,m->last));
    for(j=0;j<m->n_samples;j++){fprintf(out, "%-.6e ", m->samples[j]);}
    fprintf(out,"\n");
    if(wait){pthread_mutex_unlock(&(m->available));}
  }
}

double * hmonitor_get_events(hmon m, unsigned i){
  return &(m->events[i*(m->n_events+1)]);
}

double hmonitor_get_event(hmon m, unsigned row, unsigned event){
  return m->events[row*(m->n_events+1)+event];
}

long hmonitor_get_timestamp(hmon m, unsigned i){
  return (long)m->events[i*(m->n_events+1)+m->n_events];
}

int hmonitor_start(hmon m){
  if(m->stopped){
    if(m->eventset_start(m->eventset) == -1){return -1;}
    m->stopped = 0;
  }
  return 0;
}

int hmonitor_stop(hmon m){
  if(!m->stopped){
    if(m->eventset_stop(m->eventset) == -1){return -1;}
    m->stopped = 1;
  }
  return 0;
}

int hmonitor_trylock(hmon m, int wait){
 int err = pthread_mutex_trylock(&m->available);
 if(err == EBUSY){
   if(wait){
     pthread_mutex_lock(&m->available);
     pthread_mutex_unlock(&m->available);
   }
   return 0;
 }
 pthread_mutex_unlock(&m->available);
 if(err != 0){perror("pthread_mutex_trylock"); return -1;}
 return 1;
}

int hmonitor_release(hmon m){
  if(pthread_mutex_unlock(&m->available) != 0){perror("pthread_mutex_unlock"); return -1;}
  return 0;
}

int hmonitor_read(hmon m){
  m->last = (m->last+1)%(m->window);
    
  /* Save timestamp */
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  hmonitor_set_timestamp(m, 1000000000 * tp.tv_sec + tp.tv_nsec - m->ref_time);
    
  /* Read events */
  if((m->eventset_read(m->eventset, hmonitor_get_events(m, m->last))) == -1){
    fprintf(stderr, "Failed to read counters from monitor on obj %s:%d\n",
	    hwloc_type_name(m->location->type), m->location->logical_index);
    return -1;
  }
  m->total = m->total+1;
  hmonitor_release(m);
  return 0;
}

void hmonitor_reduce(hmon m){
  /* Reduce events */
  if(m->model!=NULL){m->model(m);}
  else{memcpy(m->samples, hmonitor_get_events(m, m->last), sizeof(double)*(m->n_samples));}
  for(unsigned i=0;i<m->n_samples;i++){
    m->max[i] = (m->max[i] > m->samples[i]) ? m->max[i] : m->samples[i];
    m->min[i] = (m->min[i] < m->samples[i]) ? m->min[i] : m->samples[i];
  }
}

