#include "../../hmon.h"
#include "learning.h"

#define KMEAN_ITER 10
#define K 2
#define LAMBDA 1000

/**
 * Perform K-mean clustering into K clusters on events, and output each event label.
 **/
void clustering(hmatrix events, __attribute__ ((unused)) unsigned last, double * samples, unsigned n_samples){
    /* Normalize events */
    gsl_matrix * normalized_points = to_gsl_matrix(events.data, events.rows, events.cols);
    gsl_matrix_normalize_columns(normalized_points);
    
    /* centroids move */
    centroids c = kmean(normalized_points, K, KMEAN_ITER);
    
    /**** Output points label */
    for(unsigned i=0; i<events.rows && i<n_samples; i++){samples[i] = gsl_vector_ulong_get(c->label, i);}

    /* Cleanup */
    gsl_matrix_free(normalized_points);
    delete_centroids(c);
}

/* fit a linear model out of events */
void lsq_fit(hmatrix events, __attribute__ ((unused)) unsigned last, double * samples, unsigned n_samples){
    /* Fit full matrix only */
    if(last < n_samples-1){return;}
    
    /* Normalize events */
    gsl_matrix * normalized_events = to_gsl_matrix(events.data, events.rows, events.cols);
    gsl_matrix_normalize_columns(normalized_events);

    /* Extract features and target(first column) */
    gsl_vector_const_view y = gsl_matrix_const_column(normalized_events, 0);
    gsl_matrix_const_view X = gsl_matrix_const_submatrix(normalized_events, 0, 1, normalized_events->size1, normalized_events->size2-1);

    /* Build the model */
    lm model = new_linear_model(X.matrix.size2, LAMBDA);
    linear_model_fit(model, &X.matrix, &y.vector);
    
    /* Output model parameters */
    for(unsigned i=0; i<events.rows && i<n_samples; i++){samples[i] = gsl_vector_get(model->Theta, i);}

    /* Cleanup */
    delete_linear_model(model);
}


