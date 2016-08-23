#include "hmon_utils.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

static DIR * proc_open_dir(char * path){
    DIR * dir = NULL;
    dir = opendir(path);
  if(dir==NULL)
      perror("attach opendir:");
 return dir;
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
    FILE * proc_file = fopen(proc_path, "r");
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
    status_file = fopen(path,"r");
    if(status_file == NULL){
	perror("fopen");
	return;
    }
    
    line = malloc(8);
    do{
	free(line); line = NULL;
	err = getline(&line, &read, status_file);
    } while(err!=-1 && strncmp(line, token, token_length));
    
    fclose(status_file);
    if(err == -1){
	free(line);
	return;
    }
    
    line[strlen(line)-1] = '\0';
    hwloc_bitmap_sscanf(cpuset, line+token_length+1);
    free(line);
}

/***************************************************************************************************************/
/*                                             Setting tasks placement                                         */
/***************************************************************************************************************/

extern hwloc_topology_t monitor_topology;


/**
 * Check whether linux cpuset filesystem can be used.
 * If so, create a hierarchy filesystem of topology cpusets.
 **/
/* static const char * proc_cpuset_root = "/dev/cpuset/monitor"; */

/* static int proc_cpuset_init(){ */
/*     char        cpuset_path[2048]; */
/*     char        cpuset_str[64]; */
/*     hwloc_obj_t cpuset_obj = hwloc_get_root_obj(monitor_topology); */
    
/*     memset(cpuset_path, 0, sizeof(cpuset_path)); */
/*     snprintf(cpuset_path, sizeof(cpuset_path), "%s", proc_cpuset_root);  */
/*     if(mkdir(proc_cpuset_root, S_IRUSR|S_IWUSR) == -1){ */
/* 	perror("mkdir in /dev/cpuset/monitor"); */
/* 	return -1; */
/*     } */
/*     /\* Walk the topology and create a cpuset hierarchy of the topology restricted to monitors *\/ */
/*  proc_cpuset_create: */
/*     memset(cpuset_str,0,sizeof(cpuset_str)); */
/*     hwloc_bitmap_snprintf(cpuset_str, sizeof(cpuset_str), cpuset_obj->cpuset); */
/*     strcat(cpuset_path,"/"); strcat(cpuset_path,cpuset_str); */
/*     if(mkdir(proc_cpuset_root, S_IRUSR|S_IWUSR) == -1){ */
/* 	perror("mkdir in /dev/cpuset/monitor"); */
/* 	return -1; */
/*     } */
/*  proc_topo_walk: */
/*     if(cpuset_obj->first_child != NULL) */
/* 	cpuset_obj = cpuset_obj->first_child; */
/*     else if(cpuset_obj->next_sibling != NULL){ */
/* 	cpuset_obj =  cpuset_obj->next_sibling; */
/* 	if(cpuset_obj->userdata!=NULL) */
/* 	    memset(cpuset_path+strlen(cpuset_path)-strlen(cpuset_str)-1, 0, strlen(cpuset_str)); */
/*     } */
/*     else{ */
/* 	size_t len = strlen(cpuset_path); */
/* 	char * c = &(cpuset_path[len]); */
/* 	while(cpuset_obj != NULL && cpuset_obj->next_sibling == NULL){ */
/* 	    cpuset_obj = cpuset_obj->parent; */
/* 	    if(cpuset_obj->userdata==NULL) */
/* 		continue; */
/* 	    do{*c = *c-1;} while(*c!='/' || c!=cpuset_path); */
/* 	} */
/* 	memset(c , 0, len - (c - cpuset_path)); */
/*     } */
/*     if(cpuset_obj == NULL) */
/* 	return 0; */
/*     if(cpuset_obj->userdata == NULL) */
/* 	goto proc_topo_walk; */
    
/*     cpuset_obj =  cpuset_obj->next_sibling; */
/*     goto proc_cpuset_create; */
/* } */


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


/* pid_t  */
/* start_executable(char * executable, char * exe_args[]) */
/* { */
/*   printf("starting %s\n",executable); */
/*   pid_t ret=0; */
/*   pid_t *child = mmap(NULL, sizeof *child, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); */
/*   *child=0; */
/*   pid_t pid2, pid1 = fork(); */
/*   if(pid1<0){ */
/*     perror("fork"); */
/*     exit(EXIT_FAILURE); */
/*   } */
/*   if(pid1>0){ */
/*     wait(NULL); */
/*   } */
/*   else if(pid1==0){ */
/*     pid2=fork(); */
/*     if(pid2){ */
/*       *child = pid2; */
/*       msync(child, sizeof(*child), MS_SYNC); */
/*       exit(0); */
/*     } */
/*     else if(!pid2){ */
/*       printf("starting %s\n",executable); */
/*       ret = execvp(executable, exe_args); */
/*       if (ret) { */
/* 	monitor_print_err( "Failed to launch executable \"%s\"\n", */
/* 		executable); */
/* 	perror("execvp"); */
/* 	exit(EXIT_FAILURE); */
/*       } */
/*     } */
/*   } */
/*   msync(child, sizeof(*child), MS_SYNC); */
/*   ret = *child; */
/*   munmap(child, sizeof *child); */
/*   return ret;  */
/* } */


