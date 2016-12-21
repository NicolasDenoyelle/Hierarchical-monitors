%{
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>     
#include <float.h>
#include <dlfcn.h>
#include "./hmon.h"
#include "./hmon/hmonitor.h"
#include "./hmon/harray.h"
#include "./internal.h"

  /**
   * %test monitor
   * fake{
   * OBJ:=PU;
   * PERF_LIB:=fake;
   * EVSET:=FAKE_MONITOR;
   * WINDOW:=1;
   * %syntax: output0=input1 OP inputk... , output1=...
   * %REDUCTION:=$0=$1/2+$0/2, $1=$1*$0/2;
   * %this reduction output the variance of events. Syntax: number_of_output#function
   * REDUCTION:=1#monitor_evset_var;
   * }
   **/

  static hwloc_obj_t root;

  /* Default fields */
  const char * default_perf_lib = "fake"; 

  /* User fields */
  char *                     perf_plugin_name;
  char *                     reduction_plugin_name;
  char *                     reduction_code;
  char *                     code;
  int                        silent;
  int                        display;
  unsigned                   window;
  unsigned                   location_depth;
  harray                     events;
  harray                     reductions;

  /* This function is called for each newly parsed monitor */
  static void reset_monitor_fields(){
    if(perf_plugin_name){free(perf_plugin_name);}
    if(reduction_code){free(reduction_code);}
    if(reduction_plugin_name){free(reduction_plugin_name);}
    empty_harray(events);
    empty_harray(reductions);
    window                 = 1;        /* default store 1 sample */
    silent                 = 0;        /* default not silent */
    display                = 0;        /* default do not display */     
    location_depth         = 0;        /* default on root */
    perf_plugin_name       = NULL;
    reduction_plugin_name  = NULL;
    reduction_code         = NULL;
  }

  /* This function is called once before parsing */
  static void import_init(){
    events = new_harray(sizeof(char*), 16, free);
    reductions = new_harray(sizeof(char*), 16, free);
    reset_monitor_fields();
  }

  /* This function is called once after parsing */
  static void import_finalize(){
    /* cleanup */
    if(reduction_code){free(reduction_code);}
    if(reduction_plugin_name){free(reduction_plugin_name);}
    delete_harray(reductions);
    delete_harray(events);
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

  static char * concat_and_replace(int del, int n, ...){
    int i;
    va_list ap;
    size_t c = 0, size = 0;
    va_start(ap,n);
    for(i=0;i<n;i++){size += 1+strlen(va_arg(ap, char*));}
    char * ret = malloc(size); memset(ret,0,size);
    va_start(ap,n);
    for(i=0;i<n;i++){
      char * str = va_arg(ap, char*);
      c += sprintf(ret+c, "%s", str);
      if(i==del){free(str);}
    }
    return ret;      
  }
    
  /* Finalize monitor creation */
  static void monitor_create(char * id){
    hwloc_obj_t obj = NULL;
    char * model_plugin = reduction_plugin_name;

    /* Build reduction on events */
    if(reduction_code != NULL){
      reduction_code = concat_and_replace(3,4, "\nvoid ", id, "(hmon m){\n", reduction_code);
      reduction_code = concat_and_replace(1,2, "#include <hmon/hmonitor.h>\n\n", reduction_code);
      hmon_stat_plugin_build(id, reduction_code);
      model_plugin = id;
    }
	
    /* Build one monitor per location */
    if(harray_length(events) == 0){
      monitor_print_err("Monitor %s has no event defined and will not be buit.\n", id);
      goto end_create;
    }

    /* Collect input and output names  */
    char ** event_names = harray_to_char(events);
    char ** reduction_names = NULL;
    if(harray_length(reductions) > 0){reduction_names = harray_to_char(reductions);}
      
    while((obj = hwloc_get_next_obj_inside_cpuset_by_depth(hmon_topology, root->cpuset, location_depth, obj)) != NULL){
      hmon m = new_hmonitor(id,
			    obj,
			    (const char **)event_names,
			    harray_length(events),
			    window,
			    (const char **)reduction_names,
			    harray_length(reductions),
			    perf_plugin_name,
			    model_plugin);
      
      if(m!=NULL){if(hmon_register_hmonitor(m, silent, display) == -1){delete_hmonitor(m);}}	  
    }

    free(event_names);
    if(reduction_names != NULL){free(reduction_names);}
    
  end_create:
    reset_monitor_fields();
  }

  extern char yytext[];
  extern FILE *yyin;
  extern int column;
  extern int yylineno;
  
  int yyerror(const char *s) {
    monitor_print_err("\n%d:%d: %s while scanning input file\n", yylineno, column, s);
    exit(EXIT_FAILURE);
  }

  extern int yylex();
  %}

%error-verbose
%token <str> OBJ_FIELD EVSET_FIELD PERF_LIB_FIELD REDUCTION_FIELD WINDOW_FIELD SILENT_FIELD DISPLAY_FIELD INTEGER REAL NAME PATH VAR PERF_CTR NET_CTR

%type <str> term associative_expr commutative_expr associative_op commutative_op event 

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
: NAME '{' field_list '}' {
  if(reduction_plugin_name == NULL && reduction_code != NULL){
    reduction_plugin_name = $1;
  }
  monitor_create($1);
 }
;

field_list
: field
| field field_list
;

field
: OBJ_FIELD NAME ';'{
  hwloc_obj_t obj = location_parse(hmon_topology, $2);
  if(obj == NULL) perror_EXIT("Wrong monitor obj.\n");
  location_depth = obj->depth;
  free($2);
 }
| REDUCTION_FIELD  reduction ';'
| PERF_LIB_FIELD   NAME      ';' {perf_plugin_name = $2;}
| SILENT_FIELD     INTEGER   ';' {silent = atoi($2); free($2);}
| DISPLAY_FIELD    INTEGER   ';' {display = atoi($2); free($2);}
| WINDOW_FIELD     INTEGER   ';' {window = atoi($2); free($2);}
| EVSET_FIELD event_list     ';' {}
;

event_list
: event                {
  harray_push(events, $1);
 }
| event_list ',' event {
  harray_push(events, $3);
  }
;

event
: NAME  {$$=$1;}
| PERF_CTR {$$=$1;}
| NET_CTR {$$=$1;}
;

reduction
: INTEGER '#' NAME {
  int i;
  reduction_plugin_name = $3;
  /* Default names */
  for(i=0; i<atoi($1); i++){
    char * name = malloc(16);
    memset(name, 0, 16);
    snprintf(name, 16, "V%d", atoi($1));
    harray_push(reductions, name);
  }
  free($1);
  free($3);
 }
| assignement_list{
  reduction_code = concat_and_replace(1,2,"double * events  = hmonitor_get_events(m, m->last);\n", reduction_code);
  reduction_code = concat_and_replace(0,2, reduction_code, "}\n");
  }
;

assignement_list
: assignement
| assignement ',' assignement_list
;

assignement
: NAME '=' commutative_expr {
  if(reduction_code==NULL){reduction_code=strdup("");}
  char out[128]; memset(out,0,sizeof(out));
  snprintf(out, sizeof(out), "m->samples[%d]=", harray_length(reductions));
  reduction_code = concat_and_replace(0, 4, reduction_code, out, $3, ";\n");
  free($3);
  harray_push(reductions, $1);  
 }
;

associative_op
: '+' {$$=strdup("+");}
| '-' {$$=strdup("-");}
;

commutative_op
: '*' {$$=strdup("*");}
| '/' {$$=strdup("/");}
;


associative_expr
: term associative_op term {$$=concat_expr(3, $1,$2, $3); free($1); free($2); free($3);}
| associative_expr associative_op associative_expr {$$=concat_expr(3, $1,$2, $3); free($1); free($2); free($3);}
| term                     {$$=$1;}
;

commutative_expr
: associative_expr                                 {$$=$1;}
| associative_expr commutative_op associative_expr {$$=concat_expr(3, $1,$2, $3); free($1); free($2); free($3);}
| commutative_expr commutative_op commutative_expr {$$=concat_expr(3, $1,$2, $3); free($1); free($2); free($3);}
;

term
: VAR                      {$$ = $1;}
| INTEGER                  {$$ = $1;}
| REAL                     {$$ = $1;}
| '(' commutative_expr ')' {$$ = concat_expr(3,"(",$2,")"); free($2);}
;

%%

int hmon_import(const char * input_path, const hwloc_cpuset_t domain)
{
  if(domain == NULL) root = hwloc_get_root_obj(hmon_topology);
  else root = hwloc_get_obj_covering_cpuset(hmon_topology, domain);
  
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
      fprintf (stderr, "Cannot open %s\n", input_path);
      perror("fopen");
      return -1;
    }
  }
  else {
    fprintf (stderr, "error: invalid input file name\n");
    return -1;
  }
  return 0;
}

