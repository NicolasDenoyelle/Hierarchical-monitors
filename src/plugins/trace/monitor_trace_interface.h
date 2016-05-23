#include <hwloc.h>
#include <stdio.h>

/**
 * This performance_interfaceandle homogeneous trace reading. Implementing it allows upper monitor layer to read the trace
 * and store results into a monitor.
 * It assumes trace to contain sets of values entries.
 * It assumes each entry of the trace to contain a timestamp, a location on machine, a set of performance values.
 **/

struct monitor_trace_entry;
void monitor_trace_seek_begin(FILE * trace);
int  monitor_trace_parse_next_entry(FILE * trace, char * location, long * timestamp, double * values, int * trace_values_index);
int  monitor_trace_match_location_hwloc_obj(char * trace_location, hwloc_obj_t location_to_match);

