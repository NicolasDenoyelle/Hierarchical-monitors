#include "../../hmon.h"
#include "learning.h"

/**
 * Perform K-mean clustering into K clusters on events, and output each event label.
 **/
void clustering(hmatrix events, __attribute__ ((unused)) unsigned last, double * samples, unsigned n_samples, __attribute ((unused)) void ** userdata){
    /* Normalize events */
    gsl_matrix * normalized_points = to_gsl_matrix(events.data, events.rows, events.cols);
    gsl_matrix_normalize_columns(normalized_points, NULL);
    
    /* centroids move */
    centroids c = kmean(normalized_points, K);
    
    /**** Output points label */
    for(unsigned i=0; i<events.rows && i<n_samples; i++){samples[i] = gsl_vector_ulong_get(c->label, i);}

    /* Cleanup */
    gsl_matrix_free(normalized_points);
    delete_centroids(c);
}

/* fit a linear model out of events */
void lsq_fit(hmatrix events, __attribute__ ((unused)) unsigned last, double * samples, unsigned n_samples, void ** userdata){
    /* Output prediction error */
    samples[0] = cblas_ddot(n_samples-1, &hmat_get_row(events, last)[1], 1, &samples[1], 1);
    samples[0] = samples[0] - hmat_get_row(events, last)[0];
    samples[0] = samples[0]*samples[0]/(n_samples-1);
    /* Fit full matrix only if matrix is full */
    if(last < events.rows-1){return;}
    
    /* Normalize events */
    gsl_matrix * matrix_events = to_gsl_matrix(events.data, events.rows, events.cols);
    
    /* Extract features(events without timesteps and target) and target(first event) */
    gsl_vector_const_view y = gsl_matrix_const_column(matrix_events, 0);
    gsl_matrix_const_view X = gsl_matrix_const_submatrix(matrix_events, 0, 1, events.rows, n_samples-1);

    /* Build the model */
    lm lm = *userdata;
    if(lm == NULL){lm = *userdata = new_linear_model(n_samples-1, LAMBDA);}
    linear_model_fit(lm, &X.matrix, &y.vector);
    
    /* Output model parameters */
    for(unsigned i=1; i<n_samples; i++){samples[i] = gsl_vector_get(lm->Theta, i-1);}
    
    /* Cleanup */
    /* delete_linear_model(lm); */
}


