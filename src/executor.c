#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include "executor.h"

// 1. Handling shell commands without arguments
void noArgCommand(char *command[]) {
    pid_t pid = fork();
    if (pid == 0) {
        execvp(command[0], command);
        fprintf(stderr, "Error: Command '%s' not found.\n", command[0]);
        exit(1);
    } else {
        wait(NULL);
    }
}

// 2. Handling output redirection (>, 2>)
void handleRedirect(char *command[]) {
    pid_t pid = fork();
    if (pid == 0) {
        int fd, i = 0, j = 0;
        char *cleanCommand[10];

        while (command[i] != NULL) {
            if (strcmp(command[i], ">") == 0) {
                if (command[i+1] == NULL) {
                    fprintf(stderr, "Output file not file not specified\n");
                    exit(1);
                }
                fd = open(command[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { // in case we cannot find the file or open it
                    perror("File open failed");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                i += 2;
            } else if (strcmp(command[i], "2>") == 0) {
                if (command[i+1] == NULL) {
                    fprintf(stderr, "Error: Missing filename after '2>'\n");
                    exit(1);
                }
                fd = open(command[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("File open failed");
                    exit(1);
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
                i += 2;
            } else {
                cleanCommand[j++] = command[i++];
            }
        }
        cleanCommand[j] = NULL;
        execvp(cleanCommand[0], cleanCommand);
        perror("Exec failed");
        exit(1);
    } else {
        wait(NULL);
    }
}

// 3. Handling input redirection
void inputRedirect(char *command[]) {
    pid_t pid = fork();
    if (pid == 0) {
        int fd, i = 0, j = 0;
        char *cleanCommand[50];

        while (command[i] != NULL) {
            if (strcmp(command[i], "<") == 0) {
                if (command[i+1] == NULL) {
                    fprintf(stderr, "Error: Missing filename after '<'\n");
                    exit(1);
                }
                fd = open(command[i+1], O_RDONLY);
                if (fd < 0) {
                    perror("File open failed");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
                i += 2;
            } else {
                cleanCommand[j++] = command[i++];
            }
        }
        cleanCommand[j] = NULL;
        execvp(cleanCommand[0], cleanCommand);
        perror("Exec failed");
        exit(1);
    } else {
        wait(NULL);
    }
}

// 4. Handling simple pipes (command1 | command2)
void handlePipes(char *command[]) {
    int fd[2];
    if (pipe(fd) == -1) {
        perror("Pipe failed");
        return;
    }

    int i = 0;
    while (command[i] != NULL && strcmp(command[i], "|") != 0) {
        i++;
    }

    if (command[i] == NULL || command[i+1] == NULL) {  // Check if command after "|" exists
        fprintf(stderr, "Error: Command missing after pipe.\n");
        return;
    }

    command[i] = NULL;  // to separate the two commands

    pid_t pid1 = fork();
    if (pid1 < 0) {
        perror("Fork failed");
        return;
    }
    if (pid1 == 0) {  
        close(fd[0]);  
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]); 
        execvp(command[0], command);
        fprintf(stderr, "Error: Command '%s' not found.\n", command[0]);
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 < 0) {
        perror("Fork failed");
        return;
    }
    if (pid2 == 0) { 
        close(fd[1]);  
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);  
        execvp(command[i+1], &command[i+1]);
        fprintf(stderr, "Error: Command '%s' not found.\n", command[i+1]);
        exit(1);
    }

    // Close both pipe ends in parent
    close(fd[0]);
    close(fd[1]);

    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);
}


// 5. Handling multiple pipes (command1 | command2 | command3)
void handleMultiplePipes(char *command[]) {
    int cmd_count = 1;
    int i = 0;

    while (command[i] != NULL) {
        if (strcmp(command[i], "|") == 0) cmd_count++;
        i++;
    }

    // Check for empty commands between pipes
    for (i = 0; command[i] != NULL; i++) {
        if ((strcmp(command[i], "|") == 0) &&
            (i == 0 || command[i+1] == NULL || strcmp(command[i+1], "|") == 0)) {
            fprintf(stderr, "Error: Empty command between pipes.\n");
            return;
        }
    }


    int pipes[10][2];
    for (i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("Pipe creation failed");
            return;
        }
    }

    int cmd_start = 0;
    for (i = 0; i < cmd_count; i++) {
        int cmd_end = cmd_start;
        while (command[cmd_end] != NULL && strcmp(command[cmd_end], "|") != 0) {
            cmd_end++;
        }

        char *cmd[50];
        int j;
        for (j = 0; j < cmd_end - cmd_start; j++) {
            cmd[j] = command[cmd_start + j];
        }
        cmd[j] = NULL;

        pid_t pid = fork();
        if (pid == 0) {
            if (i == 0) {
                dup2(pipes[0][1], STDOUT_FILENO);
            } else if (i == cmd_count - 1) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            } else {
                dup2(pipes[i-1][0], STDIN_FILENO);
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            for (int k = 0; k < cmd_count - 1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }

            if (execvp(cmd[0], cmd) == -1) {
                fprintf(stderr, "Error: Command '%s' not found in pipe sequence.\n", cmd[0]);
                exit(1);
            }
        }

        cmd_start = cmd_end + 1;
    }

    for (i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    for (i = 0; i < cmd_count; i++) {
        wait(NULL);
    }
}

// Function to handle combined redirections (command < input.txt > output.txt 2> error.log)
void handleCombinedRedirect(char *command[]) {
    pid_t pid = fork();
    if (pid == 0) {
        char *cmd[50];
        int i = 0, j = 0;
        int in_fd = -1, out_fd = -1, err_fd = -1;
        
        // Parse command and redirections
        while (command[i] != NULL) {
            if (strcmp(command[i], "<") == 0) {
                if (command[i+1] == NULL) {
                    fprintf(stderr, "Error: Input file not specified.\n");
                    exit(1);
                }
                in_fd = open(command[i+1], O_RDONLY);
                if (in_fd < 0) {
                    fprintf(stderr, "Error: Cannot open input file '%s': %s\n", 
                            command[i+1], strerror(errno));
                    exit(1);
                }
                i += 2;
            }
            else if (strcmp(command[i], ">") == 0) {
                if (command[i+1] == NULL) {
                    fprintf(stderr, "Error: Output file not specified.\n");
                    exit(1);
                }
                out_fd = open(command[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd < 0) {
                    fprintf(stderr, "Error: Cannot open output file '%s': %s\n", 
                            command[i+1], strerror(errno));
                    exit(1);
                }
                i += 2;
            }
            else if (strcmp(command[i], "2>") == 0) {
                if (command[i+1] == NULL) {
                    fprintf(stderr, "Error: Error output file not specified.\n");
                    exit(1);
                }
                err_fd = open(command[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (err_fd < 0) {
                    fprintf(stderr, "Error: Cannot open error file '%s': %s\n", 
                            command[i+1], strerror(errno));
                    exit(1);
                }
                i += 2;
            }
            else if (strcmp(command[i], "2>&1") == 0) {
                dup2(STDOUT_FILENO, STDERR_FILENO);
                i++;
            }
            else {
                cmd[j++] = command[i++];
            }
        }
        cmd[j] = NULL;
        
        // Apply redirections
        if (in_fd != -1) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != -1) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        if (err_fd != -1) {
            dup2(err_fd, STDERR_FILENO);
            close(err_fd);
        }
        
        execvp(cmd[0], cmd);
        fprintf(stderr, "Error: Command '%s' not found.\n", cmd[0]);
        exit(1);
    }
    wait(NULL);
}

// Function to handle combined pipes and redirections (e.g., cmd1 < in.txt | cmd2 > out.txt)
void handlePipeRedirect(char *command[]) {
    int cmd_count = 1;
    int i = 0;
    
    // Count number of commands (pipes + 1)
    while (command[i] != NULL) {
        if (strcmp(command[i], "|") == 0) cmd_count++;
        i++;
    }
    
    // Create array of pipe file descriptors
    int pipes[10][2];  // Support up to 10 pipes
    
    // Create all pipes
    for (i = 0; i < cmd_count - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("Pipe creation failed");
            return;
        }
    }
    
    // Split commands and execute them
    int cmd_start = 0;
    for (i = 0; i < cmd_count; i++) {
        // Find end of current command
        int cmd_end = cmd_start;
        while (command[cmd_end] != NULL && strcmp(command[cmd_end], "|") != 0) {
            cmd_end++;
        }
        
        // Check if the command is empty
        if (cmd_end == cmd_start) {
            fprintf(stderr, "Error: Empty command between pipes.\n");
            // Clean up pipes
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return;
        }
        
        // Create command array
        char *cmd[50];
        int j;
        for (j = 0; j < cmd_end - cmd_start; j++) {
            cmd[j] = command[cmd_start + j];
        }
        cmd[j] = NULL;
        
        // Fork and execute
        pid_t pid = fork();
        if (pid == 0) {
            // Set up pipe connections
            if (i == 0) {
                // First command
                if (cmd_count > 1) {
                    dup2(pipes[0][1], STDOUT_FILENO);
                }
            }
            else if (i == cmd_count - 1) {
                // Last command
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            else {
                // Middle command
                dup2(pipes[i-1][0], STDIN_FILENO);
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            
            // Close all pipes in child
            for (int k = 0; k < cmd_count - 1; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            
            // Process redirections within this command
            char *clean_cmd[50];
            int n = 0, m = 0;
            int in_fd = -1, out_fd = -1, err_fd = -1;
            
            while (cmd[n] != NULL) {
                if (strcmp(cmd[n], "<") == 0) {
                    if (cmd[n+1] == NULL) {
                        fprintf(stderr, "Error: Input file not specified.\n");
                        exit(1);
                    }
                    in_fd = open(cmd[n+1], O_RDONLY);
                    if (in_fd < 0) {
                        fprintf(stderr, "Error: Cannot open input file '%s': %s\n", 
                                cmd[n+1], strerror(errno));
                        exit(1);
                    }
                    n += 2;
                }
                else if (strcmp(cmd[n], ">") == 0) {
                    if (cmd[n+1] == NULL) {
                        fprintf(stderr, "Error: Output file not specified.\n");
                        exit(1);
                    }
                    out_fd = open(cmd[n+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (out_fd < 0) {
                        fprintf(stderr, "Error: Cannot open output file '%s': %s\n", 
                                cmd[n+1], strerror(errno));
                        exit(1);
                    }
                    n += 2;
                }
                else if (strcmp(cmd[n], "2>") == 0) {
                    if (cmd[n+1] == NULL) {
                        fprintf(stderr, "Error: Error output file not specified.\n");
                        exit(1);
                    }
                    err_fd = open(cmd[n+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (err_fd < 0) {
                        fprintf(stderr, "Error: Cannot open error file '%s': %s\n", 
                                cmd[n+1], strerror(errno));
                        exit(1);
                    }
                    n += 2;
                }
                else if (strcmp(cmd[n], "2>&1") == 0) {
                    dup2(STDOUT_FILENO, STDERR_FILENO);
                    n++;
                }
                else {
                    clean_cmd[m++] = cmd[n++];
                }
            }
            clean_cmd[m] = NULL;
            
            // Check if the command is empty
            if (m == 0) {
                fprintf(stderr, "Error: Empty command.\n");
                exit(1);
            }
            
            // Apply redirections
            if (in_fd != -1) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (out_fd != -1) {
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
            if (err_fd != -1) {
                dup2(err_fd, STDERR_FILENO);
                close(err_fd);
            }
            
            execvp(clean_cmd[0], clean_cmd);
            fprintf(stderr, "Error: Command '%s' not found.\n", clean_cmd[0]);
            exit(1);
        }
        
        cmd_start = cmd_end + 1;
        
        // Check if there's a missing command after pipe
        if (command[cmd_start] == NULL && i < cmd_count - 1) {
            fprintf(stderr, "Error: Command missing after pipe.\n");
            // Clean up pipes
            for (int j = 0; j < cmd_count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Wait for forked children
            for (int j = 0; j <= i; j++) {
                wait(NULL);
            }
            return;
        }
    }
    
    // Close all pipes in parent
    for (i = 0; i < cmd_count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    
    // Wait for all children
    for (i = 0; i < cmd_count; i++) {
        wait(NULL);
    }
}