/**
 * This file learns an event as a fourier serie function.
 **/

#include <hmon.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "learning.h"

#define N                   32      /* Features-1: harmonics, polynomial ... */
#define FUTURE              16      /* Timesteps in the future */
#define ERROR_THRESHOLD      1      /* Learn if prevision error is greater than this */
#define LEARN_THRESHOLD   0.01      /* Descent gradiant while cost improvement is greater than this */

struct brain{
    struct perceptron * p;
    double *            X; /* Features (m*n) */
    double *            Xf; /* Future features (n) */
    double *            Y; /* Goal (m) */
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
static struct brain * new_brain(const int m, const int n, const double max, const double min){
    struct brain * b = malloc(sizeof(*b));
    b->p = new_perceptron(n, max, min);
    b->X = malloc(sizeof(double)*n*m);
    b->Xf = malloc(sizeof(double)*n);
    b->Y = malloc(sizeof(double)*m);
    b->m = m;
    b->n = n;
    b->set_features = 1;
    b->future = malloc(sizeof(double) * FUTURE);
    for(int i = 0; i<FUTURE; i++)
	b->future[i] = 0.0;
    b->c = 0;
    return b;
}

#define PI          3.14159265
#define PHI         0.0     /* Phase     */
#define T           1.0     /* Period    */

static void set_fourier_features(double *X, const unsigned n, const long t){
    X[0] = 1.0;
    for(unsigned i=1; i<n; i++)
	X[i] = cos(2*PI*i*t/T); /* cos(2*PI*i*t/T + PHI) but T=1 and PHI=0*/
}

static void set_polynomial_features(double *X, const unsigned n, const long t){
    X[0] = 1.0;
    for(unsigned i=1; i<n; i++)
	X[i] = pow(t,i);
}

static double fit(struct monitor * hmon, void (* set_features)(double *, const unsigned, const long)){
    const unsigned       c = hmon->current;
    const unsigned       m = hmon->n_samples;
    const int            n = N+1;
    const long           t = hmon->total;
    double               j = DBL_MAX, J;
    /* store data structure in monitor */
    if(hmon->userdata == NULL){
	hmon->userdata = new_brain(m, n, 1, -1);
    }
    
    struct brain            * b = hmon->userdata;
    struct perceptron       * p = b->p;
    double *                  X = b->X;
    double *                  Y = b->Y;
    double error, present, past, goal;

    set_features(&(X[c*n]), n, t);

    /* Predict FUTURE timesteps ahead */
    present         = fabs(perceptron_output(p, &(X[c*n])));
    goal            = fabs(hmon->samples[c]);
    error           = 100*fabs(present-goal)/(present+goal);

    /* Train online if error is too great */
    if(error > ERROR_THRESHOLD){
	unsigned iter = 0;
	if(hmon->total > hmon->n_samples){
	    do{
		J = j;
		memcpy(Y, hmon->samples, m*sizeof(*Y));
		j = perceptron_fit_by_gradiant_descent(p, X, Y, m);
		iter++;
	    } while( fabs(J-j) > LEARN_THRESHOLD );
	}
	else{
	    do{
		J = j;
		goal = hmon->samples[c];
		j = perceptron_fit_by_gradiant_descent(p, &(X[c*n]), &goal, 1);
		iter++;
	    } while( fabs(J-j) > LEARN_THRESHOLD );
	}
	/* printf("Converged in %u iterations\n", iter); */
    }
    
    set_features(b->Xf, n, t+FUTURE);
    b->future[b->c] = perceptron_output(p, b->Xf);
    b->c = (b->c+1)%FUTURE;
    past = fabs(b->future[b->c]);
    error = 100*fabs(past-goal)/(past+goal);

    /* Output Future relative error */
    return error;
}

/**
 * Predict next sample by fitting a fourier serie.
 * Function to be used as field SAMPLES_REDUCE in monitor definition.
 **/
double fourier_fit(struct monitor * hmon){return fit(hmon, set_fourier_features);}

/**
 * Predict next sample by fitting a polynom serie.
 * Function to be used as field SAMPLES_REDUCE in monitor definition.
 **/
double polynomial_fit(struct monitor * hmon){return fit(hmon, set_polynomial_features);}

