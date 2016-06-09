# Hierarchical monitors

This utility help set up monitors to capture events and match them with the machine topology.

## Description
Let a SMP system with 16 LLC caches and 8 DDR memory.
Let a memory bound application to optimize.
If you want to balance memory and cache usage, you will look at LLC miss and LLC access events.
You might want to assert memory usage balance first because waiting for data in cache is faster than waiting for data in memory.
Consequently, you will first balance memory accesses accross the system, then cache accesses accross each memory.
And you will also require to map access informations with memory inclusiveness to achieve good tasks placement.

Hierarchical monitors gives you all this information at once.
As an illustration example, the figure above shows a [previous implementation](https://github.com/NicolasDenoyelle/dynamic_lstopo) using hwloc graphical output. 

![](E5-2650.png?raw=true)

A monitor is a counter, located somewhere on the machine topology, recording events, and aggregating 
events samples into a single insightful information.

The configuration of a monitor let you choose: 
* its depth location on topology, 
* its eventset ABI implementation (see performance plugins), 
* its eventset, 
* the length of its eventsets history, 
* an eventset to sample reduction function (instruction, cycles -> instructions/cycles) (see stat plugins)
* a samples to value analysis function to compute the monitor value given the history of samples computed with derivation function. (see stat plugins)

This information is necessary to caracterize applications with respect to the underlying hardware.

### Plugins description:
A plugin is a file with pattern name: `<name>.monitor_plugin.so` loadable with dlopen.
There are two kind of plugins: 

* Performance plugins, 
Several implementations are already available:
  * papi: use papi hardware events in monitors. This implementation assumes that you define a monitor for a leaf
  of the topology, or for the root of the topology. Defining a monitor with this plugin on another location can
  be misleading if you are not aware that it will record events with default papi options.
  * maqao: use maqao hardware events in monitors. It also assumes that monitor will be located on a leaf
  or on the root of the topology, otherwise events are system wide events.
  * accumulate: accumulates eventsets of closest children monitors. Event names are defined as topology depths name.
  * hierarchical: read closest children monitors values to fill eventset.
  * trace: read events from a monitor trace. Events are field index to read.
`plugins/performance_plugin.h` document the ABI to implement a plugin usable in a monitor. 

* Statistic plugins,
listed in environment: `export MONITOR_STAT_PLUGINS=<plugin1>,<plugin2>...` (stat_default is loaded by default)",
containing functions with prototype: `double <name>(struct monitor *)`,
loaded at library initialization, 
and which function <name> might be loaded to perform the task of reducing monitor's events to a sample or samples history to a synthetic value into.

## Requirements:

* hwloc with defined values or above: 
  * `HWLOC_API_VERSION     0x00020000`
  * `HWLOC_COMPONENT_ABI   5`

* papi, or maqao(lprof) to build default plugins ABI implementation.
If you don't have one of them you will have to remove them from line `PERF_PLUGINS` in Makefile.

* monitors using papi and maqao are system wide.
You must ave permission to read performance counters system wide. 
It can be achieved by running the command: `echo "-1" > /proc/sys/kernel/perf_event_paranoid` as root.

* bison and lex for monitor importer.

## Set up:

Everything is set up in Makefile.

* `make` will compile the library, the plugins and the utility
* `make install` will do `make` and copy targets in (bin,lib,include...) directories located in `PREFIX` directory.

## Usage

### Defining monitors:

You have to describe the monitors into a single parsable file with this syntax:
```
#This monitor measure the total of L3 miss performed on each processing unit.
L3_miss_pu{
	OBJ:=PU;                              #Default machine.
	PERF_LIB:=papi;                       #Compulsory.
	EVSET:=PAPI_L3_TCM;                   #Compulsory.

#other optional fields
       #EVSET_REDUCE:=$0/$1;                  #No event reduction for this monitor.	
       #N_SAMPLES:=4;	                      #Default to 32. (history of samples length)
       #ACCUMULATE:=0;                        #Default to 0.  (accumulate if > 0)
       SILENT:=1;                             #Default to 0.  (print if == 0)
       #MAX:=0;                               #Default to DBL_MIN.  (Preset maximum value of sample)
       #MIN:=0;                               #Default to DBL_MAX.  (Preset minimum value of sample)
       #SAMPLES_REDUCE:=monitor_samples_last; #No sample reduction.
}

L3_miss{
	PERF_LIB:=accumulate;
	EVSET:=PU;                            #Use accumulation of eventsets of child monitors on PU.
}

L3_balance{
	PERF_LIB:=hierarchical;
	EVSET:=L3;                            #Read each L3 cache miss value.
	SAMPLES_REDUCE:=gsl_stat_var;         #Not yet implemented in any plugin but would compute variance of L3 cache miss to check cache pressure balance
}

```

### Embeding the library:
* Include `"monitor.h"` into the files calling the monitor library.
* Compile your code with -lmonitor ldflag.
* Code sample:

```
#include "monitor.h"
#include <sys/types.h>
#include <unistd.h>

int main(){
    monitor_lib_init(NULL, "output_file");
    monitors_import("my_monitors_path");
    monitors_attach(getpid(), -1);
    monitors_start();
    monitors_update(-1);
    /* Code here */
    monitors_update(-1);
    monitor_lib_finalize();
    return 0;
}
```
It is really easy to access information into monitors.
The file `"monitor.h"` let you know about the monitor structure, and gives you access to the machine topology.
Their are to way to find monitors: 
* by walking the topology and dereferencing topology node's with non NULL userdata, 
* or by iterating over the `monitors` array using the array API in `"utils.h"`.

### Using the monitor utility:
* display help: `monitor --help`
* `monitor --input monitor_configuration_file`

## Coding the library

![](software_view.png?raw=true)

