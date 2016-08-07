#include "../../hmon.h"
#include "learning.h"

#define KMEAN_ITER 10
#define K 2

/**
 * Perform K-mean clustering into K clusters on events, and each event label.
 **/
void clustering(hmatrix events, __attribute__ ((unused)) unsigned last, double * samples, unsigned n_samples){
    /* Normalize events */
    gsl_matrix * normalized_points = to_gsl_matrix(events.data, events.rows, events.cols);
    gsl_matrix_normalize_columns(normalized_points);
    
    /* centroids move */
    centroids c = kmean(normalized_points, K, KMEAN_ITER);
    
    /**** Output points label */
    for(unsigned i=0; i<events.rows && i<n_samples; i++){samples[i] = gsl_vector_ulong_get(c->col, i);}

    /* Cleanup */
    gsl_matrix_free(normalized_points);
    delete_centroids(c);
}

