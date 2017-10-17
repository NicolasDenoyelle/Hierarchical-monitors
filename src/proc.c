#include <stdlib.h>
#include <stdio.h>
#include <hwloc.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "./internal.h"

static DIR * proc_open_dir(const char * path){
    DIR * dir = NULL;
    dir = opendir(path);
  if(dir==NULL)
      perror("attach opendir:");
 return dir;
}

static FILE * proc_open_file(const char * path){
  FILE * proc_file = fopen(path, "r");
  if(proc_file == NULL){
    monitor_print_err("Can't open %s for reading", path);
    return NULL;
  }
  return proc_file;
}

/****************************************************************************************************************/
/*                                          Looking at tasks placement                                          */
/****************************************************************************************************************/

#define SLEEPING 0
#define RUNNING 1

struct proc_task{
    int pu_num;
    int state;
};

static int
read_proc_stats(const char * proc_path, struct proc_task * p_info)
{
    unsigned pu_num;
    char state;  
    FILE * proc_file = proc_open_file(proc_path);
    if(proc_file == NULL){
	p_info->state=0;
	return -1;
    }
    int err = fscanf(proc_file,"%*d %*s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %*u %*u %*d %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*u %*d %d",&state,&pu_num);
    fclose(proc_file);
    if(err==EOF){perror("fscanf"); return -1;}
    p_info->pu_num = pu_num;
    if(state!='R')
	p_info->state=SLEEPING;
    else
	p_info->state=RUNNING;
    return 0;
}

#define str_append(str, max_size, ...) (strlen(str) + snprintf(str+strlen(str), max_size-strlen(str), __VA_ARGS__))

static void running_cpuset_and(char * task_dir_name, size_t max_len, hwloc_cpuset_t cpuset, int recurse){
    struct dirent * task_dirent;
    DIR * task_dir;
    struct proc_task task_infos;
    size_t len, t_len;


    /* read task infos for this task */
    len = strlen(task_dir_name);
    snprintf(task_dir_name+len,max_len-len, "/stat");
    read_proc_stats(task_dir_name,&task_infos);
    if(task_infos.state == RUNNING)
	hwloc_bitmap_set(cpuset,task_infos.pu_num);
    memset(task_dir_name + len, 0, 5);

    /* recurse for child tasks */
    if(recurse){
	snprintf(task_dir_name+len, max_len-len, "/task/");
	if((task_dir = proc_open_dir(task_dir_name)) != NULL){
	    while((task_dirent=readdir(task_dir))!=NULL){
		if(!strcmp(task_dirent->d_name,".") || !strcmp(task_dirent->d_name,".."))
		    continue;
		t_len = snprintf(task_dir_name+len+6, max_len-len-6, "%s", task_dirent->d_name);
		running_cpuset_and(task_dir_name, max_len, cpuset, recurse-1);
		memset(task_dir_name+len+6, 0, t_len);
	    }
	    rewinddir(task_dir);
	    closedir(task_dir);
	}
	memset(task_dir_name+len, 0, 6);
    }
    return;
}

void
proc_get_running_cpuset(pid_t pid, hwloc_cpuset_t cpuset, int recurse)
{
    char proc_path[128];
    memset(proc_path, 0, sizeof(proc_path));
    hwloc_bitmap_zero(cpuset);
    snprintf(proc_path,128,"/proc/%d",(int)pid);
    running_cpuset_and(proc_path,128,cpuset, recurse);
}

void proc_get_allowed_cpuset(pid_t pid, hwloc_cpuset_t cpuset){
    FILE * status_file = NULL;
    char path[1024];
    const char * token = "Cpus_allowed:";
    char * line = NULL;
    ssize_t err;
    size_t read, token_length;

    memset(path, 0, sizeof(path));
    snprintf(path, sizeof(path), "/proc/%ld/status", (long)pid);
    token_length = strlen(token);
    status_file = proc_open_file(path);
    if(status_file == NULL){goto exit_error;}
    
    line = malloc(8);
    do{
	free(line); line = NULL;
	err = getline(&line, &read, status_file);
    } while(err!=-1 && strncmp(line, token, token_length));
    
    fclose(status_file);
    if(err == -1){goto exit_error_with_line;}
    
    line[strlen(line)-1] = '\0';
    hwloc_bitmap_sscanf(cpuset, line+token_length+1);
    free(line);
    return;

exit_error_with_line:
    free(line);
exit_error:
    hwloc_bitmap_fill(cpuset);
}


/* /\***************************************************************************************************************\/ */
/* /\*                                             Looking at cpu usage                                            *\/ */
/* /\***************************************************************************************************************\/ */

/* struct proc_cpu{ */
/*   hwloc_obj_t location; */
/*   unsigned long user[2];   /\* normal processes executing in user mode *\/ */
/*   unsigned long nice[2];   /\* niced processes executing in user mode *\/ */
/*   unsigned long system[2]; /\* processes executing in kernel mode *\/ */
/*   unsigned long idle[2];   /\* twiddling thumbs *\/ */
/*   unsigned long iowait[2]; /\* waiting for I/O to complete *\/ */
/*   unsigned long irq[2];    /\* servicing interrupts *\/ */
/*   unsigned long softirq[2];/\* servicing softirqs *\/ */
/* }; */


