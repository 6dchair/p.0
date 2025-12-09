%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.tab.h"

int line_num = 1;
int column_num = 1;

void update_column(int length);
%}

%option noyywrap

DIGIT       [0-9]
LETTER      [a-zA-Z]
IDENT       {LETTER}({LETTER}|{DIGIT}|_)*
WHITESPACE  [ \t\r\f]+
NEWLINE     \n
COMMENT     "//"[^\n]*

%%

{COMMENT}   { update_column(yyleng); /* ignore comments */ }

">>>"       { update_column(3); return PROG_START; }
"<<<"       { update_column(3); return PROG_END; }

"int"       { update_column(3); return KW_INT; }
"p"         { update_column(1); return KW_PRINT; }

"="         { update_column(1); return '='; }
"+"         { update_column(1); return '+'; }
"-"         { update_column(1); return '-'; }
"*"         { update_column(1); return '*'; }
"/"         { update_column(1); return '/'; }
"("         { update_column(1); return '('; }
")"         { update_column(1); return ')'; }
","         { update_column(1); return ','; }
":"         { update_column(1); return ':'; }

{IDENT}     { 
              yylval.str_val = strdup(yytext);
              update_column(yyleng);
              return ID;
            }

{DIGIT}+    {
              yylval.int_val = atoi(yytext);
              update_column(yyleng);
              return NUM;
            }

\"([^"\\\n]|\\.)*\" {
              // string literal with escape sequences
              char *text = yytext;
              int len = yyleng;
              
              // remove quotes
              text[len-1] = '\0';
              text++;
              
              // process escape sequences
              char *result = malloc(len * 2);
              char *dest = result;
              char *src = text;
              
              while(*src) {
                  if(*src == '\\') {
                      src++;
                      switch (*src) {
                          case 'n': *dest++ = '\n'; break;
                          case 't': *dest++ = '\t'; break;
                          case '"': *dest++ = '"'; break;
                          case '\\': *dest++ = '\\'; break;
                          default: *dest++ = '\\'; *dest++ = *src; break;
                      }
                      src++;
                  } else {
                      *dest++ = *src++;
                  }
              }
              *dest = '\0';
              
              yylval.str_val = result;
              update_column(yyleng);
              return STR;
            }

{WHITESPACE} { update_column(yyleng); }

{NEWLINE}   { line_num++; column_num = 1; return NEWLINE_TOKEN; }

.           { 
              fprintf(stderr, "Lexical error at line %d, column %d: Unexpected character '%c'\n", 
                      line_num, column_num, yytext[0]);
              update_column(1);
              return ILLEGAL;
            }

%%

void update_column(int length) {
    column_num += length;
}
