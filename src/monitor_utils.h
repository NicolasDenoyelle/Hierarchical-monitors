#ifndef MONITOR_UTILS_H
#define MONITOR_UTILS_H

#include <hwloc.h>
#include <stdio.h>

/************************************************ Array utils **************************************************/

struct hmon_array * new_hmon_array          (size_t elem_size, unsigned max_elem, void (*delete_element)(void*));
struct hmon_array * hmon_array_dup          (struct hmon_array *);
void           delete_hmon_array       (struct hmon_array *);
void           empty_hmon_array        (struct hmon_array *);
unsigned       hmon_array_length       (struct hmon_array *);
void *         hmon_array_get          (struct hmon_array *, unsigned);
void *         hmon_array_set          (struct hmon_array *, unsigned, void *);
void *         hmon_array_pop          (struct hmon_array *);
void           hmon_array_push         (struct hmon_array *, void *);
void *         hmon_array_remove       (struct hmon_array *, int);
void           hmon_array_insert       (struct hmon_array *, unsigned, void *);
unsigned       hmon_array_insert_sorted(struct hmon_array * array, void * element, int (* compare)(void*, void*));
void           hmon_array_sort         (struct hmon_array * array, int (* compare)(void*, void*));
int            hmon_array_find         (struct hmon_array * array, void * key, int (* compare)(void*, void*));

/*********************************************** hwloc utils ***************************************************/

int          location_cpubind(hwloc_obj_t);
int          location_membind(hwloc_obj_t);
char *       location_name   (hwloc_obj_t);
hwloc_obj_t  location_parse  (char *);
char **      location_avail  (unsigned *);
int          location_compare(void *, void *); /* void * must be: hwloc_obj_t */

/*********************************************** proc utils ****************************************************/

void  proc_get_allowed_cpuset(pid_t, hwloc_cpuset_t);
void  proc_get_running_cpuset(pid_t, hwloc_cpuset_t out, int recurse);
void  proc_move_tasks        (pid_t, hwloc_obj_t to, hwloc_obj_t from, int recurse);
pid_t start_executable       (char * exe, char * args[]);

/*********************************************** misc utils ****************************************************/

#define perror_EXIT(msg) do{perror(msg); exit(EXIT_FAILURE);} while(0)
#define malloc_chk(ptr, size) do{ptr = malloc(size); if(ptr == NULL){perror_EXIT("malloc");}} while(0)
#define realloc_chk(ptr, size) do{if((ptr = realloc(ptr,size)) == NULL){perror_EXIT("realloc");}} while(0)
#define tspec_diff(s,e) ((e->tv_sec-s->tv_sec)*1000000000 + (e->tv_nsec-s->tv_nsec))
#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define MIN(a,b) ((a)<(b) ? (a) : (b))
#define str(s) #s
#define x_str(s) str(s)

#define load_fun(lib,dlhandle,fun) do{					\
	lib->fun = dlsym(dlhandle,x_str(fun));				\
	if(lib->fun == NULL){						\
	    monitor_print_err("Failed to load function "x_str(fun)" from %s\n",path); \
	    monitor_print_err("%s\n", dlerror());			\
	}								\
    } while(0)
#define monitor_print_err(...) fprintf(stderr, __FILE__  "(" x_str(__LINE__) "): " __VA_ARGS__)

#endif /* MONITOR_UTILS_H */

