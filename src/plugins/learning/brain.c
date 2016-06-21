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
#define FUTURE      1       /* Timesteps in the future */

struct brain{
    struct perceptron * p;
    double *            X; /* Features (m*n) */
    unsigned            n; /* Number of features */
    unsigned            m; /* Number of samples */
    double *       future; /* array of prediction in the future */
    unsigned            c; /* Index of current prediction */
    int      set_features; /* Boolean telling if it is necessary to set the features again */
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
    b->set_features = 1;
    b->future = malloc(sizeof(double) * FUTURE);
    for(int i = 0; i<FUTURE; i++)
	b->future[i] = 0.0;
    b->c = 0;
    return b;
}


static void set_fourier_features(double * X, unsigned n, long t){
    X[0] = 1.0;
    for(unsigned i=1; i<n; i++)
	X[i] = cos(2*PI*i*t); /* cos(2*PI*i*t/T + PHI) but T=1 and PHI=0*/
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
    
    struct brain      * b = hmon->userdata;
    struct perceptron * p = b->p;
    double            * X = b->X;
    double error, present, past, goal;

    /* Set features */
    if(b->set_features){set_fourier_features(&(b->X[c*n]), n, hmon->timestamps[c]);}

    /* Predict FUTURE timesteps ahead */
    b->future[b->c] = perceptron_output(p, &(X[((c+FUTURE)%m)*n]));
    present         = perceptron_output(p, &(X[c*n]));
    b->c            = (b->c+1)%FUTURE;
    past            = b->future[b->c];
    goal            = hmon->samples[c];
    error           = 100*fabs(present-goal)/fabs(present+goal);
    
    /* Train only if prediction mispredict by more than 5%, and for a significant amount of samples (might be one sample)*/
    if(c == m-1 && error > 5){
	b->set_features = 0;
    	perceptron_fit_by_gradiant_descent(p, X, hmon->samples, m);
    }
    /* Output relative error */
    error = 100*fabs(past-goal)/fabs(past+goal);
    return error;
}

