#include <gsl/gsl_vector_double.h>
#include <gsl/gsl_vector_ulong.h>
#include "../../hmon.h"

struct centroids{
    unsigned           m;         /* Number of points */ 
    unsigned           n;         /* Number of coordinates */
    unsigned           k;         /* Number of centroids */
    gsl_vector       * points;    /* vector of m*n element with stride m */
    gsl_vector       * centroids; /* vector of k*n element with stride m */
    gsl_vector_ulong * col;       /* points colors: size=m */
    gsl_vector_ulong * ncol;      /* points per centroid: size=k */
};

struct centroids * new_centroids(const unsigned m, const unsigned n, const unsigned k);
void               delete_centroids(struct centroids * c);
void               centroids_set(struct centroids * c, const double * points, const size_t offset, const unsigned n);
void               kmean(struct centroids * c, unsigned iter);
void               gsl_vector_print(const gsl_vector * v);
    
#define KMEAN_ITER 10
#define K 2
/**
 * Perform K-mean clustering into K clusters on timestamps and events, and output timestamp barycentre of barycentre.
 * @param m: the monitor used for clustering
 * @return The timestamp barycentre
 **/
double centroid_clustering(hmon hmon){
    unsigned m = hmon->events.rows, n = hmon->events.cols;

    /* initialize centroids */
    struct centroids *c = hmon->userdata;
    if(c == NULL){
	c = new_centroids(m, n, K);
	hmon->userdata=c;
    }

    /* Set events into centroids points */
    centroids_set(c, hmon->events.data, 0, n-1);
    
    /* centroids move */
    kmean(c, KMEAN_ITER);
    
    /**** The compute centroids barycentre */
    double ts_max, ts_min;
    gsl_vector_view ts = gsl_vector_subvector_with_stride(c->points, 0, 1, c->m);
    ts_max = gsl_vector_max(&ts.vector);
    ts_min = gsl_vector_min(&ts.vector);
    
    gsl_vector * barycentre = gsl_vector_calloc(c->n);
    for(unsigned i = 0; i < c->k; i++){
	gsl_vector_view centroid = gsl_vector_subvector_with_stride(c->centroids, i*c->n, 1, c->n);
	gsl_vector_add(barycentre, &centroid.vector);
    }
    gsl_vector_scale(barycentre,1.0/c->k);
    double ts_barycentre = barycentre->data[0];
    gsl_vector_free(barycentre);
    return (ts_barycentre-ts_min)/(ts_max-ts_min);
}

