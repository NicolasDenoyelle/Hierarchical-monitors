#include <string.h>
#include "learning.h"

void gsl_vector_print(const gsl_vector * v){
    unsigned i;
    printf("[%lf", gsl_vector_get(v,0));
    for(i=1;i<v->size;i++){printf(", %lf", gsl_vector_get(v,i));}
    printf("]");
}

void gsl_vector_normalize(gsl_vector * v){
    double max  = gsl_vector_max(v);
    double min = gsl_vector_min(v);
    double mu = gsl_stats_mean(v->data, 1, v->size);
    gsl_vector_add_constant(v, -mu);
    if(max!=min){gsl_vector_scale(v,1/(max-min));}
}

void gsl_matrix_normalize_columns(gsl_matrix * mat){
    for(unsigned i =0; i<mat->size2; i++){
	gsl_vector_view col = gsl_matrix_column (mat, i);
	gsl_vector_normalize(&col.vector);
    }
}

void gsl_matrix_normalize_rows(gsl_matrix * mat){
    for(unsigned i =0; i<mat->size2; i++){
	gsl_vector_view row = gsl_matrix_row(mat, i);
	gsl_vector_normalize(&row.vector);
    }
}

gsl_matrix * to_gsl_matrix(const double * values, const unsigned m, const unsigned n){
    gsl_matrix * mat = gsl_matrix_alloc(m,n);
    memcpy(mat->data, values, m*n*sizeof(double));
    return mat;
}

gsl_vector * to_gsl_vector(const double * values, const unsigned n){
    gsl_vector * v = gsl_vector_alloc(n);
    memcpy(v->data, values, n*sizeof(double));
    return v;
}

