#ifndef HMONITOR_H
#define HMONITOR_H

#include <hwloc.h>

/**
 * A hierarchical monitor (hmon) is an object recording performance values for a certain topology node.
 * Monitors are stored on nodes of the topology. Monitors on the same node are read sequentially.
 * Performance values are collected and computed with user defined libraries.
 * The monitor library help initializing a set of monitors and read them simultaneously.
 * It monitors the whole system.
 * @brief Struct to hold hardware events value.
 **/
typedef struct hmon{
  /** identifier **/
  char * id;
  /** node where values are recorded **/
  hwloc_obj_t location;
  
  /* monitor reference reset time */
  long long ref_time;
  
  /** monitor input: eventsets, set of collected event. Last element of each line is the timestamp **/
  void   * eventset;
  double * events;
  unsigned n_events;
  /* The number of stored event updates, the index of latest update, and the total number of updates */
  unsigned window, last, total;

  /** monitor output: events reduction **/
  double * samples, * max, * min;
  unsigned n_samples;
  void (* model)(struct hmon*);
    
  /** pointers to performance library handling event collection. Functions documentation in plugins/performance_plugin.h  **/
  int (* eventset_start)   (void *);
  int (* eventset_stop)    (void *);
  int (* eventset_reset)   (void *);
  int (* eventset_read)    (void *, double *);
  int (* eventset_destroy) (void *);
    
  /** If stopped do not stop twice **/
  int stopped;
  /** another monitor depend on this one and is in charge for updating it **/
  struct hmon * depend;
  /** Not available while reading events **/
  pthread_mutex_t available;

  /* Set to NULL, unused by the library, but maybe by some plugins */
  void * userdata;
} * hmon;

/** 
 * Create a new monitor 
 * @arg id: The monitor name identifier. Can be the same for several monitors at same depth location.
 * @arg location: The toplogy location the monitor is supposed to monitor.
 * @arg event_names: Event names to add to monitor eventset.
 * @arg n_events: The number of events to record.
 * @arg window: The number of records to keep into the monitor.
 * @arg n_samples: The number of outputs of the monitor.
 * @arg perf_plugin: The plugin's name used to collect input events.
 * @arg model_plugin: The plugin's name used to reduce collected events into monitor output samples.
 * @return A new monitor.
 **/
hmon new_hmonitor(const char * id, hwloc_obj_t location, const char ** event_names, const unsigned n_events,
		      const unsigned window, unsigned n_samples, const char* perf_plugin, const char* model_plugin);

/**
 * Delete a monitor
 * @arg m: The monitor to delete.
 **/
void delete_hmonitor(hmon m);

/**
 * Reset a monitor attributes as if it was newly created. 
 * @arg m: The monitor to reset.
 **/
void hmonitor_reset(hmon m);

/**
 * Start recording events.
 * @arg m: The monitor to start.
 * @return 0 on success, -1 if eventset_start call failed.
 **/
int hmonitor_start(hmon m);

/**
 * Stop recording events.
 * @arg m: The monitor to stop.
 * @return 0 on success, -1 if eventset_stop call failed.
 **/
int hmonitor_stop(hmon m);

/**
 * Update monitor timestamp and input events..
 * @arg m: The monitor to update.
 * @return 0 on success, -1 if eventset_read call failed.
 **/
int hmonitor_read(hmon m);

/**
 * Call monitor reduction function and update maximum and minimum value of each output event.
 * @arg m: The monitor to reduce.
 **/
void hmonitor_reduce(hmon m);

/**
 * Output monitor's output to a file.
 * @arg out: The file where to print monitor.
 * @arg m: The monitor to print.
 * @arg wait: If 0 print immediately, else block until monitor update is done, 
 *            e.g hmonitor_reduce has been called and no call to hmonitor_read has been made since.
 **/
void hmonitor_output(hmon m, FILE* out, int wait);

/**
 * Get the events of a previous read stored into the monitor.
 * @arg m: The monitor which events are to be retrieved.
 * @arg i: The index/oldness of the events. Must be less than m->window.
 * @return The ith events collected from now. 
 **/
double * hmonitor_get_events(hmon m, unsigned i);

/**
 * Get the ith event of one of previously read events.
 * @arg m: The monitor which event value is to be retrieved.
 * @arg row: the index of the previous read of events from now. Must be less than m->window.
 * @arg event: The event index among row events.
 * @return An event value.
 **/
double   hmonitor_get_event(hmon m, unsigned row, unsigned event);

/**
 * Get the monitor timestamp of a previously collected set of events.
 * @arg m: The monitor from which a timestamp is to be retrieved.
 * @arg i: The index/oldness of the timestamp. Must be less than m->window.
 * @return A timestamp in nanoseconds since the previous resset.
 **/
long hmonitor_get_timestamp(hmon m, unsigned i);

#endif
