%{
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>     
#include <float.h>
#include <dlfcn.h>
#include "monitor.h"
#include "performance.h"
#include "stats.h"

    /**
     * #Commentary
     *
     * MONITOR {
     *   OBJ:= PU:0..3;                     # name:logical_index where to map monitors. Can be a range of OBJ;
     *   PERF_LIB:= PAPI_monitor;           # Performance library to read counters
     *   EVSET:= PAPI_L1_DCM, PAPI_L1_DCA;  # A list of events defined by PERF_LIB
     * 
     * #Optional fields: 
     *   # The number of stored values for timestamps and aggregates. Default to 32.
     *   N_SAMPLE:=128;                     
     *   # A stats_library defined in stats/stats_utils.h or a stats plugin or an arithmetique expression of events. Default to no reduce.
     *   EVSET_REDUCE:=$0/$1;
     *   # A stats_library defined in stats/stats_utils.h or a stats plugin to reduce samples of reduced events.
     *   SAMPLES_REDUCE:=$0/$1;
     *   # Preset a maximum monitor value to keep in monitor structure. Default to DBL_MIN
     *   MAX:=0;
     *   # Preset a minimum monitor value to keep in monitor structure. Default to DBL_MAX
     *   MIN:=0;
     *   # Set if LIB should accumulate events values along time. Default to 0 (false).
     *   ACCUMULATE:=1;
     *   # Set if monitor should print output. Default to 0 (prints output).
     *   SILENT:=1;
     * }
     *
     * MONITOR_REDUCE {
     *   OBJ:= L3;
     *   LIB:= hierarchical_monitor;
     *   EVSET:= PU; # The monitors on objects PU, children of each L3 are selected.
     *   EVSET_REDUCE:=MONITOR_EVSET_SUM; # Will sum accumulated events of children monitor on PU.
     *   SAMPLES_REDUCE:=MONITOR_STAT_ID;
     * }
     *
     **/

    extern struct monitor * new_monitor(hwloc_obj_t location, void * eventset, unsigned n_events, unsigned n_samples, struct monitor_perf_lib * perf_lib, struct monitor_stats_lib * stat_evset_lib, struct monitor_stats_lib * stat_samples_lib, int silent);

    /* Default fields */
    const char *   default_perf_lib                           = "fake_perf_monitor"; 
    const double   default_max                                = DBL_MIN;
    const double   default_min                                = DBL_MAX;
    const unsigned default_n_sample                           = 32;
    const int      default_accumulate                         = 0;
    const int      default_silent                             = 0;

    /* User fields */
    char *                     monitor_perf_lib;
    char *                     monitor_stats_evset_lib;
    char *                     monitor_stats_samples_lib;
    int                        monitor_accumulate;
    int                        monitor_silent;
    double                     monitor_max;
    double                     monitor_min;
    unsigned                   monitor_n_sample;
    struct array *     monitor_location;
    struct array *     monitor_evset;
    char *                     monitor_aggregation_code;


    /* This function is called for each newly parsed monitor */
    void reset_monitor_fields(){
	if(monitor_perf_lib){free(monitor_perf_lib);}
	if(monitor_stats_samples_lib){free(monitor_stats_samples_lib);}
	if(monitor_stats_evset_lib){free(monitor_stats_evset_lib);}
	monitor_max             = default_max;
	monitor_min             = default_min;
	monitor_n_sample        = default_n_sample;
	monitor_accumulate      = 0;
	monitor_silent          = 0;
	monitor_perf_lib        = NULL;
	monitor_stats_evset_lib   = NULL;
	monitor_stats_samples_lib       = NULL;
	empty_array(monitor_evset); 
	empty_array(monitor_location);
	if(monitor_aggregation_code){free(monitor_aggregation_code);}
	monitor_aggregation_code = NULL;
    }

    /* This function is called once before parsing */
    void import_init(){
	monitor_evset = new_array(sizeof(char*),8,free);
	monitor_location = new_array(sizeof(hwloc_obj_t),32,NULL);	
	reset_monitor_fields();
    }

    /* This function is called once after parsing */
    void import_finalize(){
	/* cleanup */
	delete_array(monitor_location);
	delete_array(monitor_evset);
    }

    static void print_avail_events(struct monitor_perf_lib * lib){
	int n = 0;
	char ** avail = lib->monitor_events_list(&n);
	printf("List of available events:\n");
	while(n--){
	    printf("\t%s\n",avail[n]);
	    free(avail[n]);
	}
	free(avail);
    }

    /* Finalize monitor creation */
    void monitor_create(char * name){
	hwloc_obj_t obj;
	void * eventset;
	struct monitor_perf_lib * perf_lib;
	struct monitor_stats_lib * evset_lib = NULL;
	struct monitor_stats_lib * samples_lib = NULL;
	unsigned j, n, n_siblings = array_length(monitor_location);
	int err, added_events;

	/* Monitor default on root */
	if(n_siblings == 0){
	    n_siblings = 1;
	    array_push(monitor_location, hwloc_get_root_obj(monitors_topology));
	}

	/* Load dynamic library */
	if(!monitor_perf_lib){ monitor_perf_lib = strdup(default_perf_lib);}
	perf_lib =  monitor_load_perf_lib(monitor_perf_lib);
	if(perf_lib == NULL){
	    fprintf(stderr, "Failed to load %s performance library.\n", name);
	    exit(EXIT_FAILURE);
	}
	
	if(monitor_aggregation_code)
	    evset_lib = monitor_build_custom_stats_lib(name, monitor_aggregation_code);
	else if(monitor_stats_evset_lib)
	    evset_lib = monitor_load_stats_lib(monitor_stats_evset_lib);
	if(monitor_stats_samples_lib)
	    samples_lib = monitor_load_stats_lib(monitor_stats_samples_lib);
		
	while((obj =  array_pop(monitor_location)) != NULL){
	    if(obj->userdata != NULL){
		char * name = location_name(obj);
		fprintf(stderr, "Can't define a monitor on obj %s, because a monitor is already defined there\n", name);
		free(name);
		continue;
	    }

	    /* Initialize this->eventset */
	    if(perf_lib->monitor_eventset_init(&eventset, obj, monitor_accumulate)==-1){
		monitor_print_err( "failed to initialize %s eventset\n", name);
		return;
	    }

	    /* Add events */
	    n = array_length(monitor_evset);
	    added_events = 0;
	    for(j=0;j<n;j++){
		err = perf_lib->monitor_eventset_add_named_event(eventset,(char*)array_get(monitor_evset,j));
		if(err == -1){
		    monitor_print_err("failed to add event %s to %s eventset\n", (char*)array_get(monitor_evset,j), name);
		    print_avail_events(perf_lib);
		    exit(EXIT_FAILURE);
		}
		added_events += err;
	    }
	    
	    if(added_events==0){
		perf_lib->monitor_eventset_destroy(eventset);
		continue;
	    }
	
	    /* Finalize eventset initialization */
	    perf_lib->monitor_eventset_init_fini(eventset);

	    /* Create monitor */
	    new_monitor(obj, eventset, added_events, monitor_n_sample, perf_lib, evset_lib, samples_lib, monitor_silent);
	}
	reset_monitor_fields();
    }

static char * concat_expr(int n, ...){
    va_list ap;
    int i;
    size_t c = 0, size = 0;
    va_start(ap,n);
    for(i=0;i<n;i++)
	size += 1+strlen(va_arg(ap, char*));
    char * ret = malloc(size); memset(ret,0,size);
    va_start(ap,n);
    for(i=0;i<n;i++){
	char * str = va_arg(ap, char*);
	c += sprintf(ret+c, "%s", str);
    }
    return ret;
}

    extern char yytext[];
    extern FILE *yyin;
    extern int column;
    extern int yylineno;
  
    int yyerror(const char *s) {
	fflush(stdout);
	monitor_print_err("\n%d:%d: %s while scanning input file\n", yylineno, column, s);
	exit(EXIT_FAILURE);
    }

    extern int yylex();
    %}

