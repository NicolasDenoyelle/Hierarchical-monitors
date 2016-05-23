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
* its eventset ABI implementation, 
* its eventset, 
* the length of its eventsets history, 
* an eventset derivation function (instruction, cycles -> instructions/cycles)
* a function to compute the monitor value given the history of samples computed with derivation function.

This information is necessary to caracterize applications with respect to the underlying hardware.

### Plugins description:

The file `plugins/performance_utils.h` document the ABI to implement a library usable in a monitor.
Several implementations are already available:

* papi: use papi hardware events in monitors. This implementation assumes that you define a monitor for a leaf
  of the topology, or for the root of the topology. Defining a monitor with this plugin on another location can
  be misleading if you are not aware that it will record events with default papi options.
* maqao: use maqao hardware events in monitors. It also assumes that monitor will be located on a leaf
  or on the root of the topology, otherwise events are system wide events.
* hierarchical: accumulates eventsets of closest children monitors. Event names are defined as topology depths name.
* trace: read events from a monitor trace. Events are field index to read.

## Requirements:

* hwloc with defined values or above: 
  * `HWLOC_API_VERSION     0x00020000`
  * `HWLOC_COMPONENT_ABI   5`

* papi, or maqao(lprof) to build default plugins ABI implementation.
If you don't have one of them you will have to remove them from line `PERF_PLUGINS` in Makefile.

* monitors using papi and maqao are system wide.
You must ave permission to read performance counters system wide. 
It can be achieved by running the command: `echo "-1" > /proc/sys/kernel/perf_event_paranoid` as root.

## Set up:

Everything is set up in Makefile.

* `make` will compile the library, the plugins and the utility
* `make install` will do `make` and copy targets in (bin,lib,include...) directories located in `PREFIX` directory.

## Usage

### Defining monitors:

You have to describe the monitors into a single parsable file with this syntax:
```
#This monitor measure the total of instruction per cycle performed on each processing unit.
ins_cyc_pu{
	OBJ:=PU;                              #Default machine.
	PERF_LIB:=papi;                       #Compulsory.
	EVSET:=PAPI_TOT_INS, PAPI_TOT_CYC;    #Compulsory.
	EVSET_REDUCE:=$0/$1;                  #Default to sum.
#other optional fields
       N_SAMPLES:=4;	                     #Default to 32. (history of samples length)
       ACCUMULATE:=1;                        #Default to 0.  (accumulate if > 0)
       SILENT:=1;                            #Default to 0.  (print if == 0)
       MAX:=0;                               #Default to 0.  (Preset maximum value of sample)
       MIN:=0;                               #Default to 0.  (Preset minimum value of sample)
       SAMPLES_REDUCE:=MONITOR_SAMPLES_LAST; #Default to this value. (Computes a value based on the monitor values)
}

ins_cyc{
	PERF_LIB:=hierarchical;
	EVSET:=PU;                            #Use accumulation of eventsets of child monitors on PU.
	EVSET_REDUCE:=$0/$1;
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
    monitors_attach(getpid(), -1);
    monitors_import("my_monitors_path");
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

