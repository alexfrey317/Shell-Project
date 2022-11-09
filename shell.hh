#ifndef shell_hh
#define shell_hh

#include "command.hh"
#include "y.tab.hh"

struct Shell {

  static void prompt();
  static char* path;
  static int return_val;
  static int back_pid;
  static std::string last_command;
  static bool quotes;


  static Command _currentCommand;
};


int yyparse(void);
void myunputc(int);
void source(const char*);
const char* home_exp();
void tilde_exp(char*);
void expandWildcards(std::string*);
void subShell(char *);
void env_exp(char*);
void free_path();


#endif