%error-verbose
%token <str> OBJ_FIELD EVSET_FIELD MAX_FIELD MIN_FIELD PERF_LIB_FIELD EVSET_REDUCE_FIELD SAMPLES_REDUCE_FIELD N_SAMPLE_FIELD ACCUMULATE_FIELD SILENT_FIELD INTEGER REAL NAME PATH VAR ATTRIBUTE

%type <str> primary_expr add_expr mul_expr event

%union{
    char * str;
 }

%start monitor_list
%%

monitor_list
: monitor
| monitor monitor_list
;

monitor
: NAME '{' field_list '}' { monitor_create($1); free($1);}
;

field_list
: field
| field field_list
;

field
: OBJ_FIELD NAME ';'{
    hwloc_obj_t obj = location_parse($2);
    if(obj == NULL) perror_EXIT("Wrong monitor obj.\n");
    unsigned nbobjs = hwloc_get_nbobjs_by_type(monitors_topology, obj->type);
    while(nbobjs --){
	obj = hwloc_get_obj_by_type(monitors_topology, obj->type, nbobjs);
	array_push(monitor_location, obj);
    }
    free($2);
 }
| EVSET_REDUCE_FIELD  add_expr  ';' {monitor_aggregation_code = $2;}
| EVSET_REDUCE_FIELD  NAME      ';' {monitor_stats_evset_lib = $2;}
| SAMPLES_REDUCE_FIELD      NAME      ';' {monitor_stats_samples_lib = $2;}
| PERF_LIB_FIELD   NAME      ';' {monitor_perf_lib = $2;}
| ACCUMULATE_FIELD INTEGER   ';' {monitor_accumulate = atoi($2); free($2);}
| SILENT_FIELD     INTEGER   ';' {monitor_silent = atoi($2); free($2);}
| N_SAMPLE_FIELD   INTEGER   ';' {monitor_n_sample = atoi($2); free($2);}
| EVSET_FIELD event_list     ';' {}
| min_field                      {}
| max_field                      {}
;

