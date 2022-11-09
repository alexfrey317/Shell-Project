#include <cstdio>
#include <algorithm>
#include <sstream>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <dirent.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <regex.h>

#include "shell.hh"
#include "command.hh"


int added_args;
char* path;
int Shell::return_val;
int Shell::back_pid;
std::string Shell::last_command;
bool Shell::quotes;

extern "C" void reset_buffer();

extern "C" void sigint_handler(int sig) {
  if (sig != SIGINT) return;

  int status;
  pid_t pid = waitpid(-1, &status, WNOHANG);

  //Print new line
  printf("\n");

  //Exit or continue
  if (pid == -1) {
    Shell::prompt();
  }

  reset_buffer();
}

extern "C" void sigchild_handler(int sig) {
  if (sig != SIGCHLD) return;

  int status;
  pid_t pid;
  while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (pid > 0 && isatty(0)) {
      printf("\n[%d] exited\n", pid);
    }
  }    
}

void free_path() {
  free(path);
}



void unput_chars(const char * buffer) {
  for (int i = strlen(buffer); i >= 0; i--) {
    //Character to print
    char c = buffer[i];

    //Replace new lines
    if (c == '\n') c = ' ';

    myunputc(c);
  }
}

const char* home_exp() {
  if (!(getenv("HOME"))) {
    return getpwuid(getuid())->pw_dir;
  } else {
    return getenv("HOME");
  }
}

void exp_wild(std::string prefix, std::string suffix) {
  //If Suffix is empty
  if (suffix.empty()) {
    added_args++;
    Command::_currentSimpleCommand->insertArgument(new std::string(prefix));
    return;
  } 
  
  //Declare and Initialize
  int pos = suffix.find("/");
  std::string component;

  //Set up component to check and change suffix
  if (pos != (int) std::string::npos) {
    component = suffix.substr(0, pos);
    suffix = suffix.substr(pos + 1);
  } else {
    component = suffix;
    suffix = "";
  }

  //If wildcards dont exist
  if (component.find("*") == std::string::npos && component.find("?") == std::string::npos) {
    if (prefix.empty()) {
      if (component.empty()) {
        exp_wild("/" + component, suffix);
      } else {
        exp_wild(component, suffix);
      }
    } else {
      if (prefix == "/") {
        exp_wild("/" + component, suffix);
      } else {
        exp_wild(prefix + "/" + component, suffix);
      }
    }
    return;
  }

  //If wildcards exist
  //Convert to regex
  component = replaceAll(component, ".", "\\.");
  component = replaceAll(component, "*", ".*");
  component = replaceAll(component, "?", ".");
  component = "^" + component + "$";

  //Set directory
  std::string dir = prefix.empty() ? "." : prefix;
  DIR * dirp = opendir(dir.c_str());

  //Check for directory error
  if (dirp == NULL) {
    return;
  }  

  //Declare and Initialize for regex
  struct dirent *entry;
  std::vector<std::string> arg_list = std::vector<std::string>();  

  //Loop through regex matches
  while ((entry = readdir(dirp))) {
    //Setup regex
    regex_t re;
    regmatch_t match;
    int result;  
    int add_arg = 0;

    //Store regex in re
    result = regcomp(&re, component.c_str(), REG_EXTENDED | REG_NOSUB);
    
    //Check for errors
    if (result != 0) {
      perror("regcomp");
      exit(-1);
    }

    //Compare regex
    result = regexec(&re, entry->d_name, 1, &match, 0);

    //Check if match
    if (!result) {
        if (entry->d_name[0] == '.') {
          if (component[1] == '\\') {
            add_arg = 1;
          }
        } else {
          add_arg = 1;
        }
    }  

    //Free reg
    regfree(&re);

    //If passed requirements, add
    if (add_arg) {
      std::string name = std::string(entry->d_name);

      if (prefix.empty()) {
        if (name.empty()) {
          arg_list.push_back("/" + name);
        } else {
          arg_list.push_back(name);
        }
      } else {
        if (prefix == "/") {
          arg_list.push_back("/" + name);
        } else {
          arg_list.push_back(prefix + "/" + name);
        }
      }
    }
  }

  //Close dirp
  closedir(dirp);

  //Sort
  sort(arg_list.begin(), arg_list.end());
  //my_sort(arg_list);

  //Run recursive
  for (int i = 0; i < (int) arg_list.size(); i++) {
    exp_wild(arg_list[i], suffix);
  }
}

void expandWildcards(std::string* arg) {
  //Check if contains * or ?
  if ((arg->find("?") == std::string::npos 
  && arg->find("*") == std::string::npos) 
  || Shell::quotes) {
    Shell::quotes = false;
    Command::_currentSimpleCommand->insertArgument(new std::string(*arg));
    return;
  }

  //Set up added_args
  added_args = 0;

  exp_wild("", *arg);

  if (!added_args) {
    Command::_currentSimpleCommand->insertArgument(new std::string(*arg));
  }

}

