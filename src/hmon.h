#ifndef MONITOR_H
#define MONITOR_H

#include <hwloc.h>

/************************************************ Array utils **************************************************/
typedef struct hmon_array{
    void **  cell;
    unsigned length;
    unsigned allocated_length;
    void     (*delete_element)(void*);
} * harray;

harray         new_harray          (size_t elem_size, unsigned max_elem, void (*delete_element)(void*));
harray         harray_dup          (harray);
void           delete_harray       (harray);
void           empty_harray        (harray);
unsigned       harray_length       (harray);
void *         harray_get          (harray, unsigned);
void **        harray_get_data(harray harray);
void *         harray_set          (harray, unsigned, void *);
void *         harray_pop          (harray);
void           harray_push         (harray, void *);
void *         harray_remove       (harray, int);
void           harray_insert       (harray, unsigned, void *);
unsigned       harray_insert_sorted(harray array, void * element, int (* compare)(void*, void*));
void           harray_sort         (harray array, int (* compare)(void*, void*));
int            harray_find         (harray array, void * key, int (* compare)(void*, void*));
int            harray_find_unsorted(harray harray, void * key);
/***************************************************************************************************************/


/************************************************ Monitor ******************************************************/
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

    /* The number of stored updates, the index of latest update, and the total number of updates */
    unsigned window, last, total;

    /** monitor input: eventsets, set of collected event. Last element of each line is the timestamp **/
    void   * eventset;
    double * events;
    unsigned n_events;

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
    /** 1:ACTIVE, 0:SLEEPING **/
    int state_query;
    /** another monitor depend on this one and is in charge for updating it **/
    struct hmon * depend;
    /** Not available while reading events **/
    pthread_mutex_t available;

    /* Set to NULL, unused by the library, but maybe by some plugins */
    void * userdata;
} * hmon;

/**
 * Get pointer on monitor events row.
 **/
double * monitor_get_events(hmon, unsigned row);
/**
 * Get event value at row row and column idx.
 **/
double   monitor_get_event(hmon, unsigned row, unsigned idx);
/**
 * Get timestamp at row.
 **/
long     monitor_get_timestamp(hmon, unsigned row);
/**
 * Reset monitor values
 **/
void     monitor_reset(hmon);
/**
 * Read events in monitor.
 **/
void     monitor_read(hmon);
/**
 * Compute samples in monitor.
 **/
void     monitor_reduce(hmon);

/***************************************************************************************************************/


/************************************************ Monitor lib **************************************************/
extern harray           monitors;
extern hwloc_topology_t monitors_topology;
extern unsigned         monitors_topology_depth;
extern hwloc_cpuset_t   monitors_running_cpuset;

/**
 * Initialize the library.
 * @param topo, an topology to map monitor on. If topo is NULL the current machine topology is used.
 * @param output, the path where to print the trace.
 * @return -1 on error, 0 on success;
 **/
int monitor_lib_init(hwloc_topology_t topo, char * restrict_obj, char * output);

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
 * To be called once.
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
void monitor_buffered_output(hmon m, int force);

/**
 * print monitor current: location, timestamp, events, min_value, value, max_value.
 * @param m, the monitor to print.
 * @param wait_availability, wait monitor availibility
 **/
void monitor_output(hmon m, int wait_availability);

/**
 * print all printable monitors using a one of the method to print a single monitor.
 * @param monitor_output_method, the method to print monitors among the method to print a single monitor.
 * @param flag, the flag to pass to the monitor_output_method.
 **/
void monitors_output(void (* monitor_output_method)(hmon, int), int flag);

/**
 * Additionnaly to print text file using update, one can display monitors on topology.
 **/
void monitor_display_all(int verbose);

/**
 * Delete all structure built during the library usage.
 **/
void monitor_lib_finalize();
/***************************************************************************************************************/

#endif //MONITOR_H

