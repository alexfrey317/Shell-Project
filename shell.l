
/*
 *
 * CS-252 
 * shell.l: lexical analyzer for shell
 * You have to extend it.
 *
 */

%{

#include <cstring>
#include "y.tab.hh"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

#include "shell.hh"

static void yyunput (int c,char *buf_ptr  );

void myunputc(int c) {
  unput(c);
}

extern "C" char * read_line();

static char *p;

int mygetc(FILE * f) {
  char ch;

     if (!isatty(0)) {
  // stdin is not a tty. Call real getc
    return getc(f);
  }

  // stdin is a tty. Call our read_line.
  if (p==NULL || *p == 0) {
    char * s = read_line();
    p = s;
  }

  ch = *p;
  p++;

  return ch;
}

#undef getc
#define getc(f) mygetc(f)

void remove_char(char* str, int idx) {
  for (int i = idx; i < strlen(str); i++) {
    str[i] = str[i + 1];
  }
}

void remove_backslash(char* str) {
  int idx = 0;
  while(idx <= strlen(str)) {
    if (str[idx] == '\\') {
      remove_char(str, idx);
    }
    idx++;
  } 
}

void source(const char* file_name) {
  //Open file
  FILE *input = fopen(file_name, "r");

  //Check for error
  if (!input) return;
  char c = 'p';
  
  //Set up buffers and parse through
  YY_BUFFER_STATE current_state = YY_CURRENT_BUFFER;
  YY_BUFFER_STATE buffer = yy_create_buffer(input, YY_BUF_SIZE);
  yy_switch_to_buffer(buffer);
  yyparse();
  yy_switch_to_buffer(current_state);
  yy_delete_buffer(buffer);
  
  fclose(input);
}



%}

%option noyywrap

%%

\n {
  return NEWLINE;
}

[ \t] {
  /* Discard spaces and tabs */
}

[\r] {
  //Discard carriage returns
}

">" {
  return GREAT;
}

">>" {
  return GREATGREAT;
}

"<" {
  return LESS;
}

"2>" {
  return TWOGREAT;
}

">&" {
  return GREATAMP;
}

">>&" {
  return GREATGREATAMP;
}

&[0-9] {
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}

"&" {
  return AMP;
}

"|" {
  return PIPE;
}

"source "[^ \t\n><&|]+ {
  //For source
  source(yytext + 7);
}

([^ \t\n\r><&|$]*~[^ \t\n\r><&|$]*)+ {
  //Tilde expansion
  tilde_exp(yytext);
}

([^ \t\n\r><&|$]*\$\{[^\n{]*\}[^ \t\n\r><&|$]*)+ {
  //For Environment variable expansion
  env_exp(yytext);
}

\$\([^\n(]*\) {
  // For Subshell
  subShell(yytext);
}

\"[^\n\"]*\" { 
  // For Quotations
  Shell::quotes = true;
  yytext[strlen(yytext) - 1] = 0;
  yytext[0] = 0;
  yylval.cpp_string = new std::string(yytext + 1);
  return WORD;
}

([^ \t\n><&|$]*\\[^ \t\n]+)+ {
  //For backslashes
  remove_backslash(yytext);
  yylval.cpp_string = new std::string(yytext);

  return WORD;
}

[^ \t\n\r><&|$]+  {  
  //For generic words
  yylval.cpp_string = new std::string(yytext);
  return WORD;
}
