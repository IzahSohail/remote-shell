#ifndef EXECUTOR_H
#define EXECUTOR_H

void noArgCommand(char *command[]);
void handleRedirect(char *command[]);
void inputRedirect(char *command[]);
void handlePipes(char *command[]);
void handleMultiplePipes(char *command[]);
void handlePipeRedirect(char *command[]);
void handleCombinedRedirect(char *command[]);

#endif // EXECUTOR_H
