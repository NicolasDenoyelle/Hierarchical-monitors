%{
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>     
#include <float.h>
#include <dlfcn.h>
#include "hmon.h"

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
     *   WINDOW:=128;                     
     *   # A stats_library defined in stats/stats_utils.h or a stats plugin or an arithmetique expression of events. Default to no reduce.
     *   SAMPLE_REDUCE:=$0/$1;
     *   # A stats_library defined in stats/stats_utils.h or a stats plugin to reduce samples in WNIDOW.
     *   WINDOW_REDUCE:=$0/$1;
     *   # Preset a maximum monitor value to keep in monitor structure. Default to DBL_MIN
     *   MAX:=0;
     *   # Preset a minimum monitor value to keep in monitor structure. Default to DBL_MAX
     *   MIN:=0;
     *   # Set if LIB should accumulate events values along time. Default to 0 (false).
     *   ACCUMULATE:=1;
     *   # Set if events should be normalized. Default to 0 (false).
     *   NORMALIZE:=1;
     *   # Set if monitor should print output. Default to 0 (prints output).
     *   SILENT:=1;
     * }
     *
     * MONITOR_REDUCE {
     *   OBJ:= L3;
     *   LIB:= hierarchical_monitor;
     *   EVSET:= PU; # The monitors on objects PU, children of each L3 are selected.
     *   SAMPLE_REDUCE:=MONITOR_EVSET_SUM; # Will sum accumulated events of children monitor on PU.
     *   WINDOW_REDUCE:=MONITOR_STAT_ID;
     * }
     *
     **/

    extern struct monitor * new_monitor(char*, hwloc_obj_t, void*, unsigned, unsigned, const char*, const char*, const char*, int, int);

    /* Default fields */
    const char * default_perf_lib = "fake"; 

    /* User fields */
    char *                     perf_plugin_name;
    char *                     evset_analysis_name;
    char *                     samples_analysis_name;
    char *                     code;
    int                        accumulate;
    int                        normalize;
    int                        silent;
    double                     max;
    double                     min;
    unsigned                   window;
    unsigned                   location_depth;
    struct hmon_array *        event_names;


    /* This function is called for each newly parsed monitor */
    static void reset_monitor_fields(){
	if(perf_plugin_name){free(perf_plugin_name);}
	if(samples_analysis_name){free(samples_analysis_name);}
	if(evset_analysis_name){free(evset_analysis_name);}
	if(code){free(code);}
	max                    = DBL_MIN;
	min                    = DBL_MAX;
	window                 = 1;        /* default store 1 sample */
	accumulate             = 0;        /* default do not accumulate */
	normalize              = 0;        /* default do not normalize */
	silent                 = 0; 	   /* default not silent */     
	location_depth         = 0; 	   /* default on root */
	perf_plugin_name       = NULL;
	evset_analysis_name    = NULL;
	samples_analysis_name  = NULL;
	code                   = NULL;
	empty_hmon_array(event_names); 
    }

    /* This function is called once before parsing */
    static void import_init(){
	event_names = new_hmon_array(sizeof(char*),8,free);
	reset_monitor_fields();
    }

    /* This function is called once after parsing */
    static void import_finalize(){
	/* cleanup */
	delete_hmon_array(event_names);
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

    static void print_avail_events(struct monitor_plugin * lib){
	char ** (* events_list)(int *) = monitor_plugin_load_fun(lib, "monitor_events_list", 1);
	if(events_list == NULL)
	    return;

	int n = 0;
	char ** avail = events_list(&n);
	printf("List of available events:\n");
	while(n--){
	    printf("\t%s\n",avail[n]);
	    free(avail[n]);
	}
	free(avail);
    }

    /* Finalize monitor creation */
    static void monitor_create(char * name){
	hwloc_obj_t obj = NULL;
	void * eventset = NULL;
	struct monitor_plugin * perf_lib = NULL;
	int    (* eventset_init)(void **, hwloc_obj_t, int);
	int    (* eventset_init_fini)(void*);
	int    (* eventset_destroy)(void*);
	int    (* add_named_event)(void*, char*);
	unsigned j, n;
	int err, added_events;
	char * reduce_code = NULL;

	/* Load dynamic library */
	if(!perf_plugin_name){perf_plugin_name = strdup(default_perf_lib);}
	perf_lib =  monitor_plugin_load(perf_plugin_name, MONITOR_PLUGIN_PERF);
	if(perf_lib == NULL){
	    fprintf(stderr, "Failed to load %s performance library.\n", name);
	    return;
	}
	eventset_init      = monitor_plugin_load_fun(perf_lib, "monitor_eventset_init",      1);
	eventset_init_fini = monitor_plugin_load_fun(perf_lib, "monitor_eventset_init_fini", 1);
	add_named_event    = monitor_plugin_load_fun(perf_lib, "monitor_eventset_add_named_event",    1);
	eventset_destroy   = monitor_plugin_load_fun(perf_lib, "monitor_eventset_destroy",   1);
	
	/* Build reduction on events */
	if(code){	    
	    reduce_code = concat_expr(6,"#include <hmon.h>\n", "double ", name, "(struct monitor * monitor){return ", code, ";}\n");
	    monitor_stat_plugin_build(name, reduce_code);
	    free(reduce_code);
	    evset_analysis_name = strdup(name);
	}
	
	/* Build one monitor per location */
	
	while((obj = hwloc_get_next_obj_by_depth(monitors_topology, location_depth, obj)) != NULL){
	    if(obj->userdata != NULL){
		char * name = location_name(obj);
		fprintf(stderr, "Can't define a monitor on obj %s, because a monitor is already defined there\n", name);
		free(name);
		continue;
	    }

	    /* Initialize this->eventset */
	    if(eventset_init(&eventset, obj, accumulate)==-1){
		monitor_print_err( "failed to initialize %s eventset\n", name);
		return;
	    }

	    /* Add events */
	    n = hmon_array_length(event_names);
	    added_events = 0;
	    for(j=0;j<n;j++){
		err = add_named_event(eventset,(char*)hmon_array_get(event_names,j));
		if(err == -1){
		    monitor_print_err("failed to add event %s to %s eventset\n", (char*)hmon_array_get(event_names,j), name);
		    print_avail_events(perf_lib);
		    exit(EXIT_FAILURE);
		}
		added_events += err;
	    }
	    
	    if(added_events==0){
		eventset_destroy(eventset);
		continue;
	    }
	
	    /* Finalize eventset initialization */
	    eventset_init_fini(eventset);

	    /* Create monitor */
	    new_monitor(name, obj, eventset, added_events, window, perf_plugin_name, evset_analysis_name, samples_analysis_name, normalize, silent);
	}
	reset_monitor_fields();
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
%token <str> OBJ_FIELD EVSET_FIELD MAX_FIELD MIN_FIELD PERF_LIB_FIELD WINDOW_REDUCE_FIELD SAMPLE_REDUCE_FIELD WINDOW_FIELD NORMALIZE_FIELD ACCUMULATE_FIELD SILENT_FIELD INTEGER REAL NAME PATH VAR ATTRIBUTE

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
    location_depth = obj->depth;
    free($2);
 }
| SAMPLE_REDUCE_FIELD  add_expr  ';' {code = $2;}
| SAMPLE_REDUCE_FIELD  NAME      ';' {evset_analysis_name = $2;}
| WINDOW_REDUCE_FIELD  NAME     ';' {samples_analysis_name = $2;}
| PERF_LIB_FIELD   NAME      ';' {perf_plugin_name = $2;}
| ACCUMULATE_FIELD INTEGER   ';' {accumulate = atoi($2); free($2);}
| NORMALIZE_FIELD INTEGER   ';' {normalize = atoi($2); free($2);}
| SILENT_FIELD     INTEGER   ';' {silent = atoi($2); free($2);}
| WINDOW_FIELD   INTEGER   ';' {window = atoi($2); free($2);}
| EVSET_FIELD event_list     ';' {}
| min_field                      {}
| max_field                      {}
;

max_field
: MAX_FIELD REAL    ';' { max = (double)atof($2); free($2);}
| MAX_FIELD INTEGER ';' { max = (double)atoi($2); free($2);}
;

min_field
: MIN_FIELD REAL    ';' { min = (double)atof($2); free($2);}
| MIN_FIELD INTEGER ';' { min = (double)atoi($2); free($2);}
;

event_list
: event                {hmon_array_push(event_names,$1);}
| event ',' event_list {hmon_array_push(event_names,$1);}
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
    char hmon_array_index[strlen($1)]; 
    memset(hmon_array_index,0,sizeof(hmon_array_index)); 
    snprintf(hmon_array_index,sizeof(hmon_array_index),"%s",$1+1);
    free($1);
    $$ = concat_expr(3,"(double)(monitor->events[monitor->current][",hmon_array_index,"])");}
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

