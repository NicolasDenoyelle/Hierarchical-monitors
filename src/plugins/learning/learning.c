#include "../../hmon.h"
#include "learning.h"

/**
 * Perform K-mean clustering into K clusters on events, and output each event label.
 **/
void clustering(hmon monitor){
    /* Normalize events */
    unsigned m = monitor->total>monitor->window ? monitor->window : monitor->total;
    gsl_matrix events = to_gsl_matrix(monitor->events, m, monitor->n_events+1);
    gsl_matrix * normalized_events = gsl_matrix_dup(&events);
    gsl_matrix_normalize_columns(normalized_events, NULL);
    
    /* centroids move */
    centroids c = kmean(normalized_events, K);
    
    /**** Output points label */
    for(unsigned i=0; i<m && i<monitor->n_samples; i++){monitor->samples[i] = gsl_vector_ulong_get(c->label, i);}

    /* Cleanup */
    gsl_matrix_free(normalized_events);
    delete_centroids(c);
}

/* fit a linear model out of events when window is full and coefficient of determination */
void lsq_fit(hmon monitor){
    double * output = monitor->samples;
    unsigned m = monitor->total>monitor->window ? monitor->window : monitor->total;
    gsl_matrix events = to_gsl_matrix(monitor->events, m, monitor->n_events+1);
    /* Extract features(events without timesteps and target) and target(first event) */
    gsl_vector Theta = to_gsl_vector(&monitor->samples[1], monitor->n_events-1);
    gsl_vector_const_view y = gsl_matrix_const_column(&events, 0);
    gsl_matrix_const_view X = gsl_matrix_const_submatrix(&events, 0, 1, m, monitor->n_events-1);
    
    /* Output coefficient of determination */
    double SS_res, SS_tot;
    double y_mean = gsl_stats_mean(y.vector.data, y.vector.stride, y.vector.size);
    gsl_vector * y_pred = gsl_vector_alloc(X.matrix.size1);
    gsl_vector_memcpy(y_pred,&y.vector);
    /* y-y_mean */
    gsl_vector_add_constant(y_pred, -y_mean);
    /* (y-y_mean)^2 */
    gsl_blas_ddot(y_pred,y_pred, & SS_tot);
    /* y */
    gsl_vector_add_constant(y_pred, y_mean);
    /* Theta*X - y */
    gsl_blas_dgemv(CblasNoTrans, 1, &X.matrix, &Theta, -1, y_pred);
    /* (Theta*X - y)^2 */
    gsl_blas_ddot(y_pred,y_pred, & SS_res);
    gsl_vector_free(y_pred);
    /* R^2 Closer to one is better */
    *output = 1-SS_res/SS_tot;
    
    /* Fit full matrix only */
    if(monitor->last == monitor->window-1){
	/* Build the model */
	lm lm = monitor->userdata;
	if(lm == NULL){lm = monitor->userdata = new_linear_model(monitor->n_events-1, LAMBDA);}
	linear_model_fit(lm, &X.matrix, &Theta, &y.vector);
    }
}


