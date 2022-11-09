
/*
 * CS-252
 * shell.y: parser for shell
 *
 * This parser compiles the following grammar:
 *
 *	cmd [arg]* [> filename]
 *
 * you must extend it to understand the complete shell grammar
 *
 * Goal:
 * cmd [arg]* [ | cmd [arg]* ]* [ [> filename] [< filename] [2> filename] 
 * [ >& filename] [>> filename] [>>& filename] ]* [&]
 */

%code requires 
{
#include <string>

#if __cplusplus > 199711L
#define register      // Deprecated in C++11 so remove the keyword
#endif
}

%union
{
  char        *string_val;
  // Example of using a c++ type in yacc
  std::string *cpp_string;
}

%token <cpp_string> WORD SUBSHELL
%token NOTOKEN GREAT NEWLINE GREATGREAT LESS TWOGREAT GREATAMP GREATGREATAMP AMP PIPE

%{
//#define yylex yylex
#include <cstdio>
#include <cstring>

#include "shell.hh"

void yyerror(const char * s);
int yylex();

%}

%%

goal: //Entire Shell
  commands
  ;

commands: //All Commands entered into shell
  command
  | commands command
  ;

command:	//Each command is one line of inputs that ends in \n
  pipe_list NEWLINE {
    Shell::_currentCommand.execute();
  }
  | NEWLINE {
    Shell::prompt();
  }

  | error NEWLINE { yyerror; }
  ;

pipe_list: //List of all simple commands with a potential PIPE between them
  pipe_list simple_command 
  | pipe_list simple_command PIPE 
  | //Can be empty
  ;

simple_command: //Simple commands
  command_and_args iomodifier_list
  | command_and_args iomodifier_list AMP {
    Shell::_currentCommand._background = true;
  }
  ;

command_and_args: //Command word and the arguments and flags that follow
  command_word argument_list {
    Shell::_currentCommand.
    insertSimpleCommand( Command::_currentSimpleCommand );
  }
  ;

argument_list: //All arguments and flags
  argument_list argument
  | //Can be empty
  ;

argument: //Represents either an argument or a flag
  WORD {    
    //Check for wildcard
    expandWildcards($1);
    delete($1);
  }
  ;

command_word: //Represents the name of the command
  WORD {
    if (!strcmp($1->c_str(), "exit")) {
      printf("\n\tGood bye!!\n\n");
      free_path();
      exit(0);
    }

    if (!strcmp($1->c_str(), "quit")) {
      free_path();
      exit(0);
    }
    Command::_currentSimpleCommand = new SimpleCommand();
    Command::_currentSimpleCommand->insertArgument( $1 );
  }
  ;


iomodifier_list: //All iomodifiers 
  iomodifier_list iomodifier_opt 
  | //Can be empty
  ;

iomodifier_opt: //Represents a single iomodifier 
  GREAT WORD { //"> filename"
    if (Shell::_currentCommand._outFile) {
      yyerror("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._outFile = $2;
    }
  }
  | GREATGREAT WORD { //">> filename"
    if (Shell::_currentCommand._outFile) {
      yyerror("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand.append_out = true;
    }
  }
  | GREATAMP WORD { //">& filename"
    if (Shell::_currentCommand._errFile || Shell::_currentCommand._outFile) {
      yyerror("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
    }
  } 
  | GREATGREATAMP WORD { //">>& filename"
    if (Shell::_currentCommand._errFile || Shell::_currentCommand._outFile) {
      yyerror("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._outFile = $2;
      Shell::_currentCommand._errFile = $2;
      Shell::_currentCommand.append_out = true;
      Shell::_currentCommand.append_err = true;

    }
  }
  | TWOGREAT WORD { //"2> filename"
    if (Shell::_currentCommand._errFile) {
      yyerror("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._errFile = $2;
    }
  }
  | LESS WORD { //"< filename/command"
    if (Shell::_currentCommand._inFile) {
      yyerror("Ambiguous output redirect.\n");
    } else {
      Shell::_currentCommand._inFile = $2;
    }
  }
  ;

%%

void
yyerror(const char * s)
{
  fprintf(stderr,"%s", s);
}

#if 0
main()
{
  yyparse();
}
#endif
