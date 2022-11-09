/*
 * CS252: Shell project
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 * DO NOT PUT THIS PROJECT IN A PUBLIC REPOSITORY LIKE GIT. IF YOU WANT 
 * TO MAKE IT PUBLICALLY AVAILABLE YOU NEED TO REMOVE ANY SKELETON CODE 
 * AND REWRITE YOUR PROJECT SO IT IMPLEMENTS FUNCTIONALITY DIFFERENT THAN
 * WHAT IS SPECIFIED IN THE HANDOUT. WE OFTEN REUSE PART OF THE PROJECTS FROM  
 * SEMESTER TO SEMESTER AND PUTTING YOUR CODE IN A PUBLIC REPOSITORY
 * MAY FACILITATE ACADEMIC DISHONESTY.
 */

#include <cstdio>
#include <cstdlib>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>

#include "command.hh"
#include "shell.hh"

extern char** environ;

Command::Command() {
    // Initialize a new vector of Simple Commands
    _simpleCommands = std::vector<SimpleCommand *>();

    _outFile = NULL;
    _inFile = NULL;
    _errFile = NULL;
    _background = false;
    append_out = false;
    append_err = false;
}

std::string last_simple(std::vector<SimpleCommand *> commands) {
    int num_simples = commands.size();
    int num_args = commands[num_simples - 1]->_arguments.size();

    return *commands[num_simples - 1]->_arguments[num_args - 1];
}

void Command::insertSimpleCommand( SimpleCommand * simpleCommand ) {
    // add the simple command to the vector
    _simpleCommands.push_back(simpleCommand);
}

void Command::clear() {
    // deallocate all the simple commands in the command vector
    for (auto simpleCommand : _simpleCommands) {
        delete simpleCommand;
    }

    // remove all references to the simple commands we've deallocated
    // (basically just sets the size to 0)
    _simpleCommands.clear();

    

    if ( _outFile ) {
        if (_outFile == _errFile) { //Check for if output and error are redirected
            _errFile = NULL;
        }
        delete _outFile;
    }
    _outFile = NULL;

    if ( _inFile) {
        delete _inFile;
    }
    _inFile = NULL;

    if ( _errFile) {
        delete _errFile;
    }
    _errFile = NULL;

    _background = false;
}

void Command::print() {
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    int i = 0;
    // iterate over the simple commands and print them nicely
    for ( auto & simpleCommand : _simpleCommands ) {
        printf("  %-3d ", i++ );
        simpleCommand->print();
    }

    printf( "\n\n" );
    printf( "  Output       Input        Error        Background\n" );
    printf( "  ------------ ------------ ------------ ------------\n" );
    printf( "  %-12s %-12s %-12s %-12s\n",
            _outFile?_outFile->c_str():"default",
            _inFile?_inFile->c_str():"default",
            _errFile?_errFile->c_str():"default",
            _background?"YES":"NO");
    printf( "\n\n" );
}