max_field
: MAX_FIELD REAL    ';' { monitor_max = (double)atof($2); free($2);}
| MAX_FIELD INTEGER ';' { monitor_max = (double)atoi($2); free($2);}
;

min_field
: MIN_FIELD REAL    ';' { monitor_min = (double)atof($2); free($2);}
| MIN_FIELD INTEGER ';' { monitor_min = (double)atoi($2); free($2);}
;

event_list
: event                {array_push(monitor_evset,$1);}
| event ',' event_list {array_push(monitor_evset,$1);}
;

event
: NAME  {$$=$1;}
| NAME ATTRIBUTE {$$=concat_expr(2,$1,$2); free($1); free($2);}
;

add_expr
: mul_expr              { $$ = $1; }
| add_expr '+' mul_expr { $$ = concat_expr(3,$1,"+",$3); free($1); free($3);}
| add_expr '-' mul_expr { $$ = concat_expr(3,$1,"-",$3); free($1); free($3);}
;
   
mul_expr
: primary_expr {$$ = $1;}
| mul_expr '*' primary_expr { $$ = concat_expr(3,$1,"*",$3); free($1); free($3);}
| mul_expr '/' primary_expr { $$ = concat_expr(3,$1,"/",$3); free($1); free($3);}
;

primary_expr 
: VAR {
    char array_index[strlen($1)]; 
    memset(array_index,0,sizeof(array_index)); 
    snprintf(array_index,sizeof(array_index),"%s",$1+1);
    free($1);
    $$ = concat_expr(3,"monitor->events[monitor->current][",array_index,"]");}
| REAL    {$$ = $1;}
| INTEGER {$$ = $1;}
;

%%

int monitors_import(char * input_path)
{
    /* parsing input file and creating functions.c file */
    FILE *input = NULL;
    if (input_path!=NULL) {
	input = fopen (input_path, "r");
	if (input) {
	    import_init();
	    yyin = input;
	    yyparse();
	    fclose(input);
	    import_finalize();
	}
	else {
	    fprintf (stderr, "Could not open %s\n", input_path);
	    return -1;
	}
    }
    else {
	fprintf (stderr, "error: invalid input file name\n");
	return -1;
    }
    return 0;
}

