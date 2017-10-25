#include "internal.h"
#include <hmon/harray.h>

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <config.h>

static const char * plugin_suffix = "_hmon_plugin.so";
static harray perf_plugins = NULL;
static harray stat_plugins = NULL;

#ifndef STAT_PLUGINS
#define STAT_PLUGINS "defstats"
#endif

#ifndef PERF_PLUGINS
#define PERF_PLUGINS "proc accumulate hierarchical"
#endif

static char * unsuffix_plugin_name(const char * name){
  size_t namelen = strlen(name);
  size_t suffixlen = strlen(plugin_suffix);   
  if(namelen < suffixlen){return strdup(name);}
  if(!strncmp(plugin_suffix, &name[namelen-suffixlen], suffixlen)){return strndup(name, namelen-suffixlen);}
  return strdup(name);
}

static void delete_hmon_plugin(void * plugin){
  struct hmon_plugin * p = (struct hmon_plugin *)plugin;
  if(p->dlhandle!=NULL)
    dlclose(p->dlhandle);
  if(p->id!=NULL)
    free(p->id);
  free(p);
}

static struct hmon_plugin * hmon_plugin_load(const char * name, int plugin_type){
  struct hmon_plugin * p = NULL;
  char * prefix;
  char path[256]; memset(path,0,sizeof(path));

  /* Search if library already exists */
  if((p = hmon_plugin_lookup(name, plugin_type)) != NULL){return p;}
  prefix = unsuffix_plugin_name(name);
  snprintf(path, 256 ,"%s_hmon_plugin.so", prefix);
  free(prefix);
  malloc_chk(p, sizeof(*p));
  p->dlhandle = NULL;
  p->id = NULL;
  dlerror();
  p->dlhandle = dlopen(path,RTLD_LAZY|RTLD_GLOBAL);
  if(p->dlhandle==NULL){
    monitor_print_err("Dynamic library %s load error:%s\n",path,dlerror());
    delete_hmon_plugin(p);
    return NULL;
  }
    
  dlerror();
  p->id = strdup(name);
  switch(plugin_type){
  case HMON_PLUGIN_STAT:
    harray_push(stat_plugins, p);
    break;
  case HMON_PLUGIN_PERF:
    harray_push(perf_plugins, p);
    break;
  default:
    delete_hmon_plugin(p);
    return NULL;
    break;
  }
  return p;
}

static void __attribute__((constructor)) plugins_init(){
  /* Greeting new plugins */
  perf_plugins = new_harray(sizeof(struct hmon_plugin*), 16, delete_hmon_plugin);
  stat_plugins = new_harray(sizeof(struct hmon_plugin*), 16, delete_hmon_plugin);

  /* Checking for stat plugins */
  char * plugins_env = getenv("HMON_STAT_PLUGINS");
  char * plugin_env;
  if(plugins_env != NULL){
    plugin_env = strtok(plugins_env, " ");
    if(plugin_env != NULL) do{
	hmon_plugin_load(plugin_env, HMON_PLUGIN_STAT);
      } while((plugin_env = strtok(NULL, " ")) != NULL);
  }
  char * plugins = strdup(STAT_PLUGINS), * save_ptr;
  char * plugin = strtok_r(plugins, " ", &save_ptr);
  do{
    hmon_plugin_load(plugin, HMON_PLUGIN_STAT);
    plugin = strtok_r(NULL, " ", &save_ptr);
  } while(plugin != NULL);
  free(plugins);

  /* Checking for perf plugins */
  plugins_env = getenv("HMON_PERF_PLUGINS");
  if(plugins_env != NULL){
    plugin_env = strtok(plugins_env, " ");
    if(plugin_env != NULL) do{
	hmon_plugin_load(plugin_env, HMON_PLUGIN_PERF);
      } while((plugin_env = strtok(NULL, " ")) != NULL);
  }
  plugins = strdup(PERF_PLUGINS), * save_ptr;
  plugin = strtok_r(plugins, " ", &save_ptr);
  do{
    hmon_plugin_load(plugin, HMON_PLUGIN_PERF);
    plugin = strtok_r(NULL, " ", &save_ptr);
  } while(plugin != NULL);
  free(plugins);  
}


