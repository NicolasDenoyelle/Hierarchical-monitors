#ifndef MONITOR_H
#define MONITOR_H

#include <hmon/hmonitor.h>
#include <hmon/harray.h>
#include <hwloc.h>

/************************************************ Monitor lib **************************************************/
extern harray           monitors;
extern hwloc_topology_t hmon_topology;

/**
 * Initialize the library.
 * @param topology, an optional topology to map monitor on. If topo is NULL the current machine topology is used.
 * @param restrict_obj, a topology object where the library is allowed to spawn threads.
 * @param out, the file where to print the trace.
 * @return -1 on error, 0 on success;
 **/
int hmon_lib_init(hwloc_topology_t topology, char * out);

/**
 * Delete all library internal structures.
 **/
void hmon_lib_finalize();

/**
 * Restrict monitor registration into sepcified domain. If monitor outside of domain are already registered, they are deleted.
 * @param domain, the domain cpuset to restrict.
 **/
void hmon_restrict(hwloc_cpuset_t domain);

/**
 * Call hmon_restrict on pid execution domain.
 * @param pid, the pid of the process to look at.
 **/
void hmon_restrict_pid(pid_t pid);

/**
 * Stop monitors on cores where there is no pid's tasks or pid's tasks are sleeping.
 **/
void hmon_restrict_pid_taskset(pid_t pid, int recurse);

/**
 * Add a monitor to update with others monitor.
 * @param m, the monitor to register.
 * @param silent, a boolean telling whether m should be updated.
 * @param display, an int telling whether m display_th sample should be displayed on topology.
 * @return -1 if the monitor could not be registered (cause: monitor is out of library cpuset), else 0.
 **/
int hmon_register_hmonitor(hmon m, int silent, int display);

/**
 * Retrieve monitor on monitors topology.
 * @param depth, the depth of monitors.
 * @param logical_index, the index of monitors.
 * @return An harray of monitors if there are monitor at this location, else NULL.
 **/
harray hmon_get_monitors_by_depth(unsigned depth, unsigned logical_index);

/**
 * Start all monitors
 **/
void hmon_start();

/**
 * Stop all monitors
 **/
void hmon_stop();

/**
 * Update and print every created monitor.
 * Print format:
 * Id Obj timestamp events...
 **/
void hmon_update();

/**
 * Check update completion.
 **/
int hmon_is_uptodate();

/**
 * Import monitors from a configuration file. Imported monitors are stored in global list of monitors: monitors,
 * and also on topology node local lists of monitors.
 * @param input_path, the path to the input configuration file.
 * @return -1 on error, 0 on success to import new monitors;
 **/
int hmon_import_hmonitors(const char * input_path);

/**
 * Update monitors every us micro seconds.
 * @param us, the delay between each update.
 * @return -1 if an error occured, else 0.
 **/
int hmon_sampling_start(const long us);

/**
 * Stop monitors' sampling.
 * @return -1 if an error occured, else 0.
 **/
int hmon_sampling_stop();
  
/**
 * Display the monitors peridocally.
 * @arg display_monitors: the function used to display monitors.
 * @arg arg: the argument to pass to the display function.
 * @return 0 on success, -1 on error, and error reason is output.
 **/
int hmon_periodic_display_start(int (*display_monitors)(int), int arg);

/**
 * Stop automatic display of the monitors.
 * @return 0 on success, -1 on error, and error reason is output.
 **/
int hmon_periodic_display_stop();


#endif //MONITOR_H

