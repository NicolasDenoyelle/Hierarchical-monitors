#ifndef HMONITOR_H
#define HMONITOR_H

#include <pthread.h>
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

  /** Output file. This is private, set and destroyed by synchronize.c **/
  FILE * output;

  /** Do we display this one on topology **/
  unsigned display;
  /** If smaketopped do not stop twice **/
  int stopped;
  /** Handle concurrent updates **/
  pthread_t owner;
  pthread_mutex_t mutex;

  /* Set to NULL, unused by the library, but maybe by some plugins */
  void * userdata;
} * hmon;

/** 
 * Create a new monitor 
 * @param id: The monitor name identifier. Can be the same for several monitors at same depth location.
 * @param location: The toplogy location the monitor is supposed to monitor.
 * @param event_names: Event names to add to monitor eventset.
 * @param n_events: The number of events to record.
 * @param window: The number of records to keep into the monitor.
 * @param n_samples: The number of outputs of the monitor.
 * @param perf_plugin: The plugin's name used to collect input events.
 * @param model_plugin: The plugin's name used to reduce collected events into monitor output samples.
 * @return A new monitor.
 **/
hmon new_hmonitor(const char * id, hwloc_obj_t location, const char ** event_names, const unsigned n_events,
		  const unsigned window, unsigned n_samples, const char* perf_plugin, const char* model_plugin);

/**
 * Delete a monitor
 * @param m: The monitor to delete.
 **/
void delete_hmonitor(hmon m);

/**
 * Reset a monitor attributes as if it was newly created. 
 * @param m: The monitor to reset.
 **/
void hmonitor_reset(hmon m);

/**
 * Start recording events. If the monitor is concurrent, the call succeed only if done from the owner thread. In the latter case,
 * the lock is released.
 * If the monitor is already started, it won't start twice.
 * @param m: The monitor to start.
 * @return 1 on success, 0 if monitor is busy, -1 if eventset_start call failed.
 **/
int hmonitor_start(hmon m);

/**
 * Stop recording events. If the monitor is concurrent, the call succeed only if done from the owner thread, or the lock is successfully acquired during this call.
 * If the monitor is already stopped, it won't stop twice.
 * @param m: The monitor to stop.
 * @return 1 on success, 0 if monitor is busy, -1 if eventset_stop call failed.
 **/
int hmonitor_stop(hmon m);

/**
 * Update monitor timestamp and input events...
 * If the monitor is concurrent, the call may succceed only if monitor is released, or the calling thread is the same that 
 * acquired this monitor lock.
 * @param m: The monitor to update.
 * @return 1 on success, 0 if the monitor was locked by another thread, -1 if eventset_read call failed.
 **/
int hmonitor_read(hmon m);

/**
 * Call monitor reduction function and update maximum and minimum value of each output event.
 * The call may succceed only if the calling thread owns the monitor.
 * @param m: The monitor to reduce.
 * @return 0 if the call does not come from the owner thread, 1 if reduction succeeded.
 **/
int hmonitor_reduce(hmon m);

/**
 * Print monitor to file.
 * The monitor will only be print if its field output was manually set.
 **/
void hmonitor_output(hmon m);

/**
 * Lock monitor to avoid concurrent updates.
 * If the monitor is already locked, then the lock is not acquired.
 * If wait flag is not 0, then the call block until monitor is available.
 * @param m: The monitor to lock.
 * @param wait: block until monitor is available.
 * @return -1 if an error occured, 0 if monitor was busy, 1 if lock is acquired.
 **/
int hmonitor_trylock(hmon m, int wait);

/**
 * Unlock monitor. This call may succeed only if called from the hmonitor_trylock calling pthread.
 * @param m: The monitor to unlock.
 * @return -1 if an error occured, 1 if the monitor is released, 0 if is not.
 **/
int hmonitor_release(hmon m);

/**
 * Get the events of a previous read stored into the monitor.
 * @param m: The monitor which events are to be retrieved.
 * @param i: The index/oldness of the events. Must be less than m->window.
 * @return The ith events collected from now. 
 **/
double * hmonitor_get_events(hmon m, unsigned i);

/**
 * Get the ith event of one of previously read events.
 * @param m: The monitor which event value is to be retrieved.
 * @param row: the index of the previous read of events from now. Must be less than m->window.
 * @param event: The event index among row events.
 * @return An event value.
 **/
double   hmonitor_get_event(hmon m, unsigned row, unsigned event);

/**
 * Get the monitor timestamp of a previously collected set of events.
 * @param m: The monitor from which a timestamp is to be retrieved.
 * @param i: The index/oldness of the timestamp. Must be less than m->window.
 * @return A timestamp in nanoseconds since the previous resset.
 **/
long hmonitor_get_timestamp(hmon m, unsigned i);

#endif
