#include <stdlib.h>
#include <stdio.h>
#include "../../hmon.h"

#define N_STEPS 10

struct centroid{
    double x; /* First coordinate of centroid. */
    double y;   /* Second coordinate of centroid */
};

static double centroids_distance(struct centroid a, struct centroid b){
    a.x -= b.x;
    a.x *= a.x;
    a.y -= b.y;
    a.y *= a.y;
    return a.y+a.x;
}

static unsigned colorize(const double * samples, const double * timestamps, unsigned * colors, const unsigned m, struct centroid centroids[2]){
    unsigned i, colors_0 = 0;
    for(i=0;i<m;i++){
	struct centroid sample;
	sample.x = samples[i];
	sample.y = timestamps[i];
	double distance_0 = centroids_distance(centroids[0], sample);
	double distance_1 = centroids_distance(centroids[1], sample);
	if(distance_0<distance_1){colors[i] = 0; colors_0++;}
	else{colors[i] = 1;}
    }
    return colors_0;
}

static void kmean(const double * samples, const double * timestamps, const unsigned m, struct centroid centroids[2]){
    unsigned i;
    
    /* More centroids than sample */
    if(m<=2){
	/* Set centroids to samples */
	for(i=0; i<m; i++){
	    centroids[i].x = timestamps[i];
	    centroids[i].y = samples[i];
	}
	return;
    }
	
    /* centroids initialization */
    centroids[0].x = timestamps[0];
    centroids[0].y = samples[0];
    centroids[1].x = timestamps[m-1];
    centroids[1].y = samples[m-1];

    /* algorithm */
    unsigned * colors = malloc(sizeof(*colors)*m), n_colors[2];
    unsigned n_steps = N_STEPS;
    while(n_steps--){
	/* colorize samples */
	n_colors[0] = colorize(samples, timestamps, colors, m, centroids);
	n_colors[1] = m - n_colors[0];

	/* move centroids*/
	centroids[0].x = centroids[1].x = 0;
	centroids[0].y = centroids[1].y = 0;
	
	for(i=0;i<m;i++){
	    if(colors[i] == 0){
		centroids[0].x += timestamps[i];
		centroids[0].y += samples[i];
	    }
	    else{
		centroids[1].x += timestamps[i];
		centroids[1].y += samples[i];
	    }
	}
	if(n_colors[0] == 0){
	    centroids[0].x = 0;
	    centroids[0].y = 0;
	} else {
	    centroids[0].x/=(double)n_colors[0];
	    centroids[0].y/=(double)n_colors[0];
	}
	if(n_colors[1] == 0){
	    centroids[1].x = 0;
	    centroids[1].y = 0;
	} else {
	    centroids[1].x/=(double)n_colors[1];
	    centroids[1].y/=(double)n_colors[1];
	}
    }
    free(colors);
}


#define N_CENTROIDS 2

/**
 * Perform K-mean clustering into 2 clusters on timestamps and samples, and output timestamp boundary between clusters.
 * @param m: the monitor used for clustering
 * @return The timestamp boundary
 **/
double timestep_split(const struct monitor * m){
    struct centroid centroids[2];
    unsigned i,n = m->window>m->total ? m->total:m->window;

    /* Feature normalization */
    double * timestamps = malloc(sizeof(*timestamps)*n);
    double t0 = m->timestamps[(m->current+1)%m->window];
    double tf = m->timestamps[m->current];
    double * samples = malloc(sizeof(*samples)*n);
    for(i=0;i<n;i++){
	timestamps[i] = (m->timestamps[i]-t0)/(tf-t0);
	samples[i] = m->samples[i]/(m->max-m->min);
    }
    
    /* Set centroids */
    kmean(samples, timestamps, n, centroids);
    return (centroids[1].x+centroids[0].x - 0.5);
    
    /* De-normalize centroids */
    centroids[0].x = centroids[0].x*(tf-t0) + t0;
    centroids[1].x = centroids[1].x*(tf-t0) + t0;
    centroids[0].y = centroids[0].y*(m->max-m->min);
    centroids[1].y = centroids[1].y*(m->max-m->min);
    return (centroids[1].x+centroids[0].x - tf/2)/(tf-t0);
}


