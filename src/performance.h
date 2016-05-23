#ifndef MONITOR_PERFORMANCE_UTILS_H
#define MONITOR_PERFORMANCE_UTILS_H

#include <hwloc.h>

///////////////////////////////////////// PART TO BE IMPLEMENTED BY A USER LIBRARY /////////////////////////////////////////////
/*                                           See PAPI_monitors.c for an example                                               */

/**
 * The structure holding function from the underlying performance library loaded 
 * If the library has to initialize global objects, like PAPI_library_init, the library's initialization function should handle it.
 **/
struct monitor_perf_lib{
    char * id;
    /**
     * object returned by dlopen from which we retrieve below functions.
     **/
    void * dlhandle;
    /**
     * Global function able to list available events.
     * !!! To be implemented by a user library. Sample in PAPI_monitors.c
     * @param n_events, output the number of available counters.
     * @return An array containing each event's name.
     */  
    char ** (* monitor_events_list)(int * n_counters);
    /**
     * Function to call during monitor_recorder initialization to initialize monitor's local variables from the performance library.
     * This function will be called after the calling thread has been bound to a local object. libPAPI_monitor.c shows how an eventset is bound to the local cpu using PAPI library. However if the eventset is bound to an upper topologic object, the eventset will still be bound to a single cpu and the read values will not reflect all the values of the object's children.
     * !!! To be implemented by a user library.
     * @param monitor_eventset, the structure containing the set of variable to initialize.
     * @param The hardware location where monitor_eventset will be initialized.
     * @param A boolean telling whether this interface will accumulate results upon monitor_eventset_read calls.
     * @return 0 on succes, -1 on error.
     */
    int (* monitor_eventset_init)(void ** monitor_eventset, hwloc_obj_t location, int accumulate);
    /**
     * Function to call during monitor_recorder destruction to delete monitor's local variables from the performance library.
     * !!! To be implemented by a user library. Sample in PAPI_monitors.c
     * @param eventset, the structure containing the set of variable to destroy.
     * @return 0 on succes, -1 on error.
     */
    int (* monitor_eventset_destroy)(void * eventset);
    /**
     * Function to call in order to add a event to the local monitor.
     * !!! To be implemented by a user library. Sample in PAPI_monitors.c
     * @param monitor_eventset, the structure containing the set of variable to initialize.
     * @param event_name, the string name of the counter to add.
     * @return 0 on succes, -1 on error.
     */
    int (* monitor_eventset_add_named_event)(void * monitor_eventset, char * counter);
    /**
     * Function to call after init, and once every counter has been added.
     * Calls of monitor_eventset_init_fini() might be multiple and simultaneous for a same performance library.
     * This function is to be called before any call to monitor_eventset_start().
     * @param monitor_eventset;
     * @return 0 on succes, -1 on error.
     */
    int (* monitor_eventset_init_fini)(void * monitor_eventset);
    /**
     * Function to call to start events from the performance library on the local monitor.
     * !!! To be implemented by a user library. Sample in PAPI_monitors.c
     * @param monitor_eventset, the structure containing the set of variable to use.
     * @return The number of added events on succes, -1 on error.
     */
    int (* monitor_eventset_start)(void * monitor_eventset);
    /**
     * Function to call to stop the library events from counting on the local monitor.
     * !!! To be implemented by a user library. Sample in PAPI_monitors.c
     * @param monitor_eventset, the structure containing the set of variable to use.
     * @return 0 on succes, -1 on error.
     */
    int (* monitor_eventset_stop)(void * monitor_eventset);
    /**
     * Function to call to reset events from the performance library on the local monitor.
     * !!! To be implemented by a user library. Sample in PAPI_monitors.c
     * @param monitor_eventset, the structure containing the set of variable to use.
     * @return 0 on succes, -1 on error.
     */
    int (* monitor_eventset_reset)(void * monitor_eventset);
    /**
     * Function to call to read events value from the performance library on the local monitor.
     * !!! To be implemented by a user library. Sample in PAPI_monitors.c
     * @param monitor_eventset, the structure containing the set of variable to use.
     * @param values, the array of values to update.
     * @return 0 on success to read events. -1 if read error occured.
     */  
    int (* monitor_eventset_read)(void * monitor_eventset, double * values);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Load monitor performance plugin
 * @param name, The plugin name: name, or name.perf_monitor.so or full path.
 * @return A set of function usable by a monitor to gather performance values.
 **/
struct monitor_perf_lib * monitor_load_perf_lib(char * name);
#endif
