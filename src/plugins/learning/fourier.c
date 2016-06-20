/**
 * This file learns an event as a fourier serie function.
 **/

#include <hmon.h>
#include <string.h>
#include <math.h>
#include "learning.h"

#define PI          3.14159265
#define N           64      /* Harmonics */
#define PHI         0.0     /* Phase     */
#define T           1.0     /* Period    */

struct brain{
    struct perceptron * p;
    double *            X; /* Features (n*m) */
    unsigned            c; /* Index of current features */
    double *            Y; /* Goal (m) */
    double *            S; /* Unit vector or condition number of X */
    unsigned            n; /* Number of features */
    unsigned            m; /* Number of samples */
    int      set_features;
};

/**
 * Create a perceptron with n features and n samples.
 * @param m: the number of samples.
 * @param n: the number of features.
 * @return a brain holding a perceptron to learn, a matrix of features (period, fourier factors), goal vector copy.
 **/
struct brain * new_brain(const int m, const int n, const double max, const double min){
    struct brain * b = malloc(sizeof(*b));
    b->p = new_perceptron(n, max, min);
    b->X = malloc(sizeof(double) * (n) * m);
    b->m = m;
    b->n = n;
    b->Y = malloc(sizeof(double) * m);
    b->S = Unit(m);
    b->c = m-1;
    b->set_features = 1;
    return b;
}


static inline double * current_features(struct brain * b){
    return &(b->X[b->c*b->m]);
}

static inline double * next_features(struct brain * b){
    b->c = (b->c+1)%b->m;
    return current_features(b);
}

static void fourier_features(struct brain * b, long t){
    unsigned       n = b->n;
    double *       X = next_features(b);
    
    /* Compute factor 0,1 */
    X[0] = 1.0;
    /* Compute cosinus factor */
    for(unsigned i=1; i<n; i++)
	X[i] = cos(2*PI*i*t/T + PHI);
}

/**
 * Predict next sample by fitting a fourier serie.
 * Function to be used as field SAMPLES_REDUCE in monitor definition.
 **/
double fourier_fit(struct monitor * hmon){
    unsigned       c = hmon->current;
    unsigned       m = hmon->n_samples;
    const int      n = N+1;

    /* store data structure in monitor */
    if(hmon->userdata == NULL){
	hmon->userdata = new_brain(m, n, 1, -1);
    }
    
    struct brain * b = hmon->userdata;
    double pred, val = hmon->samples[c];

    /* Set features */
    if(b->set_features){
	fourier_features(b, hmon->timestamps[c]);
    }
    
    /* Train online */
    perceptron_fit_by_gradiant_descent(b->p, current_features(b), &val, 1);

    /* Train only for a significant amount of samples */
    if(hmon->current == m-1){
	b->set_features = 0;
    	/* memcpy(b->Y, hmon->samples, m * sizeof(double)); */
    	/* perceptron_fit_by_gradiant_descent(b->p, b->X, b->Y, m); */
    }
    
    /* Predict */
    pred = perceptron_output(b->p, current_features(b));
    val = hmon->samples[c];
    /* Output relative error */
    return 100*fabs(pred-val)/fabs(pred+val);
}

