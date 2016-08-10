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

/* fit a linear model out of events */
void lsq_fit(hmon monitor){
    double * output = monitor->samples;
    unsigned m = monitor->total>monitor->window ? monitor->window : monitor->total;
    gsl_matrix events = to_gsl_matrix(monitor->events, m, monitor->n_events+1);
    /* Extract features(events without timesteps and target) and target(first event) */
    gsl_vector Theta = to_gsl_vector(&monitor->samples[1], monitor->n_events-1);
    gsl_vector_const_view y = gsl_matrix_const_column(&events, 0);
    gsl_matrix_const_view X = gsl_matrix_const_submatrix(&events, 0, 1, m, monitor->n_events-1);
    gsl_vector x = to_gsl_vector(&(monitor_get_events(monitor, monitor->last)[1]), monitor->n_events-1);
    
    /* Output prediction error */
    gsl_blas_ddot(&x,&Theta, output);
    double error = abs(*output - monitor_get_event(monitor, monitor->last, 0));
    error /= abs(*output) + abs(monitor_get_event(monitor, monitor->last, 0));
    *output = error;
    
    /* Fit full matrix only if tolerance has is not satisfied */
    if(error > TOL){
	/* printf("Compute fit...\n"); */

	/* Build the model */
	lm lm = monitor->userdata;
	if(lm == NULL){lm = monitor->userdata = new_linear_model(monitor->n_events-1, LAMBDA);}
	linear_model_fit(lm, &X.matrix, &Theta, &y.vector);
		
	/* Cleanup */
	/* delete_linear_model(lm); */
    }
}


