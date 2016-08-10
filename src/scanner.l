%{
#include <stdio.h>
#include "parser.h"
void count();
%}

%option yylineno
%option nounput
%option noinput

integer           \-?[[:digit:]]+
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
")"                { count(); /* fprintf(stderr,")\n"); */                     return(')'); };
"("                { count(); /* fprintf(stderr,"(\n"); */                     return('('); };
";"                { count(); /* fprintf(stderr,";\n"); */                     return(';'); };
","                { count(); /* fprintf(stderr,",\n"); */                     return(',');};
"."                { count(); /* fprintf(stderr,".\n"); */                     return('.');};
"#"                { count(); /* fprintf(stderr,":\n"); */                     return('#');};
"="                { count(); /* fprintf(stderr,"=\n"); */                     return('=');};
"OBJ:="            { count(); /* fprintf(stderr,"OBJ_FIELD\n"); */             return(OBJ_FIELD);};
"EVSET:="          { count(); /* fprintf(stderr,"EVSET_FIELD\n"); */           return(EVSET_FIELD);};
"PERF_LIB:="       { count(); /* fprintf(stderr,"PERF_FIELD\n"); */            return(PERF_LIB_FIELD);};
"REDUCTION:="      { count(); /* fprintf(stderr,"REDUCTION_FIELD\n"); */       return(REDUCTION_FIELD);};
"WINDOW:="         { count(); /* fprintf(stderr,"WINDOW_FIELD\n"); */          return(WINDOW_FIELD);};
"SILENT:="         { count(); /* fprintf(stderr,"SILENT_FIELD\n"); */          return(SILENT_FIELD);};
{name}             { count(); /* fprintf(stderr,"NAME:%s\n", yytext); */      yylval.str = strdup(yytext); return(NAME);};
{attribute}        { count(); /* fprintf(stderr,"ATTRIBUTE:%s\n", yytext); */ yylval.str = strdup(yytext); return(ATTRIBUTE);};
{var}              { count(); /* fprintf(stderr,"VAR:%s\n", yytext); */       yylval.str = strdup(yytext); return(VAR); };
{path}             { count(); /* fprintf(stderr,"PATH:%s\n", yytext); */      yylval.str = strdup(yytext); return(PATH);};
{integer}          { count(); /* fprintf(stderr,"INTEGER:%s\n", yytext); */   yylval.str = strdup(yytext); return(INTEGER);};
{real}             { count(); /* fprintf(stderr,"REAL:%s\n", yytext); */      yylval.str = strdup(yytext); return(REAL);};
\n                 { count();}
%.*                { count();}
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

