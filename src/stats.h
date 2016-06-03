#ifndef MONITOR_STATS_UTILS_H
#define MONITOR_STATS_UTILS_H

#include "monitor.h"

struct monitor_stats_lib{
    char * id;
    /**
     * object returned by dlopen from which we retrieve below functions.
     **/
    void * dlhandle;
    /**
     * Compute statistiques based on an hmon_array of event samples.
     * @param monitor, the monitor holding samples.
     * @return An aggregation value computed on monitor samples.
     */  
    double (*call)(struct monitor * monitor);
};

/**
 * Load monitor stats plugin
 * @param name, The plugin name.
 * @return A set of function usable by a monitor to aggregate values.
 **/
struct monitor_stats_lib * monitor_load_stats_lib(char * name);

/**
 * Generate a monitor stats plugin from a plugin code.
 * @param name, the plugin function name.
 * @param code, The body of function call() in struct monitor_stats_lib.
 * @return A usable stats lib
 **/
struct monitor_stats_lib * monitor_build_custom_stats_lib(const char * name, const char * code);

#endif

