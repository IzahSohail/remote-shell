#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

void parseInput(char *userInput, char *command[], int *argCount) {
    *argCount = 0;
    int inQuote = 0;
    char *start = userInput;

    while (*userInput) {
        if (*userInput == '"') {
            inQuote = !inQuote;
        } else if (isspace(*userInput) && !inQuote) {
            *userInput = '\0';
            if (*start) {
                command[*argCount] = start;
                (*argCount)++;
            }
            start = userInput + 1;
        }
        userInput++;
    }

    if (*start) {
        command[*argCount] = start;
        (*argCount)++;
    }

    command[*argCount] = NULL;

    for (int i = 0; command[i] != NULL; i++) {
        printf("Token[%d]: %s\n", i, command[i]);
    }
}
