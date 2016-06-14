#include <hmon.h>
#include <gsl/gsl_statistics.h>
#include <stdlib.h>

double gsl_eventset_var(struct monitor * m){
    unsigned i, n = m->n_events, c = m->current;
    long * events = malloc(sizeof(*events) * n);
    double ret;
    for(i=0;i<n;i++){
	events[i]=m->events[c][i];
    }
    ret = gsl_stats_long_variance(events, 1, n);
    free(events);
    return ret;
}

double gsl_samples_var(struct monitor * m){
    return gsl_stats_variance(m->samples, 1, m->n_samples);
}

