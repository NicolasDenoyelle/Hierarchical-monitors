#include <gsl/gsl_matrix_double.h>
#include <gsl/gsl_vector_ulong.h>
#include <gsl/gsl_statistics.h>
#include <gsl/gsl_blas.h>
#include <gsl/gsl_multimin.h>

/*********************************** plugin functions ********************************/
void clustering(hmatrix, unsigned, double*, unsigned);
void lsq_fit(hmatrix, unsigned, double*, unsigned);
/************************************************************************************/
    
    
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
    gsl_vector_ulong * label;     /* points labels: size=m */
    gsl_vector_ulong * nlab;      /* points per centroid: size=k */
} * centroids;

void               delete_centroids(centroids);
centroids          kmean(const gsl_matrix * points, unsigned n_centroids, unsigned iter);
/************************************************************************************/


/*********************************** linear model ***********************************/
typedef struct linear_model{
    unsigned                    n;      /* The number of parameters of the model */
    gsl_vector *                Theta;  /* The model parameters */
    const gsl_matrix *          X;      /* The input row */
    const gsl_vector *          y;      /* The target output */
    double                      lambda; /* The regulizer value */
    gsl_multimin_fdfminimizer * s;      /* Opaque object used to minimize objective function */
} * lm;

lm     new_linear_model    (const unsigned n, const double lambda);
void   delete_linear_model(lm model);
void   linear_model_fit    (lm model, const gsl_matrix * X, const gsl_vector * y);
double linear_model_predict(const lm model, const gsl_vector * x);
double linear_model_xvalid (const lm model, const gsl_matrix * X_valid, const gsl_vector * y);

