# Hierarchical Monitors

## Description

A monitor is a counter, located somewhere on the machine topology, recording events, and eventually aggregating 
events into a single insightful information: `events -> monitor -> output.`
Monitors can be chained to perform several types of reduction : `events -> monitor -> output -> monitor -> output ...`

This project includes a library and a utility to set up and run monitors, as well as a script to plot monitors.

![](E5-2650.png?raw=true)

This output of a [previous implementation](https://github.com/NicolasDenoyelle/dynamic_lstopo) of the library with lstopo (hwloc) shows monitors on topology object with value and color. (It is still possible to get this output by using our [hwloc fork](https://github.com/NicolasDenoyelle/liblstopo) with --enable-liblstopo at configure)

## Requirements:

* libtool, automake for compilation chain

* hwloc with defined values or above: 
  * `HWLOC_API_VERSION     0x00020000`
  * `HWLOC_COMPONENT_ABI   5`
* papi, or maqao(lprof) to build hardware performance counters plugin (Checked by configure).

* monitors using papi and maqao are system wide.
You must have permission to read performance counters system wide. 
It can be achieved by running the command: `echo "-1" > /proc/sys/kernel/perf_event_paranoid` as root.

* bison and lex for monitor importer.

* R and Rscript for plot script.

## Set up:

Use classic autotool chain:

* Use autogen.sh to generate configure.

* Use configure to generate makefile.

* Use make, make install to build and install.

* For instance use this chain: `./autogen.sh && ./configure --prefix=/usr/local && make -j && make install`

## Configuring monitors:

When seting a new monitor you have to fill a small number of fields in a file containting all of your monitors description.
This file has a simple but strict syntax.

Several examples and help on how to build this file can be found [here](./example/example_monitor).


The configuration of a monitor let you choose: 
* (Compulsory) Its depth location on topology: PU,CORE, L1d, L1i, L2, ...
  as long as it is parsable by the function [hwloc_type_sscanf_as_depth](https://github.com/open-mpi/hwloc/blob/master/hwloc/traversal.c#L320) from hwloc library.

* (Compulsory) The library which the events come from. (See [Choosing Events Source](#choosing-events-source)).

* (Compulsory) Its events.

* (Optional) A reduction function to apply on events. (See [Reducing Events](#reducing-events)).

* (Optional) The length of the history of events.

* (Optional) A boolean to tell if the monitor should be printed to output trace.

Here is an example of a monitor which output instruction per cycle of each processing unit using papi library:

```
INS_per_CYC{
	OBJ:=PU;
	PERF_LIB:=papi;	
	EVSET:=PAPI_TOT_CYC, PAPI_TOT_INS;
	REDUCTION:=$1/$0;
}
```
## Usage

### Utility
The utility let you monitor the machine or a part of a machine restricted to a program execution domain, along time.

`hmon -h` will output utility help.

### Library
The header file `hmon.h` stands as the library documentation.

A code snippet is given [here](example/test.c).

* Include `"hmon.h"` into the files calling the monitor library.

* Compile your code with -lhmon ldflag.

### Plot Script

The plot script is not installed and is located [here](utils/hmon_plot.R).

Use the command `Rscript hmon_plot.R -h` to output usage help of the script.

Below is an example of plot output, on an application producing L2 miss phases.
L2 misses accumulation was recorded along time, and clustered in the plot script.

![](./utils/L2_miss.out.png?raw=true)

Below is another example of plot output, on an HPCCG mini app's L1 cache misses and a linear model fit of those using other events from monitor.

![](./utils/hpccg_200_200_200-0.png?raw=true)

## Choosing Events Source:

Several performance plugins (fake, accumulate, hierarchical, papi, maqao) are implemented as base for `PERF_LIB` field in monitor
definition.

Here is a brief description of each:

* fake: output events named `FAKE_MONITOR` with a constant value of 1.

* maqao, papi: output hardware events available on the underlying architecture. Those assume that you define a monitor for a leaf
  of the topology, or for the root of the topology. Defining a monitor with this plugin on another location can
  be misleading if you are not aware that it will record events with default papi options.

* accumulate: take children monitors as events, and accumulate their eventset in its own eventset.

* hierarchical: take children monitors as events, and join their eventset as its own eventset.

One can also implement its own performance plugin with this instructions:

A performance plugin is a file with pattern name: `<name>_hmonitor_plugin.so` loadable with dlopen.
The plugin must implement the [performance plugin interface](./src/plugins/performance_interface.h).

## Reducing Events
Events reduction can be done by defining arithmetic expressions of output or using a statistic plugins.

* Arithmetic expressions must follow the syntax `$j op $k op ...`

Where `$j` is the j_st element of events, and `$k` is the k_st element of events.
`$j` and `$k` may also be integer or flaoting point values.

`REDUCTION` field can be a list of arrithmetic expressions: `$0+$1, $2*$3;`

* Reduction plugin must follow the syntax `n_output#function`
Where `n_output` is the output array size writtable by the function, and `function` is a function name loadable in a reduction plugin. Here is a list of default available reduction functions:

  * hmonitor_evset_var: output the variance of the events.
  * hmonitor_events_var: output the variance of stored events for each event type.
  * hmonitor_events_mean: output the mean of stored events for each event type.
  * hmonitor_events_sum: output the sum of stored events for each event type.
  * hmonitor_events_min: output the min of stored events for each event type.
  * hmonitor_events_max: output the max of stored events for each event type.

Reduction plugins compiled with the library are automatically loaded.

Custom reduction plugins must have the name pattern: `<name>_hmonitor_plugin.so`,
must be referenced in environment variable as `export HMON_STAT_PLUGINS=<plugin1>:<plugin2>...` ,
and loadable with dlopen.

Function defined and loadable in the plugin must have the following protoype:

`void reduction_function(hmon m)`

