#include <string.h>
#include <dirent.h>
#include <dlfcn.h>
#include "monitor_utils.h"
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
    if(!strcmp(name,"SAMPLES_LAST"))
	return monitor_samples_last;
    else if(!strcmp(name,"EVSET_MAX"))
	return monitor_evset_max;
    else if(!strcmp(name,"EVSET_MIN"))
	return monitor_evset_min;
    else if(!strcmp(name,"EVSET_SUM"))
	return monitor_evset_sum;
    else 
	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                                 Plugin loaders                                          //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

const  char * plugin_suffix = ".stats_monitor.so" ;
static struct array * libs = NULL;

static void __attribute__((destructor)) stats_at_exit(){
    if(libs != NULL){delete_array(libs);}
}

static void delete_stats_lib(void * lib){
    struct monitor_stats_lib * stats_lib = (struct monitor_stats_lib *)lib;
    free(stats_lib->id);
    free(stats_lib);
}

struct monitor_stats_lib * monitor_build_custom_stats_lib(const char * name, const char * code){
    struct monitor_stats_lib * lib;
    char prefix[1024];
    char input_file_path [1024];
    char output_file_path[1024];
    char command[1024];
    void * dlhandle;
    int fd_out;
    double (*fun)(struct monitor*);

    /* Search if library already exists */
    if(libs == NULL){libs = new_array(sizeof(*lib), 16, delete_stats_lib);}
    while((lib = array_iterate(libs)) != NULL){if(!strcmp(lib->id,name)){return lib;}}

    /* prepare file for functions copy, and dynamic library creation */
    memset(prefix,0,sizeof(prefix));
    memset(input_file_path,0,sizeof(input_file_path));
    memset(output_file_path,0,sizeof(output_file_path));
    snprintf(prefix, sizeof(prefix), "%s.XXXXXX", name);
    sprintf( input_file_path, "%s.c",  prefix);
    fd_out = mkstemps(input_file_path,2);
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
    memset(command,0,sizeof(command));
    snprintf(command ,sizeof(command),"gcc %s -shared -fpic -rdynamic -o %s",input_file_path, output_file_path);
    system(command);

    /* load library */
    dlhandle = dlopen(output_file_path,RTLD_NOW);
    if(dlhandle==NULL){
	monitor_print_err("Dynamic library %s load error:%s\n", output_file_path,dlerror());
	exit(EXIT_FAILURE);
    }
    dlerror();
    fun = (double (*)(struct monitor*)) dlsym(dlhandle,name);
    if(fun == NULL){
	monitor_print_err("Cannot load aggregation function %s\n", name); 
	exit(EXIT_FAILURE);
    }
    
    /* allocate lib */
    malloc_chk(lib, sizeof(*lib));
    lib->dlhandle = dlhandle;
    lib->call = fun;
    lib->id = strdup(name);
    array_push(libs, lib);

    /* Cleanup */ 
    unlink(input_file_path);
    unlink(output_file_path);

    return lib;
}

struct monitor_stats_lib * monitor_load_stats_lib(char * name){
    char * prefix_name;
    char path[256];
    struct monitor_stats_lib * lib;

    /* Search if library already exists */
    if(libs == NULL){libs = new_array(sizeof(*lib), 16, delete_stats_lib);}
    while((lib = array_iterate(libs)) != NULL){if(!strcmp(lib->id,name)){return lib;}}

    malloc_chk(lib, sizeof *lib);
    lib->dlhandle = NULL;
    
    /* If name matches a native plugin, then return it */
    lib->call = stats_plugin_name_to_function(name);
    if(lib->call){return lib;}

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
	free(lib);
	return NULL;
    }

    lib->id = strdup(name);
    array_push(libs, lib);
    return lib;
}

