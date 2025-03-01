#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

// Function to parse user input into an array of arguments
void parseInput(char *userInput, char *command[], int *argCount) {
    *argCount = 0;
    char *token = strtok(userInput, " \n");

    while (token != NULL) {
        // Handle quoted arguments 
        if (token[0] == '"') {
            char *endQuote = strchr(token + 1, '"'); // Find closing quote
            if (endQuote) {
                *endQuote = '\0'; // Remove the closing quote
                token++; // Move past the opening quote
            }
        }

        command[*argCount] = token;
        (*argCount)++;
        token = strtok(NULL, " \n");
    }
    command[*argCount] = NULL; // Null-terminate the argument list
}
