%{
#include <stdio.h>
#include "parser.h"
void count();
%}
%option yylineno

integer           [[:digit:]]+
hexa              "0x"[0-9A-Fa-f]+
name              [[:alpha:]]+([\_\-[:alnum:]])*
affectation       {name}={name}|{name}={integer}|{name}={hexa}
attribute         ([:]+{name})+|"["{affectation}(","{affectation})*"]"
file              ([\.]?{name})+
path              [~]?([/]{file})+
real              \-?{integer}(\.{integer})?([eE][\+\-]{integer})? 
var               \${integer}


%{
#include <stdio.h>
#include "parser.h"
void count();
%}
%option yylineno

%%

"-"                { count(); return('-'); };
"+"                { count(); return('+'); };
"*"                { count(); return('*'); };
"/"                { count(); return('/'); };
"}"                { count(); return('}'); };
"{"                { count(); return('{'); };
";"                { count(); return(';'); };
"OBJ:="            { count(); return(OBJ_FIELD);        };
"EVSET:="          { count(); return(EVSET_FIELD);      };
"MAX:="            { count(); return(MAX_FIELD);        };
"MIN:="            { count(); return(MIN_FIELD);        };
"PERF_LIB:="       { count(); return(PERF_LIB_FIELD);   };
"EVSET_REDUCE:="   { count(); return(EVSET_REDUCE_FIELD);   };
"SAMPLES_REDUCE:=" { count(); return(SAMPLES_REDUCE_FIELD); };
"N_SAMPLE:="       { count(); return(N_SAMPLE_FIELD);   };
"ACCUMULATE:="     { count(); return(ACCUMULATE_FIELD); };
"SILENT:="         { count(); return(SILENT_FIELD); };
{integer}          { count(); yylval.str = strdup(yytext); return(INTEGER);  };
{real}             { count(); yylval.str = strdup(yytext); return(REAL);     };
{attribute}        { count(); yylval.str = strdup(yytext); return(ATTRIBUTE);};
{var}              { count(); yylval.str = strdup(yytext); return(VAR);      };
{path}             { count(); yylval.str = strdup(yytext);return(PATH);      };
{name}             { count(); yylval.str = strdup(yytext); return(NAME);     };
":"                { count(); return(':'); };
","                { count(); return(','); };
"."                { count(); return('.'); };
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

