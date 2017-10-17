#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include "proc.h"

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

static FILE * proc_open_file(const char * path){
  FILE * proc_file = fopen(path, "r");
  if(proc_file == NULL){
    fprintf(stderr, "Can't open %s for reading", path);
    return NULL;
  }
  return proc_file;
}

/****************************************************************************************************************/
/*                                          Looking at /proc/<pid>/stat                                         */
/****************************************************************************************************************/

struct proc_stat* new_proc_stat(const pid_t pid){
  unsigned i;
  struct proc_stat * p = malloc(sizeof *p);
  p->pid         = pid;
  p->state       = 'R';  
  p->min_flt     = 0;
  p->cmin_flt    = 0;
  p->maj_flt     = 0;
  p->cmaj_flt    = 0;
  p->utime       = 0;
  p->stime       = 0;  
  p->cutime      = 0;
  p->cstime      = 0;  
  p->num_threads = 0;
  p->start_time  = 0;
  p->vsize       = 0;
  p->rss         = 0;
  p->rsslim      = 0;
  p->task_cpu    = 0;
  p->blkio_ticks = 0;
  p->timestamp   = 0;
  for(i=0; i<sizeof(p->values)/sizeof(*p->values); i++){p->values[0] = 0;}
  char path[256]; memset(path, 0, sizeof(path));
  snprintf(path, sizeof(path), "/proc/%u/stat", pid);
  p->filename = strdup(path);
  return p;    
}

void delete_proc_stat(struct proc_stat * p){
  free(p->filename);
  free(p);
}

int proc_stat_update(struct proc_stat * p){
  FILE * f = proc_open_file(p->filename); if(f == NULL) return -1;
  int err = fscanf(f,"%d %*s %c %*d %*d %*d %*d %*d %*u %lu %lu %lu %lu %lu %lu %ld %ld %*ld %*ld %ld %*ld %llu %lu %ld %lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*lu %*d %d %*u %*u %llu",
		   &(p->pid),
		   &(p->state),
		   &(p->min_flt),
		   &(p->cmin_flt),		     
		   &(p->maj_flt),
		   &(p->cmaj_flt),
		   &(p->utime),
		   &(p->stime),
		   &(p->utime),
		   &(p->stime),
		   &(p->num_threads),		     
		   &(p->start_time),
		   &(p->vsize),
		   &(p->rss),		     
		   &(p->rsslim),
		   &(p->task_cpu),
		   &(p->blkio_ticks));
  p->timestamp  = clock();
  fclose(f);
  if(err==EOF){perror("fscanf"); return -1;}
  return 0;
}

void proc_stat_cast_double(struct proc_stat* p){
  p->values[0]  = (double)(p->min_flt);
  p->values[1]  = (double)(p->cmin_flt);
  p->values[2]  = (double)(p->maj_flt);
  p->values[3]  = (double)(p->cmaj_flt);
  p->values[4]  = (double)(p->utime);
  p->values[5]  = (double)(p->stime);
  p->values[6]  = (double)(p->cutime);
  p->values[7]  = (double)(p->cstime);    
  p->values[8]  = (double)(p->num_threads);
  p->values[9]  = (double)(p->start_time);
  p->values[10] = (double)(p->vsize);
  p->values[11] = (double)(p->rss);
  p->values[12] = (double)(p->rsslim);
  p->values[13] = (double)(p->task_cpu);
  p->values[14] = (double)(p->blkio_ticks);
  p->values[15] = (double)(p->timestamp);
}

double proc_stat_cpuload(const struct proc_stat*old, const struct proc_stat*new){
  unsigned long usage = (new->utime+new->stime+new->utime+new->stime) - (old->utime+old->stime+old->utime+old->stime);
  return usage == 0 ? usage : 100 * usage / (new->timestamp-old->timestamp);
}

void proc_stat_diff(struct proc_stat* res, const struct proc_stat* lhs, const struct proc_stat* rhs){
  res->min_flt     = lhs->min_flt     - rhs->min_flt;
  res->cmin_flt    = lhs->cmin_flt    - rhs->cmin_flt;
  res->maj_flt     = lhs->maj_flt     - rhs->maj_flt;
  res->cmaj_flt    = lhs->cmaj_flt    - rhs->cmaj_flt;
  res->utime       = lhs->utime       - rhs->utime;
  res->stime       = lhs->stime       - rhs->stime;
  res->cutime      = lhs->cutime      - rhs->cutime;
  res->cstime      = lhs->cstime      - rhs->cstime;
  res->num_threads = MAX(lhs->num_threads,rhs->num_threads);
  res->start_time  = MIN(lhs->start_time,rhs->start_time);
  res->vsize       = MAX(lhs->vsize,rhs->vsize);
  res->rss         = MAX(lhs->rss,rhs->rss);
  res->rsslim      = MAX(lhs->rsslim,rhs->rsslim);
  res->task_cpu    = lhs->task_cpu;
  res->blkio_ticks = lhs->blkio_ticks - rhs->blkio_ticks;
  res->timestamp   = lhs->timestamp   - rhs->timestamp;
}