static void __attribute__((destructor)) plugins_at_exit(){
  delete_harray(perf_plugins);
  delete_harray(stat_plugins);
}


struct hmon_plugin * hmon_plugin_lookup(const char * name, int type){
  unsigned i;
  struct hmon_plugin * p;
  if(type == HMON_PLUGIN_STAT){
    for(i=0; i<harray_length(stat_plugins); i++){
      p=harray_get(stat_plugins,i);
      if(!strcmp(p->id,name)){return p;}
    }
  }
  else if(type == HMON_PLUGIN_PERF){
    for(i=0; i<harray_length(perf_plugins); i++){
      p=harray_get(perf_plugins,i);
      if(!strcmp(p->id,name)){return p;}
    }
  }
  return NULL;
}

void * hmon_plugin_load_fun(struct hmon_plugin * plugin, const char * name, int print_error){
  void * fun = dlsym(plugin->dlhandle,name);
  if(print_error && fun == NULL){
    monitor_print_err("Failed to load function %s\n",name);
    monitor_print_err("%s\n", dlerror());
  }
  return fun;
}

void hmon_perf_plugins_list(){
  unsigned i;
  struct hmon_plugin * p;
  if(harray_length(perf_plugins)>0){
    p=harray_get(perf_plugins,0);
    printf( "%s", p->id);
    for(i=1; i<harray_length(perf_plugins); i++){
      p=harray_get(perf_plugins,i);
      printf( ", %s", p->id);
    }
    printf( "\n");
  }
}

void hmon_stat_plugins_list(){
  unsigned i;
  struct hmon_plugin * p;
  if(harray_length(stat_plugins)>0){
    p=harray_get(stat_plugins,0);
    printf( "%s", p->id);
    for(i=1; i<harray_length(stat_plugins); i++){
      p=harray_get(stat_plugins,i);
      printf( ", %s", p->id);
    }
    printf( "\n");
  }
}


void * hmon_stat_plugins_lookup_function(const char * name){
  unsigned i;
  struct hmon_plugin * p;
  void * fun = NULL;
  for(i=0; i<harray_length(stat_plugins); i++){
    p=harray_get(stat_plugins,i);
    fun = hmon_plugin_load_fun(p,name,0);
    if(fun != NULL)
      return fun;
  }
  fprintf(stderr,"Could not find function %s among following plugins:\n", name);
  hmon_stat_plugins_list();
  return NULL;
}

void hmon_stat_plugin_build(const char * name, const char * code)
{
  struct hmon_plugin * p = NULL;
  char * prefix;
  char input_file_path [1024];
  char output_file_path[1024];
  char command[1024];
  FILE * fout = NULL;

  /* Search if library already exists */
  if((p = hmon_plugin_lookup(name, HMON_PLUGIN_STAT)) != NULL){return;}

  /* prepare file for functions copy, and dynamic library creation */
  prefix = unsuffix_plugin_name(name);
  memset(input_file_path,0,sizeof(input_file_path));
  memset(output_file_path,0,sizeof(output_file_path));
  sprintf(input_file_path, "%s_hmon_plugin.c", prefix);
  sprintf(output_file_path, "%s_hmon_plugin.so", prefix);
  free(prefix);

  /* write code */
  fout = fopen(input_file_path, "w+");
  fprintf(fout, "%s", code);
  fclose(fout);
    
  /* compile code */
  memset(command,0,sizeof(command));
  snprintf(command ,sizeof(command),"%s %s -shared -fpic -rdynamic -o %s", x_str(CC), input_file_path, output_file_path);
  int err = system(command);
  switch(err){
  case -1:
    fprintf(stderr, "system call failed");
    goto cleanup;
    break;
  case EXIT_FAILURE:
    fprintf(stderr, "Plugin compilation failed");
    goto cleanup;
    break;
  default:
    /* load library */
    hmon_plugin_load(name, HMON_PLUGIN_STAT);
    break;
  }

cleanup:;
  /* Cleanup */ 
  unlink(input_file_path);
  unlink(output_file_path);
}



