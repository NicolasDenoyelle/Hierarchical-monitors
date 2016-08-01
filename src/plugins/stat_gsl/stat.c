#include <hmon.h>
#include <gsl/gsl_statistics.h>
#include <stdlib.h>

double gsl_eventset_var(struct monitor * m){
    unsigned n = m->n_events;
    double * events = &(m->events[m->last*n]);
    return gsl_stats_variance(events, 1, n);
}

double gsl_samples_var(struct monitor * m){
    return gsl_stats_variance(m->samples, 1, m->window);
}

