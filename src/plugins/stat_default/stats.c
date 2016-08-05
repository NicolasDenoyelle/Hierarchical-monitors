#include <float.h>
#include "hmon.h"

#define STAT_MAX(a,b) ((a)>(b)?(a):(b))
#define STAT_MIN(a,b) ((a)<(b)?(a):(b))

void monitor_events_max(hmatrix in, __attribute__ ((unused)) unsigned row_offset, double * out, unsigned out_size){
    unsigned r,c,cols = STAT_MIN(in.cols, out_size)-1;
    double * row_in;

    /* Init output */
    for(c=0;c<cols;c++){out[c] = DBL_MIN;}

    /* compute max row by row */
    for(r=0; r<in.rows; r++){
	row_in = hmat_get_row(in,r);
	for(c=0;c<cols;c++){out[c] = STAT_MAX(out[c], row_in[c]);}
    }
}

void monitor_events_min(hmatrix in, __attribute__ ((unused)) unsigned row_offset, double * out, unsigned out_size){
    unsigned r,c,cols = STAT_MIN(in.cols, out_size)-1;
    double * row_in;

    /* Init output */
    for(c=0;c<cols;c++){out[c] = DBL_MAX;}

    /* compute min row by row */
    for(r=0; r<in.rows; r++){
	row_in = hmat_get_row(in,r);
	for(c=0;c<cols;c++){out[c] = STAT_MIN(out[c], row_in[c]);}
    }
}

void monitor_events_sum(hmatrix in, __attribute__ ((unused)) unsigned row_offset, double * out, unsigned out_size){
    unsigned r,c,cols = STAT_MIN(in.cols, out_size)-1;
    double * row_in;

    /* Init output */
    for(c=0;c<cols;c++){out[c] = 0;}

    /* compute sum row by row */
    for(r=0; r<in.rows; r++){
	row_in = hmat_get_row(in,r);
	for(c=0;c<cols;c++){out[c] += row_in[c];}
    }
}

void monitor_events_mean(hmatrix in, unsigned row_offset, double * out, unsigned out_size){
    unsigned c,cols = STAT_MIN(in.cols, out_size)-1;
    monitor_events_sum(in, row_offset, out, out_size);
    for(c=0;c<cols;c++){out[c] /= in.rows;}
}

void monitor_events_var(hmatrix in, __attribute__ ((unused)) unsigned row_offset, double * out, unsigned out_size){
    unsigned r, c,cols = STAT_MIN(in.cols, out_size)-1;
    double * row_in;
    double sum[cols], square_sum[cols], ct = 1.0/((double)in.rows*(double)in.rows);
    
    for(c=0;c<cols;c++){sum[c] = square_sum[c] = 0;}
    for(r=0; r<in.rows; r++){
	row_in = hmat_get_row(in,r);
	for(c=0;c<cols;c++){sum[c]+=row_in[c]; square_sum[c] += row_in[c]*row_in[c];}
    }

    for(c=0;c<cols;c++){sum[c] = ct*sum[c]*sum[c];}
    ct = (1.0/(double)in.rows);
    for(c=0;c<cols;c++){out[c] = ct*square_sum[c]-sum[c];}
}

void monitor_evset_var(hmatrix in, unsigned row_offset, double * out, __attribute__ ((unused)) unsigned out_size){
    unsigned c,cols  = in.cols-1;
    double * row_in  = hmat_get_row(in,row_offset);
    double sum = 0, square_sum = 0;

    for(c=0;c<cols;c++){sum+=row_in[c]; square_sum += row_in[c]*row_in[c];}
    out[0] = square_sum/(double)cols - (1.0/(double)(cols*cols))*sum*sum;
}

