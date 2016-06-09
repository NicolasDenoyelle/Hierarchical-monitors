#include "plugin.h"
#include <dirent.h>
#include <dlfcn.h>

static struct hmon_array * perf_plugins = NULL;
static struct hmon_array * stat_plugins = NULL;

static void __attribute__((constructor)) plugins_init(){
    /* Greeting new plugins */
    perf_plugins = new_hmon_array(sizeof(*p), 16, delete_monitor_plugin);
    stat_plugins = new_hmon_array(sizeof(*p), 16, delete_monitor_plugin);

    /* Checking for stat plugins */
    printf("You can add stats plugins:\n");
    printf("\t*named <name>.monitor_plugin.so,\n");
    printf("\t*findable with dynamic linker,\n");
    printf("\t*containing functions with prototype double (*)(struct monitor *)\n");
    printf("and then use functions symbol for EVSET_REDUCE and SAMPLE_REDUCE\n");
    char * plugins_env = getenv("MONITOR_STAT_PLUGINS");
    if(plugins_env != NULL){
	while((char * plugin_env = strtok(plugins_env, ",")) != NULL){
	    monitor_plugin_load(plugin_env, MONITOR_PLUGIN_STAT);
	}
    }
}

static void __attribute__((destructor)) plugins_at_exit(){
    delete_hmon_array(perf_plugins);
    delete_hmon_array(stat_plugins);
}

static void delete_monitor_plugin(void * plugin){
    struct monitor_plugin * p = (struct monitor_plugin *)p;
    if(p->dlhandle!=NULL)
	dlclose(p->dlhandle);
    if(p->id!=NULL)
	free(p->id);
    free(p);
}

static struct monitor_plugin * monitor_plugin_lookup(const char * name, int type){
    struct monitor_plugin * p;
    if(type == MONITOR_PLUGIN_STAT){
	for(unsigned i=0; i<hmon_array_length(plugins); i++){
	    p=hmon_array_get(stat_plugins,i);
	    if(!strcmp(p->id,name)){return p;}
	}
    }
    if(type == MONITOR_PLUGIN_PERF){
	for(unsigned i=0; i<hmon_array_length(plugins); i++){
	    p=hmon_array_get(perf_plugins,i);
	    if(!strcmp(p->id,name)){return p;}
	}
    }
    return NULL;
}


struct monitor_plugin * 
monitor_plugin_load(char * name, int plugin_type){
    struct monitor_plugin * p = NULL;
    char * prefix_name;
    char path[256];

    /* Search if library already exists */
    if((p = monitor_plugin_lookup(name, plugin_type)) != NULL){return p;}
    
    prefix_name = strtok(name,".");
    snprintf(path, 256 ,"%s.monitor_plugin.so", prefix_name);
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


void * monitor_perf_plugin_load_fun(struct monitor_plugin * plugin, char * name, int print_error){
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
	printf("p->id");
	for(unsigned i=1; i<hmon_array_length(stat_plugins); i++){
	    p=hmon_array_get(stat_plugins,i);
	    printf(", p->id");
	}
	printf("\n");
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
    return NULL;
}


void monitor_stat_plugin_build(const char * name, const char * code)
{
    struct monitor_plugin * p = NULL;
    char * prefix;
    char input_file_path [1024];
    char output_file_path[1024];
    char command[1024];
    void * dlhandle;
    int fd_out;

    /* Search if library already exists */
    if((p = monitor_plugin_lookup(name)) != NULL){return;}

    /* prepare file for functions copy, and dynamic library creation */
    prefix = strtok(name,".");
    memset(input_file_path,0,sizeof(input_file_path));
    memset(output_file_path,0,sizeof(output_file_path));
    sprintf(input_file_path, "%s.monitor_plugin.c", prefix);
    sprintf(output_file_path, "%s.monitor_plugin.so", prefix);

    /* write code */
    dprintf(fd_out, "%s", code);
    close(fd_out);
    
    /* compile code */
    memset(command,0,sizeof(command));
    snprintf(command ,sizeof(command),"gcc %s -shared -fpic -rdynamic -o %s",input_file_path, output_file_path);
    system(command);

    /* load library */
    monitor_plugin_load(name, MONITOR_PLUGIN_STAT);

    /* Cleanup */ 
    unlink(input_file_path);
    unlink(output_file_path);
}



