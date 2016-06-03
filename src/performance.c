#include "performance.h"
#include "monitor_utils.h"
#include <dirent.h>
#include <dlfcn.h>

static struct array * libs = NULL;

static void __attribute__((destructor)) performance_at_exit(){
    if(libs != NULL){delete_array(libs);}
}

static void delete_perf_lib(void * lib){
    struct monitor_perf_lib * perf_lib = (struct monitor_perf_lib *)lib;
    dlclose(perf_lib->dlhandle);
    free(perf_lib->id);
    free(perf_lib);
}

struct monitor_perf_lib * 
monitor_load_perf_lib(char * name){
    struct monitor_perf_lib * lib = NULL;
    char * prefix_name;
    char path[256];

    /* Search if library already exists */
    if(libs == NULL){libs = new_array(sizeof(*lib), 16, delete_perf_lib);}
    while((lib = array_iterate(libs)) != NULL){if(!strcmp(lib->id,name)){return lib;}}
    
    prefix_name = strtok(name,".");
    snprintf(path, 256 ,"%s.monitor_plugin.so", prefix_name);
    malloc_chk(lib, sizeof(*lib));
    dlerror();
    lib->dlhandle = dlopen(path,RTLD_LAZY|RTLD_GLOBAL);

    if(lib->dlhandle==NULL){
	monitor_print_err("Dynamic library %s load error:%s\n",path,dlerror());
	exit(EXIT_FAILURE);
    }  
    
    dlerror();
    lib->id = strdup(name);
    load_fun(lib,lib->dlhandle,monitor_events_list);
    load_fun(lib,lib->dlhandle,monitor_eventset_init);
    load_fun(lib,lib->dlhandle,monitor_eventset_init_fini);
    load_fun(lib,lib->dlhandle,monitor_eventset_destroy);
    load_fun(lib,lib->dlhandle,monitor_eventset_add_named_event);
    load_fun(lib,lib->dlhandle,monitor_eventset_start);
    load_fun(lib,lib->dlhandle,monitor_eventset_stop);
    load_fun(lib,lib->dlhandle,monitor_eventset_reset);
    load_fun(lib,lib->dlhandle,monitor_eventset_read);
    array_push(libs, lib);
    return lib;
}