char* expand(std::string env) {
  //Declare and Initialize
  char* str;
  std::stringstream ss;

  //Check case
  if (env == "$") {
    ss << getpid();
    str = strdup(ss.str().c_str());
  } else if (env == "?") {
    ss << Shell::return_val;
    str = strdup(ss.str().c_str());
  } else if (env == "!") {
    ss << Shell::back_pid;
    str = strdup(ss.str().c_str());
  } else if (env == "_") {
    str = strdup(Shell::last_command.c_str());
  } else if (env == "SHELL") {
    str = strdup(path);
  } else if (getenv(env.c_str())) {
    str = strdup(getenv(env.c_str()));
  } else {
    str = strdup("");
  } 

  return str;
}

void env_exp(char* text) {
  std::string word = std::string(text);
  size_t start;
  size_t end;

  //Loop through word and replace ${}
  while ((start = word.find('$')) != std::string::npos) {
    //Set start and end
    end = word.find('}', start);

    //Expand 
    char* exp = expand(word.substr(start + 2, end - start - 2));
    
    //Replace the word and delete
    word = word.replace(start, end - start + 1, exp);
    free(exp);
  }

  //Unput
  unput_chars(word.c_str());
}

void tilde_exp(char* str) {
  //Grab home expansion
  std::string path_name = std::string(home_exp());
  path_name += "/";


  //Replace tilde with home
  std::string to_write = std::string(str);

  //Check for tilde then /
  if (to_write.find("~/", 0) != std::string::npos) {
    to_write =  replaceAll(to_write, "~/", path_name);
  } 

  //Check for tilde then no /
  if (to_write.find("~", 0) != std::string::npos) {
    to_write = replaceAll(to_write, "~", "/homes/");
  }

  //Check if solo tilde
  if (to_write == "/homes/") {
    path_name.pop_back();
    to_write = path_name;
  }

  //Unput
  unput_chars(to_write.c_str());  
}

void subShell(char* text) {
  //Fix command
  text[strlen(text) - 1] = 0;
  char *command = strdup(text + 2);

  //Save current state
  int tmp_in = dup(0);
  int tmp_out = dup(1);

  //Create string to return
  std::string *buf = new std::string();

  //Create pipe
  int fdpipe[2];
  int sdpipe[2];
  pipe(fdpipe);
  pipe(sdpipe);

  //Fork child process
  int pid = fork();

  if (pid < 0) { //Check for failed fork
      perror("fork");
      exit(2);
  } else if (pid == 0) { //Execute if child
      //Redirect and closing
      dup2(fdpipe[0], 0); close(fdpipe[0]);
      dup2(sdpipe[1], 1); close(sdpipe[1]);
      close(fdpipe[1]); close(sdpipe[0]);
    
      //Execute, will read from first_pipe and write to second_pipe
      execlp("/proc/self/exe", "/proc/self/exe", (char *) 0);

      //Exit
      perror("execle");
      exit(1);
  } else { //Communicate with child
    //Close pipes
    close(fdpipe[0]); close(sdpipe[1]);

    //Send command to stdin    
    write(fdpipe[1], command, strlen(command));
    write(fdpipe[1], "\n", 1);
    write(fdpipe[1], "quit\n", 5);

    close(fdpipe[1]); 
    
    //Declare and initialize
    char* tmp_buf = (char*) malloc(sizeof(char));

    //Read byte by byte
    while (read(sdpipe[0], tmp_buf, 1)) {
      buf->push_back(*tmp_buf);
    }

    close(sdpipe[0]);

    //Close and free
    free(tmp_buf);
    free(command);

    //Make sure child process is complete
    waitpid(pid, 0, 0);
  }

  //Reset stdin and stdout
  dup2(tmp_in, 0); close(tmp_in);
  dup2(tmp_out, 1); close(tmp_out);

  //Unput string
  buf->pop_back();
  unput_chars(buf->c_str());

  //Delete
  delete buf;
}

void Shell::prompt() {
  if (isatty(0)) {
    //Check for error code
    if (Shell::return_val && getenv("ON_ERROR")) {
      printf("%s\n", getenv("ON_ERROR"));
    }
    std::string prompt_msg = (getenv("PROMPT")) ? (std::string(getenv("PROMPT")) + " ") : "myshell> ";
    printf("%s", prompt_msg.c_str());  
    fflush(stdout);
  }
}

int main(int argc, char* argv[]) {
  //Grab full path and initialize and set pid
  if (argc > 0){
    path = realpath(*argv, NULL);
  } 

  //Set quotes
  Shell::quotes = false;

  //Catch Ctrl-C
  struct sigaction sa_int; 
  sa_int.sa_handler = sigint_handler; 
  sigemptyset(&sa_int.sa_mask); 
  sa_int.sa_flags = SA_RESTART; 

  int error_int = sigaction(SIGINT, &sa_int, NULL ); 
  if (error_int) { 
    perror( "sigaction" ); 
    exit(-1); 
  } 

  //Zombie Process Cleanup
  struct sigaction sa_chld; 
  sa_chld.sa_handler = sigchild_handler; 
  sigemptyset(&sa_chld.sa_mask); 
  sa_chld.sa_flags = SA_RESTART; 

  int error_chld = sigaction(SIGCHLD, &sa_chld, NULL ); 
  if ( error_chld ) { 
    perror( "sigaction" ); 
    exit( -1 ); 
  } 

  Shell::prompt();
  yyparse();
}

Command Shell::_currentCommand;
