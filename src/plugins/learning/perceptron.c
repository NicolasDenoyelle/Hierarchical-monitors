#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cblas.h>
#include <float.h>

/**
 * This file implements the basic tools to learn.
**/

static void   features_scale(double * X, const double * Unit, const int n, const int m);
static double gradiant_descent(const int m, const int n, const double * X, double * Theta, double * Y, const double alpha);
static double hypothesis(const int n, const double * X, const double * Theta);
double*       Unit(const int n);

#define ALPHA 0.03
#define TAU   3

struct perceptron{
    double *  Theta;         /* Features parameters     */
    double    alpha;         /* Learning rate           */
    int       n;             /* Number of features      */
};

struct perceptron *
new_perceptron(const int n, __attribute__ ((unused)) const double max, __attribute__ ((unused)) const double min)
{
    struct perceptron * p = malloc(sizeof(*p));
    p->n = n;
    p->alpha = ALPHA;
    p->Theta = Unit(n);
    return p;
}


void delete_perceptron(struct perceptron * p){free(p->Theta); free(p);}

void
perceptron_fit_by_gradiant_descent(struct perceptron * p, const double * X, double * Y, const int m)
{
    gradiant_descent(m, p->n, X, p->Theta, Y, p->alpha);
}


double perceptron_output(struct perceptron * p, double * X){return hypothesis(p->n, X, p->Theta);}


/**
 * Compute prediction.
 * @param n: the number of features.
 * @param X: the features value.
 * @param Theta: the features coefficients.
 * @return The predicition value.
 **/
static double hypothesis(const int n, const double * X, const double * Theta){return cblas_ddot(n, X, 1, Theta, 1);}


/**
 * Minimize 2*((Theta*X - Y)^2)/m with iterative method based on gradiant descent.
 * Theta is updated and Y is modified as side effect of calling dgemv.
 * @param m: the number of sample. X rows
 * @param n: the number of features. X columns
 * @param X: the features value: m*n: m row, n columns. 
 * @param Theta: the features coefficients.
 * @param Y: the goal values to predict: m samples. 
 * @param alpha: the learning parameter.
 * @return The mean square cost with previous Theta and current Y.
 **/
static double
gradiant_descent(const int m, const int n, const double * X, double * Theta, double * Y, const double alpha)
{
    cblas_dgemv(CblasRowMajor, CblasNoTrans, m, n, 1,        X, n, Theta, 1, -1, Y    , 1);
    cblas_dgemv(CblasRowMajor, CblasTrans,   m, n, -alpha/m, X, n, Y    , 1, 1 , Theta, 1);
    return 2*cblas_ddot(m,Y,1,Y,1)/m;
}


/**
 * Performs per feature scaling, let X a single feature vector of m elements: X := X/||X|| - 1/m
 * X is updated to a scaled value.
 * @param X the features values to scale.
 * @param Unit: a unit vecotr (1,1,1,1,...) of length m.
 * @param n: the number of features. X columns
 * @param m: the number of sample. X rows
 **/
static void
features_scale(double * X, const double * Unit, const int n, const int m)
{
    for(int i = 0; i<n; i++){
	double norm_i = cblas_dnrm2(m,&(X[i]), n);
	cblas_dscal(m, 1/norm_i, &(X[i]), n);
	cblas_daxpy(m, 1/m, Unit, 1, &(X[i]), n);
    }
    
}


