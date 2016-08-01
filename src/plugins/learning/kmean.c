#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include <gsl/gsl_vector_double.h>
#include <gsl/gsl_vector_ulong.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_blas.h>

/************************************** HEADER **************************************/
struct centroids{
    unsigned           m;         /* Number of points */ 
    unsigned           n;         /* Number of coordinates */
    unsigned           k;         /* Number of centroids */
    gsl_vector       * points;    /* vector of m*n element with stride m */
    gsl_vector       * centroids; /* vector of n*k element with stride m */
    gsl_vector_ulong * col;       /* points colors: size=m */
    gsl_vector_ulong * ncol;      /* points per centroid: size=k */
};

struct centroids * new_centroids(const unsigned m, const unsigned n, const unsigned k);
void               delete_centroids(struct centroids * c);
void               centroids_set(struct centroids * c, const double * points, const size_t offset, const unsigned n);
void               kmean(struct centroids * c, unsigned iter);

/************************************************************************************/

void gsl_vector_print(const gsl_vector * v){
    unsigned i;
    printf("[%lf", gsl_vector_get(v,0));
    for(i=1;i<v->size;i++){printf(", %lf", gsl_vector_get(v,i));}
    printf("]");
}

static void gsl_vector_normalize(gsl_vector * v){
    double max  = gsl_vector_max(v);
    double min = gsl_vector_min(v);
    double mu = gsl_stats_mean(v->data, 1, v->size);
    gsl_vector_add_constant(v, -mu);
    if(max!=min){gsl_vector_scale(v,1/(max-min));}
}

struct centroids * new_centroids(const unsigned m, const unsigned n, const unsigned k){
    struct centroids * c = malloc(sizeof(*c));
    c->m=m; c->n=n; c->k=k;
    c->points    = gsl_vector_alloc(m*n);
    c->centroids = gsl_vector_alloc(n*k);
    c->col       = gsl_vector_ulong_alloc(m);
    c->ncol      = gsl_vector_ulong_alloc(k);
    return c;
}

void delete_centroids(struct centroids * c){
    free(c->col);
    free(c->ncol);
    gsl_vector_free(c->points);
    gsl_vector_free(c->centroids);
}

/**
 * Set points into centroids.
 * @param c      [out]: the centroids containing points to set.
 * @param n       [in]: the number of coordinates to set.
 * @param points  [in]: the points values. n*c->m values
 * @param offset  [in]: among the centroids number of coordinates (c->n), the first coordinate to set. offset + n must be less or equal than c->n.
 **/
void centroids_set(struct centroids * c, const double * points, const size_t offset, const unsigned n){
    unsigned i;
    gsl_vector_view feature;
    double * data = &(c->points->data[c->m*offset]);
    memcpy(data, points, n*c->m*sizeof(*points));
    for(i = offset; i < n+offset; i++){
    	feature = gsl_vector_subvector_with_stride(c->points, i*c->m, 1, c->m);
    	gsl_vector_normalize(&feature.vector);
    }
}

static void centroids_init(struct centroids * c){
    unsigned i;
    for(i=0;i<c->k;i++){
	gsl_vector_view centroid = gsl_vector_subvector_with_stride(c->centroids, i*c->n, 1, c->n);
	gsl_vector_view point = gsl_vector_subvector_with_stride(c->points, rand()%c->m, c->k, c->n);
	gsl_vector_memcpy(&centroid.vector, &point.vector);
    }
}

static void centroids_color(struct centroids * c){
    unsigned i, j, color_assign = 0;
    double distance_min, distance;
    /* store difference between sample and centroid */
    
    gsl_vector * diff = gsl_vector_calloc(c->n);
    gsl_vector_ulong_set_zero(c->ncol);

    for(i=0;i<c->m;i++){
	gsl_vector_view point = gsl_vector_subvector_with_stride(c->points, i, c->m, c->n);
	distance_min = DBL_MAX;
	for(j=0;j<c->k;j++){
	    gsl_vector_view centroid = gsl_vector_subvector_with_stride(c->centroids, j*c->n, 1, c->n);
	    gsl_vector_memcpy(diff, &point.vector);
	    gsl_vector_sub (diff,  &centroid.vector);
	    gsl_blas_ddot(diff, diff, &distance);
	    if(distance < distance_min){distance_min=distance; color_assign=j;}
	}
	gsl_vector_ulong_set(c->col, i, color_assign);
	gsl_vector_ulong_set(c->ncol, color_assign, 1+gsl_vector_ulong_get(c->ncol, color_assign));
    }
    gsl_vector_free(diff);
}

static void centroids_center(struct centroids * c){
    unsigned i;
    gsl_vector_set_zero(c->centroids);
    for(i=0; i<c->m; i++){
	unsigned long color = gsl_vector_ulong_get(c->col,i);
	gsl_vector_view centroid = gsl_vector_subvector_with_stride(c->centroids, color*c->n, 1, c->n);
	gsl_vector_view point = gsl_vector_subvector_with_stride(c->points, i, c->m, c->n);
	gsl_vector_add(&centroid.vector, &point.vector);
    }
    for(i=0; i<c->k; i++){
	unsigned long ncol = gsl_vector_ulong_get(c->ncol, i);
	gsl_vector_view centroid = gsl_vector_subvector_with_stride(c->centroids, i*c->n, 1, c->n);
	if(ncol){gsl_vector_scale(&centroid.vector, 1.0/(double)ncol);}
    }
}
    
/** 
 * Run kmean algorithm 
 * @param c: the centroids to set at its points weight center.
 * @param iter: the number of iteration of the algorithm.
**/
void kmean(struct centroids * c, unsigned iter){    
    /* if more centroids than sample, then set centroids to samples. */
    if(c->k >= c->m){
	gsl_vector_set_zero(c->centroids);
	for(unsigned i = 0; i<c->m; i++){
	    gsl_vector_view point = gsl_vector_subvector_with_stride(c->points, i, c->m, c->n);
	    gsl_vector_view centroid = gsl_vector_subvector_with_stride(c->centroids, i*c->n, 1, c->n);
	    memcpy(centroid.vector.data, point.vector.data, c->m*sizeof(double));
	}
	return;
    }

    centroids_init(c);
    while(iter--){
	centroids_color(c);
	centroids_center(c);
    }
}

