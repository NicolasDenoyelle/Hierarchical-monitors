#include "hmon.h"


double monitor_samples_last(struct monitor * monitor){
    return monitor->samples[monitor->current];
}

double monitor_evset_max(struct monitor * monitor){
    unsigned c = monitor->current, n = monitor->n_events;
    double max = monitor->events[c][--n];
    while(n-->0)
	max = max > monitor->events[c][n] ? max : monitor->events[c][n];
    return max;
}

double monitor_evset_min(struct monitor * monitor){
    unsigned c = monitor->current, n = monitor->n_events;
    double min = monitor->events[c][--n];
    while(n-->0)
	min = min < monitor->events[c][n] ? min : monitor->events[c][n];
    return min;
}

double monitor_evset_sum(struct monitor * monitor){
    double result = 0;
    unsigned c = monitor->current, n = monitor->n_events;
    while(n--)
	result+=monitor->events[c][n];
    return result;
}


