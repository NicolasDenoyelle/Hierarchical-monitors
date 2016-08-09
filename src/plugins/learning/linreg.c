#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include "learning.h"

#define MAX_ITER 100

double h(const gsl_vector * x, const gsl_vector * Theta){
    double ret; gsl_blas_ddot(x,Theta,&ret); return ret;
}

void h_y(const gsl_matrix * X, const gsl_vector * Theta, gsl_vector * y){
    gsl_blas_dgemv(CblasNoTrans, 1, X, Theta, -1, y);
}

double h_y2(const gsl_matrix * X, const gsl_vector * Theta, gsl_vector * y){
    double ret; h_y(X,Theta,y); gsl_blas_ddot(y,y,&ret); return ret;
}

double h_y2_regularized(const gsl_matrix * X, const gsl_vector * Theta, gsl_vector * y, const double lambda){
    double reg; gsl_blas_ddot(Theta,Theta,&reg); return h_y2(X,Theta,y) + lambda*reg;
}

/** 
 * Squared cost function with regularization 
 * First element of x is considered as target y.
 **/
double lsq(const gsl_vector * Theta, void * params){
    double cost;
    lm model = (lm)params;
    gsl_vector * y = gsl_vector_alloc(model->y->size);
    gsl_vector_memcpy(y,model->y);
    cost = 0.5*h_y2_regularized(model->X, Theta, y, model->lambda);
    gsl_vector_free(y);
    return cost;
}

/** 
 * Squared cost function derivatives with regularization 
 * First element of x is considered as target y.
 **/     
void dlsq(const gsl_vector * Theta, void * params, gsl_vector * dTheta){
    lm model = (lm)params;
    gsl_vector * hypothesis_y = gsl_vector_alloc(model->y->size);
    gsl_vector_memcpy(hypothesis_y,model->y);
    h_y(model->X, Theta, hypothesis_y);
    gsl_vector_memcpy(dTheta,Theta);
    gsl_blas_dgemv(CblasTrans, 1, model->X, hypothesis_y, model->lambda, dTheta);
    gsl_vector_free(hypothesis_y);
}

/** 
 * Squared cost function and its derivatives with regularization 
 * First element of x is considered as target y.
 **/     
void lsqdlsq (const gsl_vector * Theta, void * params, double * cost, gsl_vector * dTheta){
    lm model = (lm)params;
    gsl_vector * hypothesis_y = gsl_vector_alloc(model->y->size);
    gsl_vector_memcpy(hypothesis_y,model->y);    
    *cost  = 0.5*h_y2_regularized(model->X, Theta, hypothesis_y, model->lambda);
    gsl_vector_memcpy(dTheta,Theta);
    gsl_blas_dgemv(CblasTrans, 1, model->X, hypothesis_y, model->lambda, dTheta);
    gsl_vector_free(hypothesis_y);
}

lm new_linear_model(const unsigned n, const double lambda){
    lm model = malloc(sizeof(*model));
    model->lambda = lambda;
    model->y = 0;
    model->n = n;
    model->Theta = gsl_vector_alloc(n);
    model->s = gsl_multimin_fdfminimizer_alloc(gsl_multimin_fdfminimizer_vector_bfgs2, n);
    gsl_vector_set_zero(model->Theta);
    return model;
}

void delete_linear_model(lm model){
    gsl_vector_free(model->Theta);
    gsl_multimin_fdfminimizer_free(model->s);
    free(model);
}

void linear_model_fit(lm model, const gsl_matrix * X, const gsl_vector * y){
    int status = GSL_CONTINUE, iter;
    gsl_multimin_function_fdf objective = {lsq, dlsq, lsqdlsq, model->n, model};
    model->X = X; model->y = y;

    gsl_multimin_fdfminimizer_set(model->s, &objective, model->Theta, 0.01, 1e-4);
    iter = MAX_ITER;
    while(iter--&& status == GSL_CONTINUE){
	status = gsl_multimin_fdfminimizer_iterate(model->s);
	if(status){break;}
	status = gsl_multimin_test_gradient(model->s->gradient, 1e-3);
	if (status == GSL_SUCCESS){
	    break;
	}
    }
    gsl_vector_memcpy(model->Theta, gsl_multimin_fdfminimizer_x(model->s));
}

inline double linear_model_predict(const lm model, const gsl_vector * x){
    return h(x, model->Theta);
}

double linear_model_xvalid(const lm model, const gsl_matrix * X_valid, const gsl_vector * y){
    gsl_vector * y_valid = gsl_vector_alloc(y->size);
    gsl_vector_memcpy(y_valid,y);    
    return h_y2(X_valid, model->Theta, y_valid);
    gsl_vector_free(y_valid);
}

