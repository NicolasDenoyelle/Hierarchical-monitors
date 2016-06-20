#include "monitor_utils.h"
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static struct hmon_array * perf_plugins = NULL;
static struct hmon_array * stat_plugins = NULL;

/* #ifndef STAT_PLUGINS */
/* #define STAT_PLUGINS "stat_default" */
/* #endif */

static void delete_monitor_plugin(void * plugin){
    struct monitor_plugin * p = (struct monitor_plugin *)plugin;
    if(p->dlhandle!=NULL)
	dlclose(p->dlhandle);
    if(p->id!=NULL)
	free(p->id);
    free(p);
}

static void __attribute__((constructor)) plugins_init(){
    /* Greeting new plugins */
    perf_plugins = new_hmon_array(sizeof(struct monitor_plugin*), 16, delete_monitor_plugin);
    stat_plugins = new_hmon_array(sizeof(struct monitor_plugin*), 16, delete_monitor_plugin);

    /* Checking for stat plugins */
    char * plugins_env = getenv("MONITOR_STAT_PLUGINS");
    char * plugin_env;
    if(plugins_env != NULL){
	plugin_env = strtok(plugins_env, " ");
	if(plugin_env != NULL) do{
	    monitor_plugin_load(plugin_env, MONITOR_PLUGIN_STAT);
	} while((plugin_env = strtok(NULL, " ")) != NULL);
    }
    char * plugins = strdup(STAT_PLUGINS), * save_ptr;
    char * plugin = strtok_r(plugins, " ", &save_ptr);
    do{
	monitor_plugin_load(plugin, MONITOR_PLUGIN_STAT);
	plugin = strtok_r(NULL, " ", &save_ptr);
    } while(plugin != NULL);
    free(plugins);
}


static void __attribute__((destructor)) plugins_at_exit(){
    delete_hmon_array(perf_plugins);
    delete_hmon_array(stat_plugins);
}


static struct monitor_plugin * monitor_plugin_lookup(const char * name, int type){
    struct monitor_plugin * p;
    if(type == MONITOR_PLUGIN_STAT){
	for(unsigned i=0; i<hmon_array_length(stat_plugins); i++){
	    p=hmon_array_get(stat_plugins,i);
	    if(!strcmp(p->id,name)){return p;}
	}
    }
    else if(type == MONITOR_PLUGIN_PERF){
	for(unsigned i=0; i<hmon_array_length(perf_plugins); i++){
	    p=hmon_array_get(perf_plugins,i);
	    if(!strcmp(p->id,name)){return p;}
	}
    }
    return NULL;
}


struct monitor_plugin * monitor_plugin_load(const char * name, int plugin_type){
    struct monitor_plugin * p = NULL;
    char * prefix_name , * cpy_name;
    char path[256];

    /* Search if library already exists */
    if((p = monitor_plugin_lookup(name, plugin_type)) != NULL){return p;}
    cpy_name = strdup(name);
    prefix_name = strtok(cpy_name,".");
    snprintf(path, 256 ,"%s.monitor_plugin.so", prefix_name);
    free(cpy_name);
    malloc_chk(p, sizeof(*p));
    p->dlhandle = NULL;
    p->id = NULL;
    dlerror();
    p->dlhandle = dlopen(path,RTLD_LAZY|RTLD_GLOBAL);
    if(p->dlhandle==NULL){
	monitor_print_err("Dynamic library %s load error:%s\n",path,dlerror());
	delete_monitor_plugin(p);
	return NULL;
    }
    
    dlerror();
    p->id = strdup(name);
    switch(plugin_type){
    case MONITOR_PLUGIN_STAT:
	hmon_array_push(stat_plugins, p);
	break;
    case MONITOR_PLUGIN_PERF:
	hmon_array_push(perf_plugins, p);
	break;
    default:
	delete_monitor_plugin(p);
	return NULL;
	break;
    }
    return p;
}


void * monitor_plugin_load_fun(struct monitor_plugin * plugin, const char * name, int print_error){
	void * fun = dlsym(plugin->dlhandle,name);
	if(print_error && fun == NULL){
	    monitor_print_err("Failed to load function %s\n",name);
	    monitor_print_err("%s\n", dlerror());
	}
	return fun;
}

void monitor_stat_plugins_list(){
    struct monitor_plugin * p;
    if(hmon_array_length(stat_plugins)>0){
	p=hmon_array_get(stat_plugins,0);
	fprintf(stderr, "%s", p->id);
	for(unsigned i=1; i<hmon_array_length(stat_plugins); i++){
	    p=hmon_array_get(stat_plugins,i);
	    fprintf(stderr, ", %s", p->id);
	}
	fprintf(stderr, "\n");
    }
}


void * monitor_stat_plugins_lookup_function(const char * name){
    struct monitor_plugin * p;
    void * fun = NULL;
    for(unsigned i=0; i<hmon_array_length(stat_plugins); i++){
	p=hmon_array_get(stat_plugins,i);
	fun = monitor_plugin_load_fun(p,name,0);
	if(fun != NULL)
	    return fun;
    }
    fprintf(stderr,"Could not find function %s among following plugins:\n", name);
    monitor_stat_plugins_list();
    return NULL;
}


void monitor_stat_plugin_build(const char * name, const char * code)
{
    struct monitor_plugin * p = NULL;
    char * prefix, * cpy;
    char input_file_path [1024];
    char output_file_path[1024];
    char command[1024];
    FILE * fout = NULL;

    /* Search if library already exists */
    if((p = monitor_plugin_lookup(name, MONITOR_PLUGIN_STAT)) != NULL){return;}

    /* prepare file for functions copy, and dynamic library creation */
    cpy = strdup(name);
    prefix = strtok(cpy,".");
    memset(input_file_path,0,sizeof(input_file_path));
    memset(output_file_path,0,sizeof(output_file_path));
    sprintf(input_file_path, "%s.monitor_plugin.c", prefix);
    sprintf(output_file_path, "%s.monitor_plugin.so", prefix);
    free(cpy);

    /* write code */
    fout = fopen(input_file_path, "w+");
    fprintf(fout, "%s", code);
    fclose(fout);
    
    /* compile code */
    memset(command,0,sizeof(command));
    snprintf(command ,sizeof(command),"gcc %s -shared -fpic -rdynamic -o %s",input_file_path, output_file_path);
    system(command);

    /* load library */
    monitor_plugin_load(name, MONITOR_PLUGIN_STAT);

    /* Cleanup */ 
    /* unlink(input_file_path); */
    /* unlink(output_file_path); */
}



