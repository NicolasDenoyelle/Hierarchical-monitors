#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_vector_ulong.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_blas.h>

/*********************************** features utils **********************************/
void         gsl_vector_print(const gsl_vector * v);
void         gsl_vector_normalize(gsl_vector * v);
void         gsl_matrix_normalize_columns(gsl_matrix * mat);
void         gsl_matrix_normalize_rows(gsl_matrix * mat);
gsl_matrix * to_gsl_matrix(const double * values, const unsigned m, const unsigned n);
/************************************************************************************/


/************************************* clustering ***********************************/
#define KMEAN_ITER 10
#define K 2

typedef struct centroids{
    unsigned           m;         /* Number of points */
    unsigned           n;         /* Number of coordinates */
    unsigned           k;         /* Number of centroids */
    gsl_matrix       * centroids; /* k rows, n columns */
    gsl_vector_ulong * label;       /* points colors: size=m */
    gsl_vector_ulong * nlab;      /* points per centroid: size=k */
} * centroids;

void               delete_centroids(centroids);
centroids          kmean(const gsl_matrix * points, unsigned n_centroids, unsigned iter);
/************************************************************************************/


/*********************************** linear model ***********************************/
typedef struct linear_model{
    gsl_matrix * Theta;
    double       alpha;
} * lm;

