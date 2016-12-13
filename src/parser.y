%{
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>     
#include <float.h>
#include <dlfcn.h>
#include "./hmon.h"
#include "./hmon/hmonitor.h"
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
  unsigned                   n_reductions;
  char *                     code;
  int                        silent;
  int                        display;
  unsigned                   window;
  unsigned                   location_depth;
  char **                    event_names;
  unsigned                   n_events;
  unsigned                   allocated_events;

  static void realloc_chk_events(){
    if(n_events+1>allocated_events){
      allocated_events*=2;
      realloc_chk(event_names, allocated_events*sizeof(*event_names));
    }
  }
    
  /* This function is called for each newly parsed monitor */
  static void reset_monitor_fields(){
    if(perf_plugin_name){free(perf_plugin_name);}
    if(reduction_code){free(reduction_code);}
    if(reduction_plugin_name){free(reduction_plugin_name);}
    while(n_events--){free(event_names[n_events]);}
    n_events               = 0;
    window                 = 1;        /* default store 1 sample */
    silent                 = 0; 	   /* default not silent */
    display                = 0; 	   /* default do not display */     
    location_depth         = 0; 	   /* default on root */
    n_reductions           = 0;
    perf_plugin_name       = NULL;
    reduction_plugin_name  = NULL;
    reduction_code         = NULL;
  }

  /* This function is called once before parsing */
  static void import_init(){
    event_names = malloc(32*sizeof(char*));
    allocated_events = 32;
    reset_monitor_fields();
  }

  /* This function is called once after parsing */
  static void import_finalize(){
    /* cleanup */
    if(reduction_code){free(reduction_code);}
    if(reduction_plugin_name){free(reduction_plugin_name);}
    while(n_events--){free(event_names[n_events]);}
    free(event_names);
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
    while((obj = hwloc_get_next_obj_inside_cpuset_by_depth(hmon_topology, root->cpuset, location_depth, obj)) != NULL){
      hmon m = new_hmonitor(id, obj, (const char **)event_names, n_events, window, n_reductions, perf_plugin_name, model_plugin);
      if(m!=NULL){if(hmon_register_hmonitor(m, silent, display) == -1){delete_hmonitor(m);}}
	  
    }
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
  realloc_chk_events();
  event_names[n_events++] = $1;
 }
| event_list ',' event {
  realloc_chk_events();
  event_names[n_events++] = $3;
  }
;

event
: NAME  {$$=$1;}
| PERF_CTR {$$=$1;}
| NET_CTR {$$=$1;}
;

reduction
: INTEGER '#' NAME {reduction_plugin_name = $3; n_reductions = atoi($1); free($1);}
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
: commutative_expr {
  if(reduction_code==NULL){reduction_code=strdup("");}
  char out[128]; memset(out,0,sizeof(out));
  snprintf(out, sizeof(out), "m->samples[%d]=", n_reductions);
  reduction_code = concat_and_replace(0, 4, reduction_code, out, $1, ";\n");
  free($1);
  n_reductions++;
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

