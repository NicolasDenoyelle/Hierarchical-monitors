#ifndef MONITOR_UTILS_H
#define MONITOR_UTILS_H

#include <hwloc.h>
#include <stdio.h>

/*********************************************** hwloc utils ***************************************************/
int          location_cpubind(hwloc_obj_t);
int          location_membind(hwloc_obj_t);
char *       location_name   (hwloc_obj_t);
hwloc_obj_t  location_parse  (char *);
char **      location_avail  (unsigned *);
int          location_compare(void *, void *); /* void * must be: hwloc_obj_t */
int          get_max_objs_inside_cpuset_by_type(hwloc_cpuset_t, hwloc_obj_type_t);

/*********************************************** proc utils ****************************************************/ 

void  proc_get_allowed_cpuset(pid_t, hwloc_cpuset_t);
void  proc_get_running_cpuset(pid_t, hwloc_cpuset_t out, int recurse);
void  proc_move_tasks        (pid_t, hwloc_obj_t to, hwloc_obj_t from, int recurse);
pid_t start_executable       (char * exe, char * args[]);

/********************************************* plugin utils ****************************************************/

#define MONITOR_PLUGIN_STAT 0
#define MONITOR_PLUGIN_PERF 1
struct monitor_plugin{
    char * id;
    void * dlhandle;
};

struct monitor_plugin * monitor_plugin_load      (const char * name, int plugin_type);
void *                  monitor_plugin_load_fun  (struct monitor_plugin * p, const char * name, int print_error);
void                    monitor_stat_plugin_build(const char * name, const char * code);
void                    monitor_stat_plugins_list();
void *                  monitor_stat_plugins_lookup_function(const char * name);

/*********************************************** misc utils ****************************************************/

#define perror_EXIT(msg) do{perror(msg); exit(EXIT_FAILURE);} while(0)
#define malloc_chk(ptr, size) do{ptr = malloc(size); if(ptr == NULL){perror_EXIT("malloc");}} while(0)
#define realloc_chk(ptr, size) do{if((ptr = realloc(ptr,size)) == NULL){perror_EXIT("realloc");}} while(0)
#define tspec_diff(s,e) ((e->tv_sec-s->tv_sec)*1000000000 + (e->tv_nsec-s->tv_nsec))
#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define str(s) #s
#define x_str(s) str(s)
#define monitor_print_err(...) fprintf(stderr, __FILE__  "(" x_str(__LINE__) "): " __VA_ARGS__)

#endif /* MONITOR_UTILS_H */
