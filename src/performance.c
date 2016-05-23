#include "performance.h"
#include "utils.h"
#include <dirent.h>
#include <dlfcn.h>

static int n_libs = 0;
static struct monitor_perf_lib ** libs = NULL;

static void __attribute__((destructor)) performance_at_exit(){
    while(n_libs--){
	dlclose(libs[n_libs]->dlhandle);
	free(libs[n_libs]->id);
	free(libs[n_libs]);
    }
    free(libs);
}

struct monitor_perf_lib * 
monitor_load_perf_lib(char * name){
    struct monitor_perf_lib * lib = NULL;
    int n;
    char * prefix_name;
    char path[256];

    /* Search if library already exists */
    for(n=0;n<n_libs;n++){
	if(!strcmp(libs[n]->id,name))
	    return libs[n];
    }
    
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
    libs = realloc(libs, sizeof(*libs)*(n_libs+1));
    libs[n_libs++] = lib;

    return lib;
}

