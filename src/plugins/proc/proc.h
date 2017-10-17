#ifndef PROC_H
#define PROC_H

/****************************************************************************************************************/
/*                                          Looking at /proc/<pid>/stat                                         */
/****************************************************************************************************************/

struct proc_stat{
  unsigned           pid;
  char               state;        /*R is running, S is sleeping, D is sleeping in an uninterruptible wait, Z is zombie, T is traced or stopped*/
  unsigned long      min_flt;      /*number of minor faults*/
  unsigned long      cmin_flt;     /*number of minor faults with child's*/
  unsigned long      maj_flt;      /*number of major faults*/
  unsigned long      cmaj_flt;     /*number of major faults with child's*/
  unsigned long      utime;        /*Process time (in clock ticks) in user space */
  unsigned long      stime;        /*Process time (in clock ticks) in kernel space */
  long               cutime;       /*Process time (in clock ticks) spent waiting for children in user space */
  long               cstime;       /*Process time (in clock ticks) spent waiting for children in kernel space */    
  unsigned long      num_threads;  /*number of threads*/
  unsigned long long start_time;   /*time the process started after system boot*/
  unsigned long      vsize;        /*virtual memory size*/
  unsigned           rss;          /*resident set memory size*/
  unsigned           rsslim;       /*current limit in bytes on the rss*/
  unsigned           task_cpu;     /*which CPU the task is scheduled on*/
  unsigned long long blkio_ticks;  /*time spent waiting for block IO*/
  unsigned long      timestamp;    /*update time*/
  char*              filename;     /*File where to reads infos*/  
  double             values[16];   /* All numeric values (from min_flt to timestamp) casted to double */
};

struct proc_stat* new_proc_stat(const pid_t pid);
void              delete_proc_stat(struct proc_stat * p);
int               proc_stat_update(struct proc_stat * p);
void              proc_stat_cast_double(struct proc_stat* p); /* Stores fields in values field as double */
void              proc_stat_diff(struct proc_stat* result, const struct proc_stat* lhs, const struct proc_stat* rhs);
double            proc_stat_cpuload(const struct proc_stat*old, const struct proc_stat*new);
/***************************************************************************************************************/
/*                                             Looking at /proc/stat                                           */
/***************************************************************************************************************/

struct proc_cpu{
  int           cpu;       /* the cpu os_index to look at or -1 for whole system */
  unsigned long user;      /* normal processes executing in user mode */
  unsigned long nice;      /* niced processes executing in user mode */
  unsigned long system;    /* processes executing in kernel mode */
  unsigned long idle;      /* twiddling thumbs */
  unsigned long iowait;    /* waiting for I/O to complete */
  unsigned long irq;       /* servicing interrupts */
  unsigned long softirq;   /* servicing softirqs */
  double        values[8]; /* All numeric values (from cpu to softirq) casted to double */  
};

struct proc_cpu * new_proc_cpu   (const int cpuID);
void              delete_proc_cpu(struct proc_cpu * p);
int               proc_cpu_update(struct proc_cpu * p);
void              proc_cpu_cast_double(struct proc_cpu* p); /* Stores fields in values field as double */
double            proc_cpu_load(const struct proc_cpu * old, const struct proc_cpu * new); /* Percentage of cputime spent */
void              proc_cpu_diff(struct proc_cpu* res, const struct proc_cpu* lhs, const struct proc_cpu* rhs);

/***************************************************************************************************************/
/*                       Looking at /proc/devices/system/node/node<i>/meminfo or /proc/meminfo                 */
/***************************************************************************************************************/

struct proc_mem{
  int           node;     /* The numa node logical index or -1 for whole system */
  unsigned long total;    /* Total memory */
  unsigned long free  ;   /* Free memory */
  unsigned long used;     /* Used memory */
  char*         filename; /* The file where to gather infos */
  double        values[4];/* All numeric values (from node to used) casted to double */  
};

struct proc_mem * new_proc_mem   (const int nodeID);
void              delete_proc_mem(struct proc_mem * p);
int               proc_mem_update(struct proc_mem * p);
void              proc_mem_cast_double(struct proc_mem* p); /* Stores fields in values field as double */
double            proc_mem_load  (struct proc_mem * p); /* %percentage of used memory */
void              proc_mem_diff(struct proc_mem* res, const struct proc_mem* lhs, const struct proc_mem* rhs);

/***************************************************************************************************************/
/*                            Looking at /proc/devices/system/node/node<i>/numastat                            */
/***************************************************************************************************************/

struct proc_numa{
  int           node;     /* NUMA node index */
  unsigned long local;    /* Number of hit on local pages */
  unsigned long remote;   /* Number of miss on local pages */
  char*         filename; /* The file to read to gather infos */
  double        values[4];/* All numeric values (from node to remote) casted to double */    
};

struct proc_numa * new_proc_numa   (const int nodeID);
void               delete_proc_numa(struct proc_numa * p);
int                proc_numa_update(struct proc_numa * p);
void               proc_numa_cast_double(struct proc_numa* p); /* Stores fields in values field as double */
void               proc_numa_diff(struct proc_numa* res, const struct proc_numa* lhs, const struct proc_numa* rhs);

#endif /* PROC_H */
