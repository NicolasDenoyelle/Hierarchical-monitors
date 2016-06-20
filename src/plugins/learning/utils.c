#include <stdlib.h>

double * Unit(const int n){
    double * one = malloc(sizeof(*one)*n);
    for(int i=0; i<n; i++){
	one[i] = 1;
    }
    return one;
}

