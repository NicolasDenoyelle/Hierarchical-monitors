#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <math.h>
#include <gsl/gsl_vector_double.h>
#include <gsl/gsl_blas.h>
#include "../../hmon.h"

static void print_vector(const gsl_vector * v){
    unsigned i;
    printf("[%lf", gsl_vector_get(v,0));
    for(i=1;i<v->size;i++){printf(", %lf", gsl_vector_get(v,i));}
    printf("]");
}

static void colorize_samples(unsigned * colors,
		     unsigned * centroid_n_colors,
		     const gsl_vector ** samples, 
		     const unsigned n_samples,
		     const gsl_vector ** centroids,
		     const unsigned n_centroids)
{
    unsigned i, j, color_assign;
    double distance_min, distance;
    /* store difference between sample and centroid */
    gsl_vector * difference = gsl_vector_calloc(samples[0]->size); 
    for(i=0;i<n_centroids;i++){centroid_n_colors[i] = 0;}
    for(i=0;i<n_samples;i++){
	distance_min = DBL_MAX;
	for(j=0;j<n_centroids;j++){
	    gsl_vector_memcpy(difference, samples[i]);
	    gsl_vector_sub (difference,  centroids[j]);
	    gsl_blas_ddot(difference, difference, &distance);
	    if(distance < distance_min){
		distance_min=distance;
		color_assign = j;
	    }
	}
	colors[i] = color_assign;
	centroid_n_colors[color_assign]++;
    }
    gsl_vector_free(difference);
}

static void adjust_centroids(const unsigned * colors,
			       const unsigned * centroid_n_colors,
			       const gsl_vector ** samples, 
			       const unsigned n_samples,
			       gsl_vector ** centroids,
			       const unsigned n_centroids)
{
    unsigned i;
    for(i=0; i<n_centroids; i++){gsl_vector_set_zero(centroids[i]);}
    for(i=0; i<n_samples; i++){gsl_vector_add(centroids[colors[i]],samples[i]);}
    for(i=0; i<n_centroids; i++){
	if(centroid_n_colors[i]){gsl_vector_scale(centroids[i], 1.0/(double)centroid_n_colors[i]);}
	else{gsl_vector_set_zero(centroids[i]);}
    }
}

#define KMEAN_THRESHOLD 0.001
static void kmean(const gsl_vector ** samples, const unsigned n_samples, gsl_vector ** centroids, const unsigned n_centroids){
     unsigned i; 
    /* if more centroids than sample, then set centroids to samples. */
    if(n_centroids >= n_samples){
	for(i=0; i<n_samples; i++){gsl_vector_memcpy(centroids[i], samples[i]);}
	for(i=n_samples; i< n_centroids; i++){gsl_vector_set_zero(centroids[i]);}
	return;
    }

    /* centroids init */
    for(i=0; i<n_centroids; i++){gsl_vector_memcpy(centroids[i], samples[i]);}

       
    /* algorithm */
    unsigned * colors   = malloc(sizeof(*colors)*n_samples);
    unsigned * n_colors = malloc(sizeof(*n_colors)*n_centroids);
    gsl_vector * c = gsl_vector_alloc(centroids[0]->size);
    double c_norm = 2*KMEAN_THRESHOLD;
    
    while(c_norm > KMEAN_THRESHOLD){
	/* colorize samples */
	colorize_samples(colors, n_colors, samples, n_samples, (const gsl_vector**)centroids, n_centroids);
	/* move centroids */
	gsl_vector_set_zero(c);
	for(i=0; i<n_centroids; i++){gsl_vector_add(c,centroids[i]);}
	adjust_centroids((const unsigned *)colors, (const unsigned *)n_colors, samples, n_samples, centroids, n_centroids);
	for(i=0; i<n_centroids; i++){gsl_vector_sub(c,centroids[i]);}
	c_norm = gsl_blas_dnrm2(c);
    }

    gsl_vector_free(c);
    free(colors);
    free(n_colors);
}


#define N_CENTROIDS 2
#define SEP_THRESHOLD 0.1
/**
 * Perform K-mean clustering into 2 clusters on timestamps and samples, and output timestamp boundary between clusters.
 * @param m: the monitor used for clustering
 * @return The timestamp boundary
 **/
double centroid_clustering(struct monitor * m){
    double separator;
    unsigned i, j, idx, n_samples = m->window>m->total ? m->total:m->window;
    double t0, tf;
    gsl_vector ** samples = malloc(sizeof(*samples)*n_samples);
    gsl_vector ** centroids = m->userdata;
    
    t0 = m->timestamps[(m->current+1)%m->window];
    tf = m->timestamps[m->current];
    for(i=0;i<n_samples;i++){
	samples[i] = gsl_vector_calloc(m->n_events+1);
	idx = (m->current-i)%m->window;
	/* time normalization */
	if(tf!=t0){gsl_vector_set(samples[i], 0, (double)(m->timestamps[idx]-t0)/(tf-t0));}
	else{gsl_vector_set(samples[i], 0, 0);}
	/* others features are assumed to be normalized */
	for(j=0;j<m->n_events;j++){gsl_vector_set(samples[i], j+1, m->events[idx][j]);}
    }

    /* centroids alloc */
    if(centroids == NULL){
	centroids = m->userdata = malloc(sizeof(*centroids)*N_CENTROIDS);
	for(i=0; i< N_CENTROIDS; i++){
	    centroids[i] = gsl_vector_alloc(m->n_events+1);
	}
    }

    /* centroids move */
    kmean((const gsl_vector**)samples, n_samples, centroids, N_CENTROIDS);
    
    /**** This part assumes N_CENTROIDS = 2 */
    separator = gsl_vector_get(centroids[0],0)+gsl_vector_get(centroids[1],0)-0.5;
    /* if(separator-SEP_THRESHOLD < 0.5 && separator+SEP_THRESHOLD > 0.5){separator = 0.5;} */
    /**** end N_CENTROIDS = 2 */

    /* Cleanup */
    /* for(i=0;i<N_CENTROIDS;i++){gsl_vector_free(centroids[i]);} */
    /* free(centroids); */
    for(i=0;i<n_samples;i++){gsl_vector_free(samples[i]);}
    free(samples);
    return separator;
}


