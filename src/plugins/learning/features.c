#include <string.h>
#include "learning.h"

struct scaling gsl_vector_normalize(gsl_vector * v){
    struct scaling scale = {gsl_vector_max(v) - gsl_vector_min(v), gsl_stats_mean(v->data, v->stride, v->size)};
    gsl_vector_add_constant(v, -scale.center);
    if(scale.scale!=0){gsl_vector_scale(v,1/scale.scale);}
    return scale;
}

void gsl_matrix_normalize_columns(gsl_matrix * mat, struct scaling * scales){
    if(scales == NULL){
	for(unsigned i =0; i<mat->size2; i++){
	    gsl_vector_view col = gsl_matrix_column (mat, i);
	    gsl_vector_normalize(&col.vector);
	}
    } else {
	for(unsigned i =0; i<mat->size2; i++){
	    gsl_vector_view col = gsl_matrix_column (mat, i);
	    scales[i] = gsl_vector_normalize(&col.vector);
	}
    }
}

void gsl_matrix_normalize_rows(gsl_matrix * mat, struct scaling * scales){
    if(scales == NULL){
	for(unsigned i =0; i<mat->size2; i++){
	    gsl_vector_view row = gsl_matrix_row(mat, i);
	    gsl_vector_normalize(&row.vector);
	}
    } else {
	for(unsigned i =0; i<mat->size2; i++){
	    gsl_vector_view row = gsl_matrix_row(mat, i);
	    scales[i] = gsl_vector_normalize(&row.vector);
	}
    }
}

gsl_matrix to_gsl_matrix(double * values, const unsigned m, const unsigned n){
    gsl_matrix mat = {m,n,n,values,NULL,0};
    return mat;
}

gsl_matrix * gsl_matrix_dup(const gsl_matrix * m){
    gsl_matrix * copy = gsl_matrix_alloc(m->size1, m->size2);
    gsl_matrix_memcpy(copy,m);
    return copy;
}

gsl_vector to_gsl_vector(double * values, const unsigned n){
    gsl_vector v = {n, 1, values, NULL, 0};
    return v;
}

gsl_vector * gsl_vector_dup(const gsl_vector * v){
    gsl_vector * copy = gsl_vector_alloc(v->size);
    gsl_vector_memcpy(copy,v);
    return copy;
}

