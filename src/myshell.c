#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include "parser.h"   // Include the parser for input parsing
#include "executor.h" // Include the executor for command execution

int main() {
    char userInput[100];
    char *command[50]; 
    int argCount;

    while (1) {
        printf("$ ");
        fflush(stdout);
        fgets(userInput, sizeof(userInput), stdin);

        // Parse the user input
        parseInput(userInput, command, &argCount);

        // Prevent segmentation fault if user just presses enter!
        if (command[0] == NULL) {
            continue;
        }

        // Exit command
        if (strcmp(command[0], "exit") == 0) {
            exit(0);
        } 

        // Check for pipes and redirections
        int pipeFound = 0, redirectFound = 0;
        int inputRedirectFound = 0, outputRedirectFound = 0, errorRedirectFound = 0;

        for (int j = 0; j < argCount; j++) {
            if (strcmp(command[j], "|") == 0) {
                pipeFound = 1;
            }
            if (strcmp(command[j], "<") == 0) {
                redirectFound = 1;
                inputRedirectFound = 1;
            }
            if (strcmp(command[j], ">") == 0) {
                redirectFound = 1;
                outputRedirectFound = 1;
            }
            if (strcmp(command[j], "2>") == 0 || strcmp(command[j], "2>&1") == 0) {
                redirectFound = 1;
                errorRedirectFound = 1;
            }
        }

        // Determine which handler to use
        if (pipeFound && redirectFound) {
            handlePipeRedirect(command);
        }
        else if (pipeFound) {
            int pipeCount = 0;
            for (int j = 0; j < argCount; j++) {
                if (strcmp(command[j], "|") == 0) {
                    pipeCount++;
                }
            }
            
            if (pipeCount > 1) {
                handleMultiplePipes(command);
            }
            else {
                handlePipes(command);
            }
        }
        else if (redirectFound) {
            if ((inputRedirectFound && outputRedirectFound) || 
                (inputRedirectFound && errorRedirectFound) || 
                (outputRedirectFound && errorRedirectFound)) {
                handleCombinedRedirect(command);
            }
            else if (inputRedirectFound) {
                inputRedirect(command);
            }
            else {
                handleRedirect(command);
            }
        }
        else {
            noArgCommand(command);
        }

    }
}
