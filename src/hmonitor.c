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

static void hmonitor_set_labels(hmon m, const char ** labels, const unsigned n){
  unsigned i;
  m->labels = malloc(sizeof(*m->labels) * m->n_samples);
  for(i=0; i<n && i<m->n_samples; i++)
    m->labels[i] = strdup(labels[i]);
  for(; i<m->n_samples; i++){
    m->labels[i] = malloc(16); memset(m->labels[i], 0, 16);
    snprintf(m->labels[i], 16, "V%u", i);
  }
}

void hmonitor_output_header(hmon m){
  unsigned i;
  char str[32]; memset(str, 0, sizeof(str));
  snprintf(str, sizeof(str), "%8s:%u", hwloc_type_name(m->location->type), m->location->logical_index);
  fprintf(m->output,"%*s %14s ", (int)strlen(str), "Obj", "Nanoseconds");
  memset(str, 0, sizeof(str));
  snprintf(str, sizeof(str), "%-.6e", 0.0);
  for(i=0; i<m->n_samples; i++) fprintf(m->output,"%*s ", (int)strlen(str), m->labels[i]);
  fprintf(m->output,"\n");
  fflush(m->output);
}

void hmonitor_fprint(hmon m, FILE* f){
  unsigned i;
  char events[1024]; memset(events, 0, sizeof(events));
  ssize_t nc = 0;
  for(i =0 ; i<m->n_events; i++){ nc += snprintf(events, sizeof(events)-nc, "%10s ", m->labels[i]); }
  fprintf(f, "%20s (%10s): output:%s, events: %s\n",
	  m->id,
	  location_name(m->location),
	  m->output?"yes":" no",	  
	  events);
}

