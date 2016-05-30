#include "monitor.h"

static unsigned get_term_width(){
    unsigned cols;
    char cols_str[16];
    FILE * cmd_f;
    cmd_f = popen("tput cols","r");
    if(fread(cols_str,sizeof(*cols_str),sizeof(cols_str),cmd_f) == 0)
	perror("fread");
    else if(strcmp(cols_str,""))
	cols = atoi(cols_str);
    pclose(cmd_f);
    return cols;
}

static inline void clear_term(){system("clear");}


/* 
 * No verbose
 * L2 [:::::::   ][           ][::         ][           ]
 *
 * Verbose                 
 * L2 [(236832   ][           ][(2         ][            ]
 *
 */
#define FILL_STR  "::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::"
#define UNDERLINE_STR  "_________________________________________________________________________________________________________________________________________________________________________________________________________________"

void monitor_display_depth(unsigned depth, unsigned cols, int verbose){
    hwloc_obj_t obj;
    struct monitor * monitor;
    char * obj_content;
    int nc, fill, width;
    unsigned i, n_obj;
    double val, scale;

    obj = hwloc_get_obj_by_depth(monitors_topology, depth, 0);
    n_obj = hwloc_get_nbobjs_by_depth(monitors_topology, depth);
    if(obj==NULL)
	return;

    /* Print obj type */
    printf("%-9s ", hwloc_obj_type_string(obj->type));
    width = (cols - 10 - n_obj*2) / n_obj;
    obj_content = malloc(width+1);

    for(i=0;i<n_obj;i++){
	obj = hwloc_get_obj_by_depth(monitors_topology,depth, i);	
	monitor = obj->userdata;
	memset(obj_content,0,width+1);
	if(monitor != NULL && hwloc_bitmap_intersects(monitor->location->cpuset,monitors_running_cpuset)){
	    if(monitor->max == monitor->min){
		fill=width;
	    }
	    else{
		val = monitor->value - monitor->min;
		scale = monitor->max - monitor->min;
		fill =  val * width / scale;
	    }
	    if(verbose){
		nc = snprintf(obj_content, fill, "%lf", monitor->value);
		nc = nc>fill ? fill : nc;
		snprintf(obj_content+nc, width-nc, "%.*s", fill-nc, FILL_STR);
	    }
	    else{
		snprintf(obj_content, fill, "%.*s", fill, FILL_STR);
	    }
	    printf("[%-*s]", width, obj_content);
	}
	else{
	    nc = hwloc_obj_type_snprintf(obj_content, width, obj, verbose);
	    nc = nc>width ? width : nc;
	    nc += snprintf(obj_content+nc, width-nc, "#%u",obj->os_index);
	    nc = nc>width ? width : nc;
	    printf("[%*s%s%*s]", (width-nc)/2, " ", obj_content, width-nc-((width-nc)/2), "");
	}
    }
    printf("\n");
    free(obj_content);
}

void monitor_display_all(int verbose){
    unsigned i, depth;
    unsigned term_width;
    
    term_width = get_term_width();
    depth = hwloc_topology_get_depth(monitors_topology);
    clear_term();
fflush(stdout);
printf("%.*s\n", term_width, UNDERLINE_STR);
    for(i=0; i<depth; i++){
	monitor_display_depth(i, term_width, verbose);
    }
}

