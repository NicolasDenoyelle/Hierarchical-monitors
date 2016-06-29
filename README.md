# Hierarchical monitors

This utility help set up monitors to capture events and match them with the machine topology.

![](E5-2650.png?raw=true)

This output of a [previous implementation](https://github.com/NicolasDenoyelle/dynamic_lstopo) of the library with lstopo (hwloc) shows monitors on topology object with value and color.

## Description

A monitor is a counter, located somewhere on the machine topology, recording events, and aggregating 
events samples into a single insightful information.

The configuration of a monitor let you choose: 
* its depth location on topology, 
* its eventset ABI implementation (see performance plugins), 
* its eventset, 
* a window size of samples to store,
* a function to reduce eventset to a sample (instruction, cycles -> instructions/cycles) (see stat plugins)
* a function to analyze samples and compute the monitor value. (see stat plugins)

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
listed in environment: `export MONITOR_STAT_PLUGINS=<plugin1>:<plugin2>...` (stat_default is loaded by default),
containing functions with prototype: `double <name>(struct monitor *)`,
loaded at library initialization, 
and which function <name> might be loaded to perform the task of reducing monitor's events to a sample or samples window to a synthetic value.

## Requirements:

* hwloc with defined values or above: 
  * `HWLOC_API_VERSION     0x00020000`
  * `HWLOC_COMPONENT_ABI   5`

* papi, or maqao(lprof) to build default plugins ABI implementation.
If you don't have one of them you will have to remove them from line `PERF_PLUGINS` in Makefile.

* monitors using papi and maqao are system wide.
You must have permission to read performance counters system wide. 
It can be achieved by running the command: `echo "-1" > /proc/sys/kernel/perf_event_paranoid` as root.

* bison and lex for monitor importer.

## Set up:

Everything is set up in Makefile.

* `make` will compile the library, the plugins and the utility
* `make install` will do `make` and copy targets in (bin,lib,include...) directories located in `PREFIX` directory.

## Usage

### Defining monitors:

You have to describe the monitors into a single parsable file with the syntax below.
Those monitors aggregate LLCs miss balance, to check if pressure is balanced over last level caches. 

```

#This monitor measure the total of L3 miss performed on each processing unit.
L3_miss_pu{
	OBJ:=PU;                              #Default machine.
	PERF_LIB:=papi;                       #Compulsory.
	EVSET:=PAPI_L3_TCM;                   #Compulsory.

#other optional fields
       #SAMPLE_REDUCE:=$0/$1;                 #No event reduction for this monitor.	
       #WINDOW:=4;	                      #Default to 1. (history of samples length)
       #ACCUMULATE:=0;                        #Default to 0.  (accumulate if > 0)
       SILENT:=1;                             #Default to 0.  (print if == 0)
       #MAX:=0;                               #Default to DBL_MIN.  (Preset maximum value of sample)
       #MIN:=0;                               #Default to DBL_MAX.  (Preset minimum value of sample)
       #WINDOW_REDUCE:=monitor_samples_last;  #No sample reduction.
}

#This monitor accumulate L3 miss on each L3
L3_miss{
	OBJ:=L3;
	PERF_LIB:=accumulate;
	EVSET:=L3_miss_PU;                     #Use accumulation of eventsets of child monitors on PU.
	SILENT:=1;
}

#This monitor print a single L3 balance value 
L3_balance{
	OBJ:=Machine;
	PERF_LIB:=hierarchical;
	EVSET:=L3_miss;                       #Read each L3 cache miss value.
	WINDOW_REDUCE:=gsl_stat_var;          #Not yet implemented in any plugin but would compute variance of L3 cache miss to check cache pressure balance
}

```

* display help: `hmon --help`
* `hmon --input monitor_configuration_file`


### Embeding the library:
* Include `"hmon.h"` into the files calling the monitor library.
* Compile your code with -lhmon ldflag.
* Code sample:

```
#include <hmon.h>

int main(){
    monitor_lib_init(NULL, NULL, "output_test");
    monitors_import("../example_monitor");
    monitors_start();
    monitors_update();
    sleep(1);
    monitors_update();
    monitors_output(monitor_output, 1);
    monitor_lib_finalize();
    return 0;
}
```
It is really easy to access information into monitors.
The file `"monitor.h"` let you know about the monitor structure, and gives you access to the machine topology.
Their are to way to find monitors: 
* by walking the topology and dereferencing topology node's with non NULL userdata, 
* or by iterating over the `monitors` array using the array API in `"utils.h"`.

## Coding the library

![](software_view.png?raw=true)

