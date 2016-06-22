#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cblas.h>
#include <float.h>

/**
 * This file implements the basic tools to learn.
**/

#define ALPHA 0.03
#define TAU   3

struct perceptron{
    double *  Theta;         /* Features parameters     */
    double    alpha;         /* Learning rate           */
    int       n;             /* Number of features      */
    double    J;             /* Cost function result    */
};

struct perceptron *
new_perceptron(const int n, __attribute__ ((unused)) const double max, __attribute__ ((unused)) const double min)
{
    struct perceptron * p = malloc(sizeof(*p));
    p->n = n;
    p->alpha = ALPHA;
    p->J = DBL_MAX;
    p->Theta = malloc(n*sizeof(*(p->Theta)));
    for(int i = 0; i< n; i++){p->Theta[i] = 1;}
    return p;
}


void delete_perceptron(struct perceptron * p){free(p->Theta); free(p);}

double
perceptron_fit_by_gradiant_descent(struct perceptron * p, const double * X, double * Y, const int m)
{
    cblas_dgemv(CblasRowMajor, CblasNoTrans, m, p->n, 1, X, p->n, p->Theta, 1, -1, Y, 1);
    cblas_dgemv(CblasRowMajor, CblasTrans,   m, p->n, -p->alpha/m, X, p->n, Y, 1, 1 , p->Theta, 1);
    double J = 2*cblas_ddot(m,Y,1,Y,1)/m;
    /* if(J > p->J){p->alpha /= TAU;} */
    /* if(J < p->J){p->alpha *= TAU;} */
    /* p->J= J; */
    return J;
}


double perceptron_output(const struct perceptron * p, const double * X){return cblas_ddot(p->n, X, 1, p->Theta, 1);}

