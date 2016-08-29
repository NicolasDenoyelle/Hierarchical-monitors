#include <float.h>
#include <hmon/hmonitor.h>

#define STAT_MAX(a,b) ((a)>(b)?(a):(b))
#define STAT_MIN(a,b) ((a)<(b)?(a):(b))

void hmonitor_events_max(hmon m){
    unsigned r, rows = STAT_MIN(m->window, m->total);
    unsigned c, cols = m->n_events;
    double * out = m->samples, *in;

    /* Init output */
    for(c=0;c<cols;c++){out[c] = DBL_MIN;}
    /* compute max row by row */
    for(r=0; r<rows; r++){
        in = hmonitor_get_events(m,r);
	for(c=0;c<cols;c++){out[c] = STAT_MAX(out[c], in[c]);}
    }
}

void hmonitor_events_min(hmon m){
    unsigned r, rows = STAT_MIN(m->window, m->total);
    unsigned c, cols = m->n_events;
    double * out = m->samples, *in;

    /* Init output */
    for(c=0;c<cols;c++){out[c] = DBL_MAX;}
    /* compute min row by row */
    for(r=0; r<rows; r++){
        in = hmonitor_get_events(m,r);
	for(c=0;c<cols;c++){out[c] = STAT_MIN(out[c], in[c]);}
    }
}

void hmonitor_events_sum(hmon m){
    unsigned r, rows = STAT_MIN(m->window, m->total);
    unsigned c, cols = m->n_events;
    double * out = m->samples, *in;

    /* Init output */
    for(c=0;c<cols;c++){out[c] = 0;}

    /* compute sum row by row */
    for(r=0; r<rows; r++){
        in = hmonitor_get_events(m,r);
	for(c=0;c<cols;c++){out[c] += in[c];}
    }
}

void hmonitor_events_mean(hmon m){
    unsigned rows = STAT_MIN(m->window, m->total);
    unsigned c, cols = m->n_events;
    double * out = m->samples;
    hmonitor_events_sum(m);
    for(c=0;c<cols;c++){out[c] /= rows;}
}

void hmonitor_events_var(hmon m){
    unsigned r, rows = STAT_MIN(m->window, m->total);
    unsigned c, cols = m->n_events;
    double * out = m->samples, *in;
    double sum[cols], square_sum[cols], ct = 1.0/((double)rows*rows);
    
    for(c=0;c<cols;c++){sum[c] = square_sum[c] = 0;}
    for(r=0; r<rows; r++){
        in = hmonitor_get_events(m,r);
	for(c=0;c<cols;c++){sum[c]+=in[c]; square_sum[c] += in[c]*in[c];}
    }

    for(c=0;c<cols;c++){sum[c] = ct*sum[c]*sum[c];}
    ct = (1.0/(double)rows);
    for(c=0;c<cols;c++){out[c] = ct*square_sum[c]-sum[c];}
}

void hmonitor_evset_var(hmon m){
    unsigned c, cols = m->n_events;
    double * out = m->samples, *in = hmonitor_get_events(m,m->last);
    double sum = 0, square_sum = 0;

    for(c=0;c<cols;c++){sum+=in[c]; square_sum += in[c]*in[c];}
    *out = square_sum/(double)cols - (1.0/(double)(cols*cols))*sum*sum;
}

