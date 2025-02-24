#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for string manipulation 
#include <unistd.h> // for fork, exec, pid_t
#include <sys/wait.h> // for wait
#include <sys/types.h> // for pid_t

// 1. handling shell Commands with No Arguments (ls, ps etc)
// 2. this function can also be used to handle commands with arguments

void noArgCommand( char *command[] ) {
    pid_t pid = fork();
    if (pid != 0) {
        wait(NULL);
    } else {
        execvp(command[0], command);
    }
}

int main(){
    char userInput[100];
    char *command[10];
    int i = 0; // this is used to keep track of the number of arguments in the command

    while(1){
        printf("$ ");
        fgets(userInput, 100, stdin);
        command[i] = strtok(userInput, " \n");

        // as long as the command is not NULL, keep tokenizing the command
        while(command[i] != NULL){
            i++; // increment i to keep track of the number of arguments in the command 
            command[i] = strtok(NULL, " \n");
            // doing so, we can handle commands with multiple arguments
        }

        if(strcmp(command[0], "exit") == 0){
            exit(0);
        } else {
            noArgCommand(command);
        }
        i = 0; // reset i to 0 for the next command
    }
}