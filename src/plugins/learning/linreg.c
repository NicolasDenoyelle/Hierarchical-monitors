#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <float.h>
#include "learning.h"

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
    gsl_vector * y = gsl_vector_dup(model->y);
    cost = h_y2_regularized(model->X, Theta, y, model->lambda);
    gsl_vector_free(y);
    return cost;
}

/** 
 * Squared cost function derivatives with regularization 
 * First element of x is considered as target y.
 **/     
void dlsq(const gsl_vector * Theta, void * params, gsl_vector * dTheta){
    lm model = (lm)params;
    gsl_vector * hypothesis_y = gsl_vector_dup(model->y);
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
    gsl_vector * hypothesis_y = gsl_vector_dup(model->y);
    *cost  = h_y2_regularized(model->X, Theta, hypothesis_y, model->lambda);
    gsl_vector_memcpy(dTheta,Theta);
    gsl_blas_dgemv(CblasTrans, 1, model->X, hypothesis_y, model->lambda, dTheta);
    gsl_vector_free(hypothesis_y);
}

lm new_linear_model(const unsigned n, const double lambda){
    lm model = malloc(sizeof(*model));
    model->lambda = lambda;
    model->s = gsl_multimin_fdfminimizer_alloc(gsl_multimin_fdfminimizer_vector_bfgs2, n);
    model->X = NULL;
    model->y = NULL;
    return model;
}

void delete_linear_model(lm model){
    gsl_multimin_fdfminimizer_free(model->s);
    free(model);
}



void linear_model_fit(lm model, const gsl_matrix * X, gsl_vector * Theta, const gsl_vector * y){
    unsigned iter = MAX_ITER;
    model->X = X; model->y = y;

    /* double cost = DBL_MAX, old_cost = 0, tol = 1.0; */
    /* const double alpha = 0.001; */
    /* gsl_vector * Theta = model->Theta; */
    /* gsl_vector * dTheta = gsl_vector_alloc(Theta->size); */
    /* while(iter-- && (cost-old_cost)*(cost-old_cost)>tol){ */
    /* 	old_cost = cost; */
    /* 	lsqdlsq (Theta, model, &cost, dTheta); */
    /* 	gsl_blas_daxpy(-alpha, dTheta, Theta); */
    /* } */
    /* gsl_vector_free(dTheta); */
    
    int status = GSL_CONTINUE;
    gsl_multimin_function_fdf objective = {lsq, dlsq, lsqdlsq, X->size2, model};
    gsl_multimin_fdfminimizer_set(model->s, &objective, Theta, 0.01, 1e-4);
    while(iter--&& status == GSL_CONTINUE){
    	status = gsl_multimin_fdfminimizer_iterate(model->s);
    	if(status){break;}
    	status = gsl_multimin_test_gradient(model->s->gradient, 1e-3);
    	if (status == GSL_SUCCESS){
    	    break;
    	}
    }
    gsl_vector_memcpy(Theta, gsl_multimin_fdfminimizer_x(model->s));

    /* gsl_multifit_linear_workspace * ws = gsl_multifit_linear_alloc(X->size1, model->n); */
    /* gsl_matrix * cov = gsl_matrix_alloc(model->n, model->n); */
    /* double chisq; */
    /* gsl_multifit_linear(X, y, model->Theta, cov, &chisq, ws); */
    /* gsl_matrix_free(cov); */
    /* gsl_multifit_linear_free(ws); */
}

double linear_model_predict(const gsl_vector * Theta, const gsl_vector * x){
    return h(x, Theta);
}

double linear_model_xvalid(const gsl_matrix * X_valid, const gsl_vector * Theta, const gsl_vector * y){
    gsl_vector * y_valid = gsl_vector_alloc(y->size);
    gsl_vector_memcpy(y_valid,y);    
    return h_y2(X_valid, Theta, y_valid);
    gsl_vector_free(y_valid);
}

