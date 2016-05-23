#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include "utils.h"
#include "stats.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                Stats functions                                          //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

double monitor_samples_last(struct monitor * monitor){
    return monitor->samples[monitor->current];
}

double monitor_evset_max(struct monitor * monitor){
    unsigned c = monitor->current, n = monitor->n_events;
    double max = monitor->events[c][--n];
    while(n-->0)
	max = max > monitor->events[c][n] ? max : monitor->events[c][n];
    return max;
}

double monitor_evset_min(struct monitor * monitor){
    unsigned c = monitor->current, n = monitor->n_events;
    double min = monitor->events[c][--n];
    while(n-->0)
	min = min < monitor->events[c][n] ? min : monitor->events[c][n];
    return min;
}

double monitor_evset_sum(struct monitor * monitor){
    double result = 0;
    unsigned c = monitor->current, n = monitor->n_events;
    while(n--)
	result+=monitor->events[c][n];
    return result;
}

static double (* stats_plugin_name_to_function(const char * name))(struct monitor *){
    if(!strcmp(name,"MONITOR_SAMPLES_LAST"))
	return monitor_samples_last;
    else if(!strcmp(name,"MONITOR_EVSET_MAX"))
	return monitor_evset_max;
    else if(!strcmp(name,"MONITOR_EVSET_MIN"))
	return monitor_evset_min;
    else if(!strcmp(name,"MONITOR_EVSET_SUM"))
	return monitor_evset_sum;
    else 
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                 Plugin loaders                                          //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

const  char * plugin_suffix = ".stats_monitor.so" ;
static int n_libs = 0;
static struct monitor_stats_lib ** libs = NULL;

static void __attribute__((destructor)) stats_at_exit(){
    while(n_libs--){
	dlclose(libs[n_libs]->dlhandle);
	free(libs[n_libs]->id);
	free(libs[n_libs]);
    }
    free(libs);
}

struct monitor_stats_lib * monitor_build_custom_stats_lib(const char * name, const char * code){
    int n;
    /* Search if library already exists */
    for(n=0;n<n_libs;n++){
	if(!strcmp(libs[n]->id,name))
	    return libs[n];
    }

    /* prepare file for functions copy, and dynamic library creation */
    char prefix[1024]; memset(prefix,0,sizeof(prefix));
    snprintf(prefix, sizeof(prefix), "%s.XXXXXX", name);
    char input_file_path [strlen(prefix)+3]; memset( input_file_path,0,sizeof( input_file_path));
    char output_file_path[strlen(prefix)+4]; memset(output_file_path,0,sizeof(output_file_path));
    sprintf( input_file_path, "%s.c",  prefix);
    int fd_out = mkstemps(input_file_path,2);
    if(fd_out == -1){
	perror("mkstemps");
	monitor_print_err("failed to create a temporary file %s\n",input_file_path);
	exit(EXIT_FAILURE);
    }
    strncpy(output_file_path, input_file_path, strlen(prefix));
    strcat(output_file_path,".so");

    /* write aggregation code */
    dprintf(fd_out, "#include <monitor.h>\n");
    dprintf(fd_out, "double %s(struct monitor * monitor){return %s;}\n", name, code);
    close(fd_out);
    
    /* compile aggregation code */
    char command[1024]; memset(command,0,sizeof(command));
    snprintf(command ,sizeof(command),"gcc %s -shared -fpic -rdynamic -o %s",input_file_path, output_file_path);
    system(command);

    /* load library */
    void * dlhandle = dlopen(output_file_path,RTLD_NOW);
    if(dlhandle==NULL){
	monitor_print_err("Dynamic library %s load error:%s\n", output_file_path,dlerror());
	exit(EXIT_FAILURE);
    }
    dlerror();
    double (*fun)(struct monitor*) = (double (*)(struct monitor*)) dlsym(dlhandle,name);
    if(fun == NULL){
	monitor_print_err("Cannot load aggregation function %s\n", name); 
	exit(EXIT_FAILURE);
    }
    
    /* allocate lib */
    struct monitor_stats_lib * lib;
    malloc_chk(lib, sizeof(*lib));
    lib->dlhandle = dlhandle;
    lib->call = fun;
    lib->id = strdup(name);
    libs = realloc(libs, sizeof(*libs)*(n_libs+1));
    libs[n_libs++] = lib;

    /* Cleanup */ 
    unlink(input_file_path);
    unlink(output_file_path);

    return lib;
}

struct monitor_stats_lib * monitor_load_stats_lib(char * name){
    char * prefix_name;
    char path[256];
    struct monitor_stats_lib * lib;
    int n;

    /* Search if library already exists */
    for(n=0;n<n_libs;n++){
	if(!strcmp(libs[n]->id,name))
	    return libs[n];
    }

    malloc_chk(lib, sizeof *lib);
    lib->dlhandle = NULL;
    
    /* If name matches a native plugin, then return it */
    lib->call = stats_plugin_name_to_function(name);
    if(lib->call)
	return lib;

    prefix_name = strtok(name,".");
    snprintf(path, 256 ,"%s.monitor_plugin.so", prefix_name);

    /* load shared libraries */
    dlerror();
    lib->dlhandle = dlopen(path,RTLD_LAZY|RTLD_GLOBAL);

    if(lib->dlhandle==NULL){
	monitor_print_err("Dynamic library %s load error:%s\n",path,dlerror());
	exit(EXIT_FAILURE);
    }  

    load_fun(lib,lib->dlhandle,call);
    if(lib->call == NULL){
	monitor_print_err("Library %s exists but does not contain required call() function. See stats_interface.h\n",name);
	dlclose(lib->dlhandle);
	return NULL;
    }

    lib->id = strdup(name);
    libs = realloc(libs, sizeof(*libs)*(n_libs+1));
    libs[n_libs++] = lib;
    return lib;
}


void
monitor_unload_stats_lib(struct monitor_stats_lib * lib)
{
    if(lib->dlhandle)
	dlclose(lib->dlhandle);
    free(lib);
}




