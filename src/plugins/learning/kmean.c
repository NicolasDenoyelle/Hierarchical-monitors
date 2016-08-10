#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include "learning.h"

static centroids new_centroids(const unsigned m, const unsigned n, const unsigned k){
    centroids c = malloc(sizeof(*c));
    c->m=m; c->n=n; c->k=k;
    c->centroids = gsl_matrix_alloc(m,k);
    c->label       = gsl_vector_ulong_alloc(m);
    c->nlab      = gsl_vector_ulong_alloc(k);
    return c;
}

void delete_centroids(centroids c){
    free(c->label);
    free(c->nlab);
    gsl_matrix_free(c->centroids);
}


static void centroids_init(centroids c, const gsl_matrix * points){
    unsigned i;
    for(i=0;i<c->k;i++){
	gsl_vector_view centroid = gsl_matrix_row(c->centroids, i);
	gsl_vector_const_view point = gsl_matrix_const_row(points, rand()%c->m);
	gsl_vector_memcpy(&centroid.vector, &point.vector);
    }
}

static void centroids_color(centroids c, const gsl_matrix * points){
    unsigned i, j, color_assign = 0;
    double distance_min, distance;
    
    /* store difference between sample and centroid */
    gsl_vector * diff = gsl_vector_calloc(c->n);
    gsl_vector_ulong_set_zero(c->nlab);

    for(i=0;i<c->m;i++){
	gsl_vector_const_view point = gsl_matrix_const_row(points, i);
	distance_min = DBL_MAX;
	for(j=0;j<c->k;j++){
	    gsl_vector_view centroid = gsl_matrix_row(c->centroids, j);
	    gsl_vector_memcpy(diff, &point.vector);
	    gsl_vector_sub (diff,  &centroid.vector);
	    gsl_blas_ddot(diff, diff, &distance);
	    if(distance < distance_min){distance_min=distance; color_assign=j;}
	}
	gsl_vector_ulong_set(c->label, i, color_assign);
	gsl_vector_ulong_set(c->nlab, color_assign, 1+gsl_vector_ulong_get(c->nlab, color_assign));
    }
    gsl_vector_free(diff);
}

static void centroids_center(centroids c, const gsl_matrix * points){
    unsigned i;
    gsl_matrix_set_zero(c->centroids);
    for(i=0; i<c->m; i++){
	gsl_vector_view centroid = gsl_matrix_row(c->centroids, gsl_vector_ulong_get(c->label,i));
	gsl_vector_const_view point = gsl_matrix_const_row(points, i);
	gsl_vector_add(&centroid.vector, &point.vector);
    }
    for(i=0; i<c->k; i++){
	unsigned long nlab = gsl_vector_ulong_get(c->nlab, i);
	gsl_vector_view centroid = gsl_matrix_row(c->centroids, i);
	if(nlab){gsl_vector_scale(&centroid.vector, 1.0/(double)nlab);}
    }
}
    
centroids kmean(const gsl_matrix * points, unsigned n_centroids){
    /* Initialize centroids */
    centroids c = new_centroids(points->size1, points->size2, n_centroids);

    /* if more centroids than sample, then set centroids to samples. */
    if(c->k >= c->m){
	gsl_matrix_set_zero(c->centroids);
	memcpy(c->centroids->data, points->data, c->m*c->n*sizeof(double));
	return c;
    }

    centroids_init(c, points);
    unsigned iter = MAX_ITER;
    while(iter--){
	centroids_color(c, points);
	centroids_center(c, points);
    }
    
    return c;
}