/* struct proc_cpu * new_proc_cpu(hwloc_obj_t location){ */
/*   if(location->type != HWLOC_OBJ_PU && location->type != HWLOC_OBJ_MACHINE){ */
/*     monitor_print_err("Location of object collecting cpu info must be either HWLOC_OBJ_PU or HWLOC_OBJ_MACHINE (currently on %s)\n", */
/* 		      hwloc_type_name(location->type)); */
/*     return NULL; */
/*   } */
  
/*   struct proc_cpu * p; */
/*   malloc_chk(p, sizeof(*p)); */
/*   p->location   = location;   */
/*   p->user[0]    = p->user[1] = 0; */
/*   p->nice[0]    = p->nice[1] = 0; */
/*   p->system[0]  = p->system[1] = 0; */
/*   p->idle[0]    = p->idle[1] = 0; */
/*   p->iowait[0]  = p->iowait[1] = 0; */
/*   p->irq[0]     = p->irq[1] = 0; */
/*   p->softirq[0] = p->softirq[1] = 0; */
/*   return p; */
/* } */

/* void delete_proc_cpu(struct proc_cpu * p){free(p);} */

/* int proc_cpu_read(struct proc_cpu * p){ */
/*   FILE * proc_file = proc_open_file("/proc/stat"); */
/*   if(proc_file == NULL){return -1;} */

/*   /\* Save old values *\/ */
/*   p->user[0]    = p->user[1]; */
/*   p->nice[0]    = p->nice[1]; */
/*   p->system[0]  = p->system[1]; */
/*   p->idle[0]    = p->idle[1]; */
/*   p->iowait[0]  = p->iowait[1]; */
/*   p->irq[0]     = p->irq[1]; */
/*   p->softirq[0] = p->softirq[1]; */

/*   /\* Read new values *\/ */
/*   char * line = NULL; */
/*   size_t len = 0; */

/*   if((len = getline(&line, &len, proc_file)) == -1){goto getline_error;} */
/*   if((sscanf(line,"cpu %lu %lu %lu %lu %lu %lu %lu\n", */
/* 		       &(p->user[1]), */
/* 		       &(p->nice[1]), */
/* 		       &(p->system[1]), */
/* 		       &(p->idle[1]), */
/* 		       &(p->iowait[1]), */
/* 		       &(p->irq[1]), */
/* 	     &(p->softirq[1]))) == EOF){goto sscanf_error;} */
  
/*   if(p->location->type == HWLOC_OBJ_PU){ */
/*     unsigned long cpu_num = 0; */
/*     do{ */
/*       len = 0; free(line); line = NULL; */
/*       if((len = getline(&line, &len, proc_file)) == -1){goto getline_error;} */
/*       if((sscanf(line,"cpu%lu %lu %lu %lu %lu %lu %lu %lu\n", */
/* 		 &cpu_num, */
/* 		 &(p->user[1]), */
/* 		 &(p->nice[1]), */
/* 		 &(p->system[1]), */
/* 		 &(p->idle[1]), */
/* 		 &(p->iowait[1]), */
/* 		 &(p->irq[1]), */
/* 		 &(p->softirq[1]))) == EOF){goto sscanf_error;} */
/*     } while(cpu_num != p->location->os_index); */
/*   } */

/*   free(line); */
/*   fclose(proc_file); */
/*   return 0; */
/* getline_error: */
/*   fclose(proc_file); */
/*   {perror("getline"); return -1;} */
/*   return -1; */
/* sscanf_error: */
/*   free(line); */
/*   fclose(proc_file); */
/*   {perror("sscanf"); return -1;} */
/*   return -1; */
/* } */

/* double proc_cpu_load(struct proc_cpu * p){ */
/*   double usage = (p->user[1]-p->user[0]) + (p->nice[1]-p->nice[0]) + (p->system[1]-p->system[0]); */
/*   return usage == 0 ? 0 : 100*usage / (usage + (p->idle[1]-p->idle[0])); */
/* } */

/* /\***************************************************************************************************************\/ */
/* /\*                                             Looking at mem usage                                            *\/ */
/* /\***************************************************************************************************************\/ */

/* struct proc_mem{ */
/*   hwloc_obj_t location; */
/*   unsigned long total;  /\* Total memory *\/ */
/*   unsigned long free;   /\* Free memory *\/ */
/*   unsigned long used;   /\* Used memory *\/ */
/* }; */

/* struct proc_mem * new_proc_mem(hwloc_obj_t location){ */
/*   if(location->type != HWLOC_OBJ_NUMANODE && location->type != HWLOC_OBJ_MACHINE){ */
/*     monitor_print_err("Location of object collecting mem info must be either HWLOC_OBJ_NUMANODE or HWLOC_OBJ_MACHINE (currently on %s)\n", */
/* 		      hwloc_type_name(location->type)); */
/*     return NULL; */
/*   } */
  
