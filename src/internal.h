#ifndef MONITOR_UTILS_H
#define MONITOR_UTILS_H

#include <hwloc.h>

/*********************************************** hwloc utils ***************************************************/

int          hwloc_check_version_mismatch();
int          location_cpubind(hwloc_topology_t, hwloc_obj_t);
int          location_membind(hwloc_topology_t, hwloc_obj_t);
char *       location_name   (hwloc_obj_t);
hwloc_obj_t  location_parse  (hwloc_topology_t, const char *);
char **      location_avail  (hwloc_topology_t, unsigned *);
int          location_compare(void *, void *); /* void * must be: hwloc_obj_t */
int          get_max_objs_inside_cpuset_by_type(hwloc_topology_t, hwloc_cpuset_t, hwloc_obj_type_t);

/*********************************************** proc utils ****************************************************/ 

void  proc_get_allowed_cpuset(pid_t, hwloc_cpuset_t);
void  proc_get_running_cpuset(pid_t, hwloc_cpuset_t out, int recurse);
void  proc_move_tasks        (pid_t, hwloc_obj_t to, hwloc_obj_t from, int recurse);
pid_t start_executable       (char * exe, char * args[]);

/********************************************* console utils ***************************************************/

void hmon_display_all(hwloc_topology_t topology, int verbose);

/********************************************* parser utils ****************************************************/

int hmon_import(const char * input_path, const hwloc_cpuset_t domain);

/********************************************* plugin utils ****************************************************/
  
#define HMON_PLUGIN_STAT 0
#define HMON_PLUGIN_PERF 1

struct hmon_plugin{
    char * id;
    void * dlhandle;
};

struct hmon_plugin *    hmon_plugin_lookup(const char * name, int type);
void *                  hmon_plugin_load_fun  (struct hmon_plugin * p, const char * name, int print_error);
void                    hmon_stat_plugin_build(const char * name, const char * code);
void                    hmon_stat_plugins_list();
void                    hmon_perf_plugins_list();
void *                  hmon_stat_plugins_lookup_function(const char * name);

/*********************************************** misc utils ****************************************************/

int hmon_compare(void* hmonitor_a, void* hmonitor_b);
#define perror_EXIT(msg) do{perror(msg); exit(EXIT_FAILURE);} while(0)
#define malloc_chk(ptr, size) do{ptr = malloc(size); if(ptr == NULL){perror_EXIT("malloc");}} while(0)
#define realloc_chk(ptr, size) do{if((ptr = realloc(ptr,size)) == NULL){perror_EXIT("realloc");}} while(0)
#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define str(s) #s
#define x_str(s) str(s)
#define monitor_print_err(...) fprintf(stderr, __FILE__  "(" x_str(__LINE__) "): " __VA_ARGS__)

#endif /* MONITOR_UTILS_H */