hmon
new_hmonitor(const char *id, const hwloc_obj_t location, const char ** event_names, const unsigned n_events,
	     const unsigned window, const char ** labels, const unsigned n_samples,
	     const char * perf_plugin, const char * model_plugin, FILE* output)
{
  if(window == 0 || id == NULL || location == NULL || perf_plugin == NULL){return NULL;}

  hmon monitor = malloc(sizeof(*monitor));
        
  /* Set default attributes */
  monitor->id = strdup(id);
  monitor->location = location;
  monitor->window = window;
  monitor->userdata = NULL;
  monitor->display = 0;
  monitor->owner = pthread_self();
  monitor->output = output;
  /* Load perf plugin functions */
  struct hmon_plugin * plugin = hmon_plugin_lookup(perf_plugin, HMON_PLUGIN_PERF);
  if(plugin == NULL){
    fprintf(stderr, "Unfound plugin %s\n", perf_plugin);
    fprintf(stderr, "Monitor on %s:%d, performance initialization failed\n",
	    hwloc_type_name(location->type),
	    location->logical_index);
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
  unsigned i, added_events = 0;
  int (* eventset_init)(void **, hwloc_obj_t);
  int (* eventset_init_fini)(void*);
  int (* add_named_event)(void*, const char*);
  eventset_init      = hmon_plugin_load_fun(plugin, "hmonitor_eventset_init"           , 1);
  eventset_init_fini = hmon_plugin_load_fun(plugin, "hmonitor_eventset_init_fini"      , 1);
  add_named_event    = hmon_plugin_load_fun(plugin, "hmonitor_eventset_add_named_event", 1);

  if(eventset_init(&monitor->eventset, location)){
    monitor_print_err("%s failed to initialize eventset\n", id);
    free(monitor);
    return NULL;
  }
  for(i=0; i<n_events; i++){
    err = add_named_event(monitor->eventset, event_names[i]);
    if(err == -1){
      monitor_print_err("failed to add event %s to %s eventset\n", event_names[i], id);
      print_avail_events(plugin);
      free(monitor);
      return NULL;
    }
    added_events += err;
  }
  eventset_init_fini(monitor->eventset);
  monitor->events = malloc(window*(added_events+1)*sizeof(double));
  monitor->n_events = added_events;
  
  /* Initialize output */  
  if(model_plugin){
    monitor->samples = malloc(sizeof(double) * n_samples+1);
    monitor->max = malloc(sizeof(double) * n_samples+1);
    monitor->min = malloc(sizeof(double) * n_samples+1);	
    monitor->n_samples = n_samples;
    monitor->model = hmon_stat_plugins_lookup_function(model_plugin);
    hmonitor_set_labels(monitor, labels, labels == NULL ? 0 : n_samples);
  } else {
    monitor->samples = malloc(sizeof(double) * added_events+1);
    monitor->max = malloc(sizeof(double) * added_events+1);
    monitor->min = malloc(sizeof(double) * added_events+1);
    monitor->n_samples = added_events;
    monitor->model = NULL;
    if(labels == NULL && added_events > n_events){
      hmonitor_set_labels(monitor, NULL, 0);
    } else if(labels == NULL){
      hmonitor_set_labels(monitor, event_names, n_events);
    } else {
      hmonitor_set_labels(monitor, labels, n_samples);
    }
    monitor->n_samples = added_events;
  }
  
  /* reset values */
  hmonitor_reset(monitor);
  pthread_mutex_init(&(monitor->mutex), NULL);
        
  /* Initialization succeed */
  return monitor;
}

void delete_hmonitor(hmon monitor){
  int i;
  hmonitor_stop(monitor);
  free(monitor->events);
  free(monitor->samples);
  free(monitor->max);
  free(monitor->min);
  free(monitor->id);
  monitor->eventset_destroy(monitor->eventset);
  for(i=0; i<monitor->n_samples; i++){free(monitor->labels[i]);}
  free(monitor->labels);
  pthread_mutex_destroy(&(monitor->mutex));
  free(monitor);
}

void hmonitor_reset(hmon m){
  unsigned i;
  m->last = 0;
  m->total = 0;
  m->last = m->window-1;
  m->stopped = 1;
  for(i=0;i<m->window*m->n_events+1;i++){m->events[i] = 0;}
  for(i=0;i<m->n_samples;i++){
    m->samples[i]=0;
    m->max[i]=DBL_MIN;
    m->min[i]=DBL_MAX;
  }
  m->eventset_reset(m->eventset);
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  m->ref_time = 1000000000 * tp.tv_sec + tp.tv_nsec;
}

void hmonitor_output(hmon m, const int force){
  if(m->output != NULL && (m->owner == pthread_self() || force)){
    unsigned j;
    char samples[m->n_samples*20]; memset(samples, 0, sizeof(samples));
    char *c = samples;
    for(j=0;j<m->n_samples;j++){c+=sprintf(c, "%-.6e ", m->samples[j]);}
    fprintf(m->output,"%8s:%u %14ld %s\n",
	    hwloc_type_name(m->location->type),
	    m->location->logical_index,
	    hmonitor_get_timestamp(m,m->last),
	    samples);
    fflush(m->output);
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
  if(!m->stopped){return 1;}
  if(m->owner == pthread_self()){
    if(m->eventset_start(m->eventset) == -1){return -1;}
    m->stopped = 0;
    if(pthread_mutex_unlock(&m->mutex) != 0){perror("pthread_mutex_unlock"); return -1;}
    return 1;
  }
  return 0;
}

int hmonitor_stop(hmon m){
  if(m->stopped){return 1;}
  if(m->owner == pthread_self() || hmonitor_trylock(m, 0) == 1){
    if(m->eventset_stop(m->eventset) == -1){return -1;}
    m->stopped = 1;
    return 1;
  }
  return 0;
}

int hmonitor_trylock(hmon m, int wait){
  int err = pthread_mutex_trylock(&m->mutex);
  if(err == EBUSY){
    if(m->owner == pthread_self()){return 1;}
    else if(wait){
      pthread_mutex_lock(&m->mutex);
      pthread_mutex_unlock(&m->mutex);
      return 0;
    }
  }
  else if(err != 0){perror("pthread_mutex_trylock"); return -1;}
  m->owner = pthread_self();
  return 1;
}

int hmonitor_release(hmon m){
  if(m->owner == pthread_self()){
    if(pthread_mutex_unlock(&m->mutex) != 0){perror("pthread_mutex_unlock"); return -1;}
    return 1;
  }
  return 0;
}

int hmonitor_read(hmon m){
  /* Only if caller took the lock, or we don't care about concurrent calls or we can acquire the lock and become owner */  
  if(m->owner == pthread_self()){
    m->last = (m->last+1)%(m->window);
    m->total = m->total+1;
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
    return 1;
  }
  return 0;
}

int hmonitor_reduce(hmon m){
  unsigned i;
  if(m->owner == pthread_self()){
    /* Reduce events */
    if(m->model!=NULL){m->model(m);}
    else{memcpy(m->samples, hmonitor_get_events(m, m->last), sizeof(double)*(m->n_samples));}
    for(i=0;i<m->n_samples;i++){
      m->max[i] = (m->max[i] > m->samples[i]) ? m->max[i] : m->samples[i];
      m->min[i] = (m->min[i] < m->samples[i]) ? m->min[i] : m->samples[i];
    }
    return 1;
  }
  return 0;
}

