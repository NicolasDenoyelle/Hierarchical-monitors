#ifndef MONITOR_H
#define MONITOR_H

#include "monitor_utils.h"

extern struct array *   monitors;
extern hwloc_topology_t monitors_topology;
extern unsigned         monitors_topology_depth;
extern hwloc_cpuset_t   monitors_running_cpuset;

/**
 * @brief Struct to hold hardware events value.
 **/
struct monitor{
    /** node where values are recorded **/
    hwloc_obj_t location;
    /** eventsets at this location **/
    void * eventset;
    /** pointers to performance library handling event collection **/
    struct monitor_perf_lib * perf_lib;
    /** 
     * events: n_samples * eventset_size.
     * samples: n_samples (aggregated events).
     * value: aggregated samples, maximum and minimum.
     **/
    double ** events, * samples, value, min_value, max_value;
    /** The number of events per eventset **/
    unsigned n_events;
    /* The timestamp of each sample */
    long * timestamps;
    /** The number of samples kept, the current sample index and the total amount of recorded samples **/
    unsigned n_samples, current, total;
    /** The functions to aggregate events: The first on aggregates events into  **/
    struct monitor_stats_lib * events_stat_lib, * samples_stat_lib;
    /** If stopped do not stop twice **/
    int stopped;
    /** Not available while reading events **/
    pthread_mutex_t available;
    void * userdata;
};

/**
 * A monitor is an object recording performance values for a certain topology node.
 * Monitors are stored on nodes of the topology. Monitors on the same node are read sequentially.
 * Performance values are collected and computed with user defined libraries.
 * The monitor library help initializing a set of monitors and read them simultaneously.
 * It monitors the whole system.
 **/

/**
 * Initialize the library.
 * @param topo, an topology to map monitor on. If topo is NULL the current machine topology is used.
 * @param output, the path where to print the trace.
 * @return 0 on error, 1 on success;
 **/
int monitor_lib_init(hwloc_topology_t topo, char * output);

/**
 * Destroy monitors outside of pid allowed cpuset.
 * Must be called before monitors_start.
 * @param pid, the pid of the process to look at.
 * @return true if successfull, false if not (because process does not exists).
 **/
int monitors_restrict(pid_t pid);

/**
 * Call monitors_restrict(<pid>).
 * If monitors_restrict(<pid>) was previously called, then <pid> is the argument passed to the latest call,
 * else it is getpid().
 * Look into /proc/<pid>/task for tasks states.
 * @param recurse, the number of recursive look into child tasks. If negative, traverse children till the leaves.
 **/
void monitors_register_threads(int recurse);

/**
 * Import monitors from a configuration file. Imported monitors are stored in global list of monitors: monitors,
 * and also on topology node local lists of monitors.
 * @param input_path, the path to the input configuration file.
 * @param output_path, the path to the file where to print monitors.
 * @return -1 on error, 0 on success to import new monitors;
 **/
int monitors_import(char * input_path);

/**
 * Start imported monitors.
 * Spawns one thread per topology object containing a monitor.
 **/
void monitors_start();

/**
 * Update and print every created monitor. If attached to a pid, also updates the set of active monitors.
 * Print format:
 * Obj timestamp events... sample min value max
 **/
void monitors_update();

/**
 * Check if monitor buffer is full, then print the full buffer, else do nothing.
 * If force flag is true, then print all previous samples untill the current.
 * @param m, the monitor to print.
 * @param force, a flag to tell whether we have to print the buffer content even if it is not full.
 **/
void monitor_buffered_output(struct monitor * m, int force);

/**
 * print monitor current: location, timestamp, events, min_value, value, max_value.
 * @param m, the monitor to print.
 * @param wait_availability, wait monitor availibility
 **/
void monitor_output(struct monitor * m, int wait_availability);

/**
 * print all printable monitors using a one of the method to print a single monitor.
 * @param monitor_output_method, the method to print monitors among the method to print a single monitor.
 * @param flag, the flag to pass to the monitor_output_method.
 **/
void monitors_output(void (* monitor_output_method)(struct monitor*, int), int flag);

/**
 * Additionnaly to print text file using update, one can display monitors on topology.
 **/
void monitor_display_all(int verbose);

/**
 * Delete all structure built during the library usage.
 **/
void monitor_lib_finalize();

#endif //MONITOR_H

