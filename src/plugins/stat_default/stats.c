#include "hmon.h"


double monitor_samples_last(struct monitor * monitor){
    return monitor->samples[monitor->last];
}

double monitor_evset_max(struct monitor * monitor){
    unsigned n = monitor->n_events;
    double * events = &(monitor->events[monitor->last*n]);
    double max = events[--n];
    while(n-->0)
	max = max > events[n] ? max : events[n];
    return max;
}

double monitor_evset_min(struct monitor * monitor){
    unsigned n = monitor->n_events;
    double * events = &(monitor->events[monitor->last*n]);
    double min = events[--n];
    while(n-->0)
	min = min < events[n] ? min : events[n];
    return min;
}

double monitor_evset_sum(struct monitor * monitor){
    double result = 0;
    unsigned n = monitor->n_events;
    double * events = &(monitor->events[monitor->last*n]);
    while(n--)
	result+=events[n];
    return result;
}

