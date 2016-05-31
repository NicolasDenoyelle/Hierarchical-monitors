%{
#include <stdio.h>
#include "parser.h"
void count();
%}

%option yylineno
%option nounput
%option noinput

integer           [[:digit:]]+
name              [[:alpha:]][[\_\-[:alnum:]]*|[[\_\-[:alnum:]]*[[:alpha:]]
real              \-?{integer}(\.{integer})?([eE][\+\-]{integer})? 
var               \${integer}
hexa              "0x"[0-9A-Fa-f]+
affectation       {name}={name}|{name}={integer}|{name}={hexa}
attribute         (":"{name})+|"["{affectation}(","{affectation})*"]"
file              ([\.]?{name})+
path              [~]?([/]{file})+


%{
#include <stdio.h>
#include "parser.h"
void count();
%}
%option yylineno

%%

"-"                { count(); /* fprintf(stderr,"-\n"); */                     return('-'); };
"+"                { count(); /* fprintf(stderr,"+\n"); */                     return('+'); };
"*"                { count(); /* fprintf(stderr,"*\n"); */                     return('*'); };
"/"                { count(); /* fprintf(stderr,"/\n"); */                     return('/'); };
"}"                { count(); /* fprintf(stderr,"}\n"); */                     return('}'); };
"{"                { count(); /* fprintf(stderr,"{\n"); */                     return('{'); };
";"                { count(); /* fprintf(stderr,";\n"); */                     return(';'); };
","                { count(); /* fprintf(stderr,",\n"); */                     return(',');};
"."                { count(); /* fprintf(stderr,".\n"); */                     return('.');};
"OBJ:="            { count(); /* fprintf(stderr,"OBJ_FIELD\n"); */             return(OBJ_FIELD);};
"EVSET:="          { count(); /* fprintf(stderr,"EVSET_FIELD\n"); */           return(EVSET_FIELD);};
"MAX:="            { count(); /* fprintf(stderr,"MAX_FIELD\n"); */             return(MAX_FIELD);};
"MIN:="            { count(); /* fprintf(stderr,"MIN_FIELD\n"); */             return(MIN_FIELD);};
"PERF_LIB:="       { count(); /* fprintf(stderr,"PERF_FIELD\n"); */            return(PERF_LIB_FIELD);};
"EVSET_REDUCE:="   { count(); /* fprintf(stderr,"EVSET_REDUCE_FIELD\n"); */    return(EVSET_REDUCE_FIELD);};
"SAMPLES_REDUCE:=" { count(); /* fprintf(stderr,"SAMPLES_REDUCE_FIELD\n"); */  return(SAMPLES_REDUCE_FIELD);};
"N_SAMPLES:="      { count(); /* fprintf(stderr,"N_SAMPLES_FIELD\n"); */       return(N_SAMPLE_FIELD);};
"ACCUMULATE:="     { count(); /* fprintf(stderr,"ACCUMULATE_FIELD\n"); */      return(ACCUMULATE_FIELD);};
"SILENT:="         { count(); /* fprintf(stderr,"SILENT_FIELD\n"); */          return(SILENT_FIELD);};
{name}             { count(); /* fprintf(stderr,"NAME:%s\n", yytext); */      yylval.str = strdup(yytext); return(NAME);};
{attribute}        { count(); /* fprintf(stderr,"ATTRIBUTE:%s\n", yytext); */ yylval.str = strdup(yytext); return(ATTRIBUTE);};
{var}              { count(); /* fprintf(stderr,"VAR:%s\n", yytext); */       yylval.str = strdup(yytext); return(VAR); };
{path}             { count(); /* fprintf(stderr,"PATH:%s\n", yytext); */      yylval.str = strdup(yytext); return(PATH);};
{integer}          { count(); /* fprintf(stderr,"INTEGER:%s\n", yytext); */   yylval.str = strdup(yytext); return(INTEGER);};
{real}             { count(); /* fprintf(stderr,"REAL:%s\n", yytext); */      yylval.str = strdup(yytext); return(REAL);};
\n                 { count();}
#.*                { count();}
x.                 { count();}

%%

int yywrap() {
return 1;
}
int column = 0;
void count() {
   int i;
   for (i = 0; yytext[i] != '\0'; i++) {
      if (yytext[i] == '\n')
         column = 0;
      else if (yytext[i] == '\t')
         column += 8 - (column % 8);
      else
         column++;
   }
}

