#ifndef MONITOR_PLUGIN_H
#define MONITOR_PLUGIN_H

#include "monitor.h"

#define MONITOR_PLUGIN_STAT 0
#define MONITOR_PLUGIN_PERF 1

struct monitor_plugin{
    char * id;
    /**
     * object returned by dlopen from which we retrieve functions.
     **/
    void * dlhandle;
};

/**
 * Load monitor plugin
 * @param name, The plugin name.
 * @return An identified plugin.
 **/
struct monitor_plugin * monitor_perf_plugin_load(char * name, int plugin_type);

/**
 * Load a function from a monitor plugin
 * @param p, the plugin from which to load function
 * @param name, the function name to load.
 * @return a pointer to the function. NULL if the function does not exists, while printing out cause.
 **/
void * monitor_perf_plugin_load_fun(monitor_plugin * p, char * name, int print_error);

/**
 * Write, compile and load a monitor stat plugin from code.
 * @param name, the plugin  name.
 * @param code, The code to compile.
 * @return A monitor plugin
 **/
void monitor_stat_plugin_build(const char * name, const char * code);

/**
 * Print every loaded plugins
 **/
void monitor_stat_plugins_list();

/**
 * Look among loaded monitor plugins if function exists, then return it, else return NULL.
 * @param the function name to lookup.
 * @return a pointer to the function if found, else NULL.
 **/
void * monitor_stat_plugins_lookup_function(const char * name);

#endif