void Command::execute() {    
    // Don't do anything if there are no simple commands
    if ( _simpleCommands.size() == 0 ) {
        Shell::prompt();
        return;
    }

    //Global Variables for execution
    pid_t pid = 0;
    unsigned int idx = 0;
    int dflt_in = dup(0);
    int dflt_out = dup(1);
    int dflt_err = dup(2);
    int fd_in;
    int fd_out;
    int fd_err = dup(2);
    int status;

    //Setup input file
    if (!_inFile) {
        fd_in = dup(dflt_in);
    } else {
        //Input file 
        fd_in = open(_inFile->c_str(), O_RDONLY, 0444);
        
        //Check if opening file error
        if (fd_in < 0) {
            perror("open");
            exit(1);
        }
    }
    
    
    for (SimpleCommand* simple : _simpleCommands) {
        //Declare command_name of simple
        const char *command_name = simple->_arguments[0]->c_str();

        //Change input and close   
        dup2(fd_in, 0); close(fd_in);

        if (idx == _simpleCommands.size() - 1) { //IO redirection
            
            //Set up Output
            if (!_outFile) {
                fd_out = dup(dflt_out);
            } else {
                //Output file
                if (append_out == true) {
                   fd_out = open(_outFile->c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
                } else {
                   fd_out = creat(_outFile->c_str(), 0666);
                }

                //Check if opening file error
                if (fd_out < 0) {
                    perror("open");
                    exit(1);
                }
            }

            //Set up Error
            if (_errFile) {
                //Error file
                if (append_err == true) {
                   fd_err = open(_errFile->c_str(), O_RDWR | O_CREAT | O_APPEND, 0666);
                } else {
                   fd_err = creat(_errFile->c_str(), 0666);
                }

                //Check if opening file error
                if (fd_err < 0) {
                    perror("open");
                    exit(1);
                }
            }

            //Change error and close
            dup2(fd_err, 2); close(fd_err);
        } else {
            //Pipe
            int fdpipe[2];
            pipe(fdpipe);

            //Set input to be previous pipe
            fd_in = fdpipe[0];
            
        
            //Set output to be next pipe
            fd_out = fdpipe[1];
        }    
     

        //Change output and close
        dup2(fd_out, 1); close(fd_out);

        //Check for special system commands
        //setenv
        if (!strcmp("setenv", command_name)) {
            //Grab name and  new value
            char *env_name = strdup(simple->_arguments[1]->c_str());
            char *env_val = strdup(simple->_arguments[2]->c_str());

            //Set 
            setenv(env_name, env_val, 1);

            //free
            free(env_name);
            free(env_val);

            //Continue looping
            idx++;
            Shell::return_val = 0;
            continue;
        }

        //unsetenv
        if (!strcmp("unsetenv", command_name)) {
            //Grab name
            char *env_name = strdup(simple->_arguments[1]->c_str());

            //unsetenv
            unsetenv(env_name);

            //free
            free(env_name);
            
            //Continue looping
            idx++;
            Shell::return_val = 0;            
            continue;

        }

        //cd
        if (!strcmp("cd", command_name)) {
            //Path name
            const char* path_name;

            //Check if second argument
            if (simple->_arguments.size() == 1) {
                //Grab home directory
                path_name = home_exp();
            } else {
                path_name = simple->_arguments[1]->c_str();
            }

            //Change directory and check for error
            int err = chdir(path_name);

            if (err == -1) {
                //Set up error message
                char* err_msg = (char*) malloc(sizeof(char) * (17 + strlen(path_name)));
                sprintf(err_msg, "cd: can't cd to %s", path_name);
                perror(err_msg);

                //free
                free(err_msg);
            }

            //Continue looping
            idx++;
            Shell::return_val = 0;
            continue;
        }


        //Start child and parent processes
        pid = fork();
        
        //Execute and/or wait
        if (pid < 0) { //Check for failed fork
            perror("fork");
            exit(2);
        } else if (pid == 0) { //Check for if child process
            //printenv
            if (!strcmp("printenv", command_name)) {
                char** ptr = environ;
                while (*ptr != NULL) {
                    printf("%s\n", *ptr);
                    ptr++;
                }

                //Exit
                exit(0);
            }
            //Set up arguments to pass to execvp
            std::vector<char const*> char_vector;
            std::transform(simple->_arguments.begin(), simple->_arguments.end(), std::back_inserter(char_vector),
                [](std::string *s) { return s->c_str(); });
            char_vector.push_back(nullptr);
            char *const *arguments = (char *const *) char_vector.data();

            //Execute and set return
            execvp(command_name, arguments);

            //If error, print then exit child process
            perror("execpv");
            exit(1);
        } 
        //Increment counter
        idx++;
        if (!_background) {
            waitpid(pid, &status, 0);
            Shell::return_val = WEXITSTATUS(status);
        }
    }
    //Close in
    close(fd_in);

    //Set last command
    Shell::last_command = last_simple(_simpleCommands);
    
    //Wait for last command
    if (!_background) { //If parent process, wait for child to finish
        waitpid(pid, NULL, 0);
    } else {
        Shell::back_pid = pid;
    }
    
    //Reset and Close Defaults
    reset_files(dflt_in, dflt_out, dflt_err);
    close_files(dflt_in, dflt_out, dflt_err);

    
    
    // Clear to prepare for next command
    clear();

    // Print new prompt
    Shell::prompt();
}

//Sets stdin, stdout, and stderr back to defaults
void Command::reset_files(int dflt_in, int dflt_out, int dflt_err) {
    dup2(dflt_in, 0);
    dup2(dflt_out, 1);
    dup2(dflt_err, 2);
}

//Closes the main copies of stdin, stdout, and stderr
void Command::close_files(int dflt_in, int dflt_out, int dflt_err) {
    close(dflt_in);
    close(dflt_out);
    close(dflt_err);
}

SimpleCommand * Command::_currentSimpleCommand;
