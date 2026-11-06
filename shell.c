#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char prompt[] = "> ";
char delimiters[] = " \t\r\n";
extern char **environ;


volatile sig_atomic_t foreground_pid = -1;

// handle ctrl+c
void handle_sigint(int sig) {
    (void)sig;

    if (foreground_pid > 0) {
        kill((pid_t)foreground_pid, SIGINT);
    }
}

// handle 10-second timeout
void handle_sigalrm(int sig) {
    (void)sig;

    if (foreground_pid > 0) {
        kill((pid_t)foreground_pid, SIGKILL);
    }
}


int main() {

    // Stores the string typed into the command line.
    char command_line[MAX_COMMAND_LINE_LEN];
    char cmd_bak[MAX_COMMAND_LINE_LEN];

    // declares array that will hold cwd 
    char directory[1024];

    // Stores the tokenized command line input.
    char *arguments[MAX_COMMAND_LINE_ARGS];

    
    
    //  ctrl+c signal handling
    struct sigaction sa = {0};              // initialize signal handler settings
    sa.sa_handler = handle_sigint;          // assign ctrl+c handler function
    sigemptyset(&sa.sa_mask);               // clear blocked signals
    sigaction(SIGINT, &sa, NULL);           // register ctrl+c handler

    // set up 10-sec timeout handling
    struct sigaction alarm_sa = {0};        // initialize alarm handler settings
    alarm_sa.sa_handler = handle_sigalrm;   // assign timeout handler function
    sigemptyset(&alarm_sa.sa_mask);         // clear blocked signals
    sigaction(SIGALRM, &alarm_sa, NULL);    // register alarm handler


    	
    while (true) {
      
        do{ 

            // 0. Modify the prompt to print the current working directory
              // get the cwd 
            getcwd(directory, 1024);
              // Print the shell prompt + cwd
            printf("%s%s", directory, prompt);
            fflush(stdout); 
          
            

            // Read input from stdin and store it in command_line. If there's an
            // error, exit immediately.
            if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL) && ferror(stdin)) {
                fprintf(stderr, "fgets error");
                exit(0);
            }

             // If the user input was EOF (ctrl+d), exit the shell.
            if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;        }


 
        }while(command_line[0] == 0x0A);  // while just ENTER pressed
        command_line[strlen(command_line) - 1] = '\0';
      

        // 1. Tokenize the command line input (split it on whitespace)

        int index = 0;

        char *token = strtok(command_line, delimiters); //gets first token

        while(index < MAX_COMMAND_LINE_ARGS - 1 && token!=NULL){ // continues to get tokens if there r any
          arguments[index] = token;
          index++;

          token = strtok(NULL, delimiters);
        }

        // end of the arguments array
        arguments[index] = NULL;

        
        // expand environment variables
        for (int i = 0; arguments[i] != NULL; i++) {
          // check if argument starts with $
          if (arguments[i][0] == '$') {

              // get variable name without $
              char *var_name = arguments[i] + 1;

              // look up variable value
              char *var_value = getenv(var_name);

              // replace argument with value if found
              if (var_value != NULL) {
                  arguments[i] = var_value;
              }
              else {
                  // use empty string if variable doesn't exist
                  arguments[i] = "";
              }
          }
        }



        // check if command should run in background
        bool background = false;

        if (index > 0 && strcmp(arguments[index - 1], "&") == 0) {
          background = true;
          arguments[index - 1] = NULL;
        }
        if (arguments[0] == NULL) {
          continue; }
        

        
      // 2. Implement Built-In Commands

        // pwd
        if (arguments[0] != NULL && strcmp(arguments[0], "pwd") == 0){
          getcwd(directory, 1024);
          printf("%s\n", directory); }
          
        //cd
        if (arguments[0] != NULL && strcmp(arguments[0], "cd") == 0) { 
          
          if (arguments[1] == NULL) { 
            char* home = getenv("HOME");
           
            if (home==NULL) {
              fprintf(stderr, "cd: HOME not set\n"); }
            else if (chdir(home)== -1) {
              perror("cd");}
        }
        

          else {
              //check if it was an error
              if (chdir(arguments[1]) == -1){
                perror("cd");  }
            
          }
        }
        
        // echo
        if (arguments[0] != NULL && strcmp(arguments[0], "echo") == 0){
          for (int i = 1; arguments[i] != NULL; i++){
           
            if (i > 1){
              printf(" "); }

            printf("%s", arguments[i]); }
          
          printf("\n");  } // echo closing
        
        // exit
        if (arguments[0] != NULL && strcmp(arguments[0], "exit") == 0) {
          printf("Exiting shell...\n");
          return 0;  }

        // env
        if (arguments[0] != NULL && strcmp(arguments[0], "env") == 0) {
          for (int i = 0; environ[i] != NULL; i++){
            printf("%s\n", environ[i]); }
        } // env closing

        //setenv
        if (arguments[0] != NULL && strcmp(arguments[0], "setenv") == 0) {
          if (arguments[1] == NULL || arguments[2] == NULL) {
            fprintf(stderr, "Usage: setenv NAME VALUE\n");  }

          else if (setenv(arguments[1], arguments[2], 1) == -1) {
            perror("setenv"); }
        } //setenv closing

      
      
      // check if user wants to redirect output
      char *output_file = NULL;

      // loop through arguments to find >
      for (int i = 0; arguments[i] != NULL; i++) {

          // check for output redirection symbol
          if (strcmp(arguments[i], ">") == 0) {

              // make sure a filename was provided
              if (arguments[i + 1] == NULL) {
                  fprintf(stderr, "redirection: missing filename\n");
                  output_file = NULL;
                  break;
              }

              // save the filename
              output_file = arguments[i + 1];

              // remove > and filename from command arguments
              arguments[i] = NULL;
              break;
          }
      }

        // 3. Create a child process which will execute the command line input


          // 4. The parent process should wait for the child to complete unless its a background process

        if (arguments[0] != NULL &&
          strcmp(arguments[0], "pwd") != 0 &&
          strcmp(arguments[0], "cd") != 0 &&
          strcmp(arguments[0], "echo") != 0 &&
          strcmp(arguments[0], "exit") != 0 &&
          strcmp(arguments[0], "env") != 0 &&
          strcmp(arguments[0], "setenv") != 0) {

          pid_t pid = fork();

          if (pid < 0) {
            perror("fork");
          }
          
          else if (pid == 0) {
             
            // restore default ctrl+c behavior for child
            signal(SIGINT, SIG_DFL);

            // check if output redirection was requested
            if (output_file != NULL) {

              // open or create file for writing
              int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);

              // check if file opened successfully
              if (fd == -1) {
                perror("open");
                _exit(1); }

              // redirect stdout from terminal to file
              if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(fd);
                _exit(1); }
                
              // close original fd after redirecting
                close(fd); 
            }  
            
            // execute the external command
            execvp(arguments[0], arguments);

            // only runs if execvp fails
            perror("execvp");
            _exit(127);

          } // elif closing 
          
          else {
            if (!background) {
                // track foreground process
                foreground_pid = pid;

                // start 10-second timer
                alarm(10);

                int status;
                while (waitpid(pid, &status, 0) == -1) {
                    if (errno != EINTR) {
                        perror("waitpid");
                        break;
                    }
                }

                // stop timer and reset foreground pid
                alarm(0);
                foreground_pid = -1;
            }
            else {
                printf("Background process started: %d\n", pid);
            }
                }
 

        }
        

      
      
        // Hints (put these into Google):
        // man fork
        // man execvp
        // man wait
        // man strtok
        // man environ
        // man signals
        
        // Extra Credit
        // man dup2
        // man open
        // man pipes
    }
    // This should never be reached.
    return -1;
} // main closing