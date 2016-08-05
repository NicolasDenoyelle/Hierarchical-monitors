#include <string.h>
#include "hmon_utils.h"
#include "hmon.h"

hmatrix new_hmatrix(unsigned rows, unsigned cols){
    hmatrix mat;
    malloc_chk(mat.data, sizeof(double)*rows*cols);
    mat.rows = rows;
    mat.cols = cols;
    return mat;
}

hmatrix no_hmatrix(){
    hmatrix mat = {NULL, 0, 0};
    return mat;
}

int hmat_is_NULL(hmatrix mat){
    return mat.data==NULL;
}

void delete_hmatrix(hmatrix mat){
    free(mat.data);
}

double * hmat_get_data(hmatrix mat){
    return mat.data;
}

double hmat_get(const hmatrix mat, const unsigned row, const unsigned col){
    return mat.data[row*mat.cols + col];
}

double * hmat_get_row(hmatrix mat, const unsigned row){
    return &(mat.data[row*mat.cols]);
}

void hmat_set(const double value, hmatrix mat, const unsigned row, const unsigned col){
    mat.data[row*mat.cols + col] = value;
}

void hmat_set_row(const double * values, hmatrix mat, const unsigned row){
    memcpy(&(mat.data[row*mat.cols]),values, mat.cols*sizeof(double));
}

void hmat_zero(hmatrix mat){
    for(unsigned i = 0; i<mat.cols*mat.rows; i++){mat.data[i] = 0;}
}

