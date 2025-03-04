//SLUSH - The SLU SHELL
//
//This assignment reads user input, and executes that input as a sequence of
//programs to execute.
//
//Program execution commands have the form:
//
//prog_n [args] ( ... prog_3 [args] ( prog_2 [args] ( prog_1 [args]
//
//For example, "grep slush ( sort -r ( ls -l -a" returns a reverse sorted list
//of all files in the current directory that contain the string "slush" in
//
//See the lab writeup for more details and other requirements.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>  
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFERSIZE 1024
#define TOKENS 15



// Ignore signal type SIGINT
void ignoreSignal(int signum) {
    printf("\n");
    return;    
}


// Function to aid in command parsing, will simply remove spaces 
void rmvspace(char* command, char** out) {
    int i = 0;
    char* arg = strtok(command, " ");
    out[i] = arg; 
    while(arg != NULL) {             
        arg = strtok(NULL, " "); 
        i++;
        if (i == sizeof(out) - 1) {
            printf("Error: Command Arguments Exceed Size of Array\n");
            break;
        }
        out[i] = arg;
    }
    out[i+1] = NULL;
}


void executeCommand(char** command) {
    // Create a child process to handle the execution
    pid_t child = fork();
    if(child == -1) {
        perror("Error: ");
        exit(-1);
    }

    // Child process code
    if (child == 0) {
        int ret = execvp(command[0], command);
        if (ret != 0) {
            perror("Error: ");
            exit(-1);
        }
    }
    // Make parent wait on child 
    else {
        waitpid(child, NULL, 0);
        return;
    }
}


// This function is a single pipe, 2 process exeuction 
void executePipe(char** args, char** piped) {
    // Initialize the pipe 
    // 0 is read stream, 1 is write stream
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Error: ");
        exit(-1);
    }
    pid_t child1 = fork();
    if (child1 == -1) {
        perror("Error: ");
        exit(-1);
    }


    // Child process for writing to pipe
    if (child1 == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        int ret = execvp(args[0],args);
        if (ret != 0) {
            perror("Error: ");
            exit(-1);
        }
        exit(0);
    }
    else {
        // Child process for reading from pipe 
        pid_t child2 = fork();
        if(child2 == -1) {
            perror("Error: ");
            exit(-1);
        }

        if(child2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            int ret = execvp(piped[0], piped);
            if (ret != 0) {
                perror("Error: ");
                exit(-1);
            }
            exit(0);
        }
        // Parent must close its ends of the pipe, and wait on last child processes
        else {
            close(pipefd[0]);
            close(pipefd[1]);
            waitpid(child2, NULL, 0);
        }
    }

}


void execnpipes(char** args, int n) {
    int ret;

    // Initialize pipes (read and write) for n - 1 processes 
    // n - 1 represents the number of pipes needed, *2 represents the read/write ends of each pipe
    int pipefd[2 * (n-1)];
    for(int i = 0; i < n - 1; i++) {
        ret = pipe(pipefd  + i * 2);
        if (ret == -1) {
            perror("Error Creating Pipes:  ");
            exit(-1);
        }
    }

    // Each command present will get its respective fork through this loop
    for(int i = 0; i < n; i++) {
        pid_t child = fork();
        if(child == -1) {
            perror("Error Forking: ");
            exit(-1);
        }
        if(child == 0) {
            // This  allows the read end of the pipe to be opened (for middle/end processes)
            if(i > 0) {
                dup2(pipefd[(i - 1) * 2], STDIN_FILENO);
            }
            // This allows the write end of the pipe to be opened (for start/middle processes)
            if(i < n -1) {
                dup2(pipefd[i * 2 + 1], STDOUT_FILENO);
            }

            // Close each associated pipe for the child process
            for(int j = 0; j < 2 * (n-1); j++) {
                close(pipefd[j]);
            }

            // Parse current command and execute 
            char* parsed_args[10];
            rmvspace(args[i], parsed_args);
            ret = execvp(parsed_args[0], parsed_args);
            if(ret == -1) {
                perror("Error: ");
                exit(-1);
            }
            exit(0);
        }
    }
    // Parent process will close all ends of each pipe (it does not need any of them)
    for(int i = 0; i < 2 * (n -1); i++) {
        close(pipefd[i]);
    }

    // Parent  process will wait on child processes to finish 
    for(int i = 0; i < n; i++) {
        wait(NULL);
    }
}

// Function to print only necessary directory path
void printDirectory() {
    char cwd[BUFFERSIZE];
    char* home;

    if(getcwd(cwd, BUFFERSIZE) == NULL) {
        printf("Error Obtaining Directory Path");
        exit(-1);
    }
    home = getenv("HOME");
    if (home == NULL) {
        printf("Error Obtaining Home Path");
    }

    // Compare two strings in order to remove home path
    if(strncmp(cwd, home, strlen(home)) == 0 ) {        // If strncmp returns 0, then the home directory is equal to  the start of cwd and can be bypassed when printed
        printf("slush|%s>", cwd + strlen(home) + 1);
    }
    else {                                              // Otherwise, simply print the full path (some discrepancy occured)
        printf("slush|%s>", cwd);
    }
}

void changeDir(char* path) {
    if(chdir(path) != 0) {
        perror("Failed to Change Directory : ");
    }
}


int main() {
    char buffer[BUFFERSIZE];      // User input buffer
    char* commands[TOKENS];       // Command tokens 
    int ret;                      // Monitor return values throughout program

    signal(2, ignoreSignal);

    // Main loop:
    while(1) {
        printDirectory();

        // Obtain an input from the user
        if (fgets(buffer, BUFFERSIZE, stdin) == NULL) {
            printf("\n");
            return 0;
        }

        // Break the input into a set of tokens
        buffer[strcspn(buffer, "\n")] = 0; // Truncate newline character from input
        char* token = strtok(buffer, "("); 
        int ind = 0;
        commands[ind] = token;

        while (token != NULL) {
            token = strtok(NULL, "(");
            ind++;
            commands[ind] = token;
        }

        // Built in command 'cd' support
        if(strncmp(commands[0], "cd", 2) == 0) {
            char* parsed_args[3];
            rmvspace(commands[0],parsed_args);
            changeDir(parsed_args[1]);
        }
        else {
            // Command line execution w/o need for pipes 
            if (ind == 1) {
                char* parsed_args[10];
                rmvspace(commands[0], parsed_args);
                executeCommand(parsed_args);
            }
            // Command line exeuction w/ a single pipe present
            else if(ind == 2) {
                char* parsed_args[10];
                rmvspace(commands[0], parsed_args);
                char* parsed_pipe[10];
                rmvspace(commands[1], parsed_pipe);
                executePipe(parsed_args, parsed_pipe);

            }
            // Command line exeuction with a variable number of pipes (must deal with start, middle, and end processes);
            else if(ind > 2 ) { 
                execnpipes(commands, ind);
            }
            else { } // Simply move onto next loop to reprint command line to terminal 
        }

    }


    return 0;
}