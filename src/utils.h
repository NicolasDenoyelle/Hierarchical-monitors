#ifndef MONITOR_UTILS_H
#define MONITOR_UTILS_H

#include <hwloc.h>
#include <stdio.h>

/************************************************ Array utils **************************************************/

struct array * new_array          (size_t elem_size, unsigned max_elem, void (*delete_element)(void*));
struct array * array_dup          (struct array *);
void           delete_array       (struct array *);
void           empty_array        (struct array *);
unsigned       array_length       (struct array *);
void *         array_get          (struct array *, unsigned);
void *         array_set          (struct array *, unsigned, void *);
void *         array_pop          (struct array *);
void           array_push         (struct array *, void *);
void *         array_remove       (struct array *, int);
void           array_insert       (struct array *, unsigned, void *);
unsigned       array_insert_sorted(struct array * array, void * element, int (* compare)(void*, void*));
void           array_sort         (struct array * array, int (* compare)(void*, void*));
int            array_find         (struct array * array, void * key, int (* compare)(void*, void*));
void *         array_iterate      (struct array *);

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

