#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_vector_ulong.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multimin.h>
#include <gsl/gsl_multifit.h>
#include "../../hmon.h"

#define MAX_ITER 10

/*********************************** plugin functions ********************************/
/**
 * Perform kmean clustering of each row point of hmatrix. Output each row point label.
 * There are as many points monitor's window size.
 **/
void clustering(hmon);

/**
 * Perform least square fitting on hmatrix rows. 
 * First columns is the value to fit.
 * Second to before last columns are the variable to parameterize.
 * Output prediction squared error, 
 * and model parameters (one parameter per matrix columns -2).
 **/
void lsq_fit(hmon);
/************************************************************************************/
    
    
/*********************************** features utils **********************************/
struct scaling{
    double center;
    double scale;
};

struct scaling gsl_vector_normalize(gsl_vector * v);
void           gsl_matrix_normalize_columns(gsl_matrix * mat, struct scaling * scales);
void           gsl_matrix_normalize_rows(gsl_matrix * mat, struct scaling * scales);
gsl_matrix     to_gsl_matrix(double * values, const unsigned m, const unsigned n);
gsl_vector     to_gsl_vector(double * values, const unsigned n);
gsl_vector *   gsl_vector_dup(const gsl_vector * v);
gsl_matrix *   gsl_matrix_dup(const gsl_matrix * m);
/************************************************************************************/


/************************************* clustering ***********************************/
#define K 2

typedef struct centroids{
    unsigned           m;         /* Number of points */
    unsigned           n;         /* Number of coordinates */
    unsigned           k;         /* Number of centroids */
    gsl_matrix       * centroids; /* k rows, n columns */
    gsl_vector_ulong * label;     /* points labels: size=m */
    gsl_vector_ulong * nlab;      /* points per centroid: size=k */
} * centroids;

void               delete_centroids(centroids);
centroids          kmean(const gsl_matrix * points, unsigned n_centroids);
/************************************************************************************/


/*********************************** linear model ***********************************/
#define LAMBDA 1000

typedef struct linear_model{
    const gsl_matrix *          X;      /* The input row */
    const gsl_vector *          y;      /* The target output */
    gsl_vector *                y_pred; /* The model fit on X */
    double                      lambda; /* The regulizer value */
    gsl_multimin_fdfminimizer * s;      /* Opaque object used to minimize objective function */
} * lm;

lm     new_linear_model    (const unsigned n, const double lambda);
void   delete_linear_model(lm model);
void   linear_model_fit    (lm model, const gsl_matrix * X, gsl_vector * Theta, const gsl_vector * y);
double linear_model_predict(const gsl_vector * Theta, const gsl_vector * x);
double linear_model_xvalid (const gsl_matrix * X_valid, const gsl_vector * Theta, const gsl_vector * y);