/***************************************************************************************************************/
/*                                             Looking at /proc/stat                                           */
/***************************************************************************************************************/

int proc_cpu_update(struct proc_cpu * p){
  FILE* f = proc_open_file("/proc/stat"); if(f==NULL){return -1;}
  int cpu_num = -1;
  int ret = 0;
  while(cpu_num != p->cpu && ret != EOF){
    ssize_t n = 0; char * line = NULL; getline(&line, &n, f);
    ret = sscanf(line,"cpu%d %lu %lu %lu %lu %lu %lu %lu",
		 &cpu_num,
		 &(p->user),
		 &(p->nice),
		 &(p->system),
		 &(p->idle),
		 &(p->iowait),
		 &(p->irq),
		 &(p->softirq));
    free(line);
  }
  fclose(f);
  return cpu_num;
}

struct proc_cpu * new_proc_cpu(const int cpu){
  unsigned i;  
  int cpu_num;
  struct proc_cpu * p = malloc(sizeof *p);
  p->cpu     = cpu;  
  p->user    = 0;
  p->nice    = 0;
  p->system  = 0;
  p->idle    = 0;
  p->iowait  = 0;
  p->irq     = 0;
  p->softirq = 0;
  for(i=0; i<sizeof(p->values)/sizeof(*p->values); i++){p->values[0] = 0;}
  return p;
}

void delete_proc_cpu(struct proc_cpu * p){
  free(p);
}

double proc_cpu_load(const struct proc_cpu * old, const struct proc_cpu * new){  
  double usage = (new->user+new->nice+new->system) - (old->user+old->nice+old->system);
  return usage == 0 ? 0 : 100*usage / (usage + (new->idle-old->idle));
}

void proc_cpu_cast_double(struct proc_cpu* p){
    p->values[0]  = (double)(p->cpu);
    p->values[1]  = (double)(p->user);
    p->values[2]  = (double)(p->nice);
    p->values[3]  = (double)(p->system);
    p->values[4]  = (double)(p->idle);
    p->values[5]  = (double)(p->iowait);
    p->values[6]  = (double)(p->irq);
    p->values[7]  = (double)(p->softirq);
}

void proc_cpu_diff(struct proc_cpu* res, const struct proc_cpu* lhs, const struct proc_cpu* rhs){
  res->cpu         = lhs->cpu==rhs->cpu ? lhs->cpu : -1;
  res->user        = lhs->user     - rhs->user;
  res->nice        = lhs->nice     - rhs->nice;
  res->system      = lhs->system   - rhs->system;
  res->idle        = lhs->idle     - rhs->idle;
  res->iowait      = lhs->iowait   - rhs->iowait;
  res->irq         = lhs->irq      - rhs->irq;
  res->softirq     = lhs->softirq  - rhs->softirq;  
}

/***************************************************************************************************************/
/*                       Looking at /proc/devices/system/node/node<i>/meminfo or /proc/meminfo                 */
/***************************************************************************************************************/