/*   struct proc_mem * p; */
/*   malloc_chk(p, sizeof(*p)); */
/*   p->location   = location;   */
/*   p->free = 0; */
/*   p->used = 0; */
/*   return p; */
/* } */

/* void delete_proc_mem(struct proc_mem * p){free(p);} */

/* int proc_mem_read(struct proc_mem * p){ */
/*   char path[1024]; memset(path,0,sizeof(path)); */

/*   if(p->location->type == HWLOC_OBJ_NUMANODE){ */
/*     int nid = p->location->os_index; */
/*     snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/meminfo", nid); */
/*   } */
/*   else */
/*     snprintf(path, sizeof(path), "/proc/meminfo"); */

/*   FILE * proc_file = proc_open_file(path); */
/*   if(proc_file == NULL){return -1;} */

/*   if(p->location->type == HWLOC_OBJ_NUMANODE){ */
/*     /\* Read new values *\/ */
/*     if(fscanf(proc_file,"Node %*d MemTotal: %lu kB\n", &(p->total)) == EOF) goto read_error; */
/*     if(fscanf(proc_file,"Node %*d MemFree: %lu kB\n",  &(p->free) ) == EOF) goto read_error; */
/*     if(fscanf(proc_file,"Node %*d MemUsed: %lu kB\n", &(p->used) ) == EOF) goto read_error; */
/*   } else { */
/*     if(fscanf(proc_file,"MemTotal: %lu kB\n", &(p->total)) == EOF) goto read_error; */
/*     if(fscanf(proc_file,"MemFree: %lu kB\n",  &(p->free) ) == EOF) goto read_error; */
/*     p->used = p->total - p->free; */
/*   } */

/*   fclose(proc_file); */
/*   return 0; */
/* read_error: */
/*   fclose(proc_file); */
/*   {perror("fscanf"); return -1;} */
/*   return -1; */
/* } */

/* double proc_mem_load(struct proc_mem * p){ */
/*   return p->used == 0 ? 0 : 100*p->used / p->total; */
/* } */

/* double proc_mem_used(struct proc_mem * p){ */
/*   return p->used*1024; */
/* } */

/* /\***************************************************************************************************************\/ */
/* /\*                                           Looking at memory balance                                         *\/ */
/* /\***************************************************************************************************************\/ */
/* struct proc_numa{ */
/*   hwloc_obj_t location; */
/*   unsigned long local;  /\* Number of hit on local pages *\/ */
/*   unsigned long remote; /\* Number of miss on local pages *\/ */
/* }; */

/* struct proc_numa * new_proc_numa(hwloc_obj_t location){ */
/*   if(location->type != HWLOC_OBJ_NUMANODE){ */
/*     monitor_print_err("Location of object collecting mem info must be HWLOC_OBJ_NUMANODE (currently on %s)\n", */
/* 		      hwloc_type_name(location->type)); */
/*     return NULL; */
/*   } */
  
/*   struct proc_numa * p; */
/*   malloc_chk(p, sizeof(*p)); */
/*   p->location   = location;   */
/*   p->local = 0; */
/*   p->remote = 0; */
/*   return p; */
/* } */

/* void delete_proc_numa(struct proc_numa * p){free(p);} */

/* int proc_numa_read(struct proc_numa * p){ */

/*   char path[1024]; memset(path,0,sizeof(path)); */
/*   int nid = p->location->os_index; */
/*   snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/numastat", nid); */

/*   FILE * proc_file = proc_open_file(path); */
/*   if(proc_file == NULL){return -1;} */
  
/*   /\* Read new values *\/ */
/*   if(fscanf(proc_file,"%*s %lu\n", &(p->local)) == EOF) goto read_error; */
/*   if(fscanf(proc_file,"%*s %lu\n", &(p->remote)) == EOF) goto read_error; */
  
/*   fclose(proc_file); */
/*   return 0; */
/* read_error: */
/*   fclose(proc_file); */
/*   {perror("fscanf"); return -1;} */
/*   return -1; */
/* } */

/* double proc_numa_local(struct proc_numa * p){ */
/*   return p->local; */
/* } */

/* double proc_numa_remote(struct proc_numa * p){ */
/*   return p->remote; */
/* } */

/***************************************************************************************************************/
/*                                             Starting runnable                                               */
/***************************************************************************************************************/

extern hwloc_topology_t monitor_topology;

pid_t
start_executable(char * executable, char * exe_args[])
{
  printf("starting %s\n",executable);
  pid_t pid = fork();
  if(pid<0){
    perror("fork");
    exit(EXIT_FAILURE);
  }
  if(pid>0){
    return pid;
  }
  else if(pid==0){
    if(execvp(executable, exe_args)){
      monitor_print_err( "Failed to launch executable \"%s\"\n", executable);
      perror("execvp");
      exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
  }
  return pid;
}