struct proc_mem * new_proc_mem(const int nodeID){
  unsigned i;  
  struct proc_mem * p = malloc(sizeof *p);
  p->node = nodeID;
  p->free = 0;
  p->used = 0;
  for(i=0; i<sizeof(p->values)/sizeof(*p->values); i++){p->values[0] = 0;}  
  char path[1024]; memset(path,0,sizeof(path));  
  if(nodeID >= 0){ snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/meminfo", nodeID); }
  else{ snprintf(path, sizeof(path), "/proc/meminfo"); }  
  p->filename = strdup(path);    
  return p;
}

void delete_proc_mem(struct proc_mem * p){
  free(p->filename);
  free(p);
}

int proc_mem_update(struct proc_mem * p){
  FILE* f = proc_open_file(p->filename); if(f==NULL){return -1;}

  if(p->node >= 0){
    /* Read new values */
    if(fscanf(f,"Node %*d MemTotal: %lu kB\n", &(p->total)) == EOF) goto read_error;
    if(fscanf(f,"Node %*d MemFree: %lu kB\n",  &(p->free) ) == EOF) goto read_error;
    if(fscanf(f,"Node %*d MemUsed: %lu kB\n", &(p->used) ) == EOF) goto read_error;
  } else {
    if(fscanf(f,"MemTotal: %lu kB\n", &(p->total)) == EOF) goto read_error;
    if(fscanf(f,"MemFree: %lu kB\n",  &(p->free) ) == EOF) goto read_error;
    p->used = p->total - p->free;
  }

  fclose(f);
  return 0;
read_error:
  fclose(f);  
  {perror("fscanf"); return -1;}
  return -1;
}

void proc_mem_cast_double(struct proc_mem* p){
  p->values[0] = (double)(p->node);
  p->values[1] = (double)(p->total);
  p->values[2] = (double)(p->free);
  p->values[3] = (double)(p->used);
}

double proc_mem_load(struct proc_mem * p){
  return p->used == 0 ? 0 : 100*p->used / p->total;
}

void proc_mem_diff(struct proc_mem* res, const struct proc_mem* lhs, const struct proc_mem* rhs){
  res->node        = lhs->node==rhs->node ? lhs->node : -1;
  res->total       = lhs->total - rhs->total;
  res->used        = lhs->used - rhs->used;
  res->free        = lhs->free - rhs->free;  
}

/***************************************************************************************************************/
/*                            Looking at /proc/devices/system/node/node<i>/numastat                            */
/***************************************************************************************************************/

struct proc_numa * new_proc_numa(const int nodeID){
  unsigned i;  
  struct proc_numa * p = malloc(sizeof *p);
  p->node   = nodeID;  
  p->local  = 0;
  p->remote = 0;
  for(i=0; i<sizeof(p->values)/sizeof(*p->values); i++){p->values[0] = 0;}
  char path[1024]; memset(path,0,sizeof(path));    
  snprintf(path, sizeof(path), "/sys/devices/system/node/node%d/numastat", nodeID);
  p->filename = strdup(path);
  return p;
}

void delete_proc_numa(struct proc_numa * p){
  free(p->filename);
  free(p);
}

int proc_numa_update(struct proc_numa * p){
  FILE* f = proc_open_file(p->filename); if(f==NULL){return -1;}
  /* Read new values */
  if(fscanf(f,"%*s %lu\n", &(p->local)) == EOF) goto read_error;
  if(fscanf(f,"%*s %lu\n", &(p->remote)) == EOF) goto read_error;
  fclose(f);
  return 0;
read_error:
  fclose(f);  
  perror("fscanf");
  return -1;
}

void proc_numa_cast_double(struct proc_numa* p){
  p->values[0] = (double)(p->node);
  p->values[1] = (double)(p->local);
  p->values[2] = (double)(p->remote);
}

void proc_numa_diff(struct proc_numa* res, const struct proc_numa* lhs, const struct proc_numa* rhs){
  res->node        = lhs->node==rhs->node ? lhs->node : -1;
  res->local       = lhs->local - rhs->local;
  res->remote        = lhs->remote - rhs->remote;
}

#ifdef TEST
#include <unistd.h>

int main(){
  struct proc_stat* p = new_proc_stat(getpid());
  proc_stat_update(p);
  printf("pid: %d\n", p->pid);
  printf("state: %c\n", p->state);
  printf("min_flt: %u\n", p->min_flt);
  printf("cmin_flt: %u\n", p->cmin_flt);    
  printf("maj_flt: %u\n", p->maj_flt);
  printf("cmaj_flt: %u\n", p->cmaj_flt);
  printf("num_threads: %u\n", p->num_threads);
  printf("start_time: %u\n", p->start_time);
  printf("vsize: %u\n", p->vsize);
  printf("rss: %u\n", p->rss);
  printf("rsslim: %u\n", p->rsslim);
  printf("cpu: %u\n", p->task_cpu);
  printf("blkio_ticks: %u\n", p->blkio_ticks);
  printf("\n");
  delete_proc_stat(p);
  
  struct proc_cpu* p0 = new_proc_cpu(-1);
  printf("cpu: %d\n", p0->cpu);
  printf("user: %u\n", p0->user);
  printf("nice: %u\n", p0->nice);
  printf("system: %u\n", p0->system);
  printf("idle: %u\n", p0->idle);
  printf("iowait: %u\n", p0->iowait);
  printf("irq: %u\n", p0->irq);
  printf("softirq: %u\n", p0->softirq);
  printf("\n");  
  delete_proc_cpu(p0);

  struct proc_mem * q = new_proc_mem(-1);
  proc_mem_update(q);
  printf("used=%u, free=%u, total=%u\n", q->total, q->free, q->used);
  printf("\n");  
  delete_proc_mem(q);

  struct proc_numa * r = new_proc_numa(0);
  proc_numa_update(r);
  printf("numa%d: local=%u, remote=%u\n", r->node, r->local, r->remote);
  delete_proc_numa(r);	 
  
  return 0;
}
#endif /* TEST */

