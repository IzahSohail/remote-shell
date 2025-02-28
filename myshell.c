#include <stdio.h>
#include <stdlib.h>
#include <string.h> // for string manipulation 
#include <unistd.h> // for fork, exec, pid_t
#include <sys/wait.h> // for wait
#include <sys/types.h> // for pid_t
#include <fcntl.h> // for open

// 1. handling shell Commands with No Arguments (ls, ps etc)
// 2. this function can also be used to handle commands with arguments

void noArgCommand(char *command[]) {
    pid_t pid = fork();
    if (pid != 0) {
        wait(NULL);
    } else {
        execvp(command[0], command);
        perror("Exec failed"); // If execvp fails
        exit(1);
    }
}

// 3. handling redirecting output like >, 2>
// this can be in different forms too like ls > file.txt, ls 2> file.txt, ls > file.txt 2> file.txt or ls nonexistent_folder > all_output.txt 2>&1
void handleRedirect(char *command[]) {
    pid_t pid = fork();
    if (pid != 0) { 
        wait(NULL);
    } else {
        int fd, i = 0, j = 0;
        char *cleanCommand[10];  

        while (command[i] != NULL) {
            if (strcmp(command[i], ">") == 0) { 
                if (command[i+1] == NULL) {
                    fprintf(stderr, "Error: Missing filename after '>'\n");
                    exit(1);
                }
                fd = open(command[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    perror("File open failed");
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
                i += 2;  // Skip `>` and the filename
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
                i += 2;  // Skip `2>` and the filename
            } else if (strcmp(command[i], "2>&1") == 0) {  
                dup2(STDOUT_FILENO, STDERR_FILENO);
                i++;  // Skip `2>&1`
            } else {
                cleanCommand[j++] = command[i++]; 
            }
        }
        cleanCommand[j] = NULL;  

        execvp(cleanCommand[0], cleanCommand);
        perror("Exec failed");
        exit(1);
    }
}

//4. handle input redirection
void inputRedirect(char *command[]){
    pid_t pid = fork();
    if (pid != 0){
        wait(NULL);
    } else {
        int fd, i = 0, j = 0; // we need both i for going through the command and j for keeping track of the clean command
        char *cleanCommand[50]; // we will store the command without the redirection symbol here
        while(command[i] != NULL){
            if(strcmp(command[i], "<") == 0){
                if(command[i+1] == NULL){
                    fprintf(stderr, "Error: Missing filename after '<'\n");
                    exit(1);
                }
                fd = open(command[i+1], O_RDONLY);
                if(fd < 0){
                    perror("File open failed");
                    exit(1);
                }
                dup2(fd, STDIN_FILENO); 
                close(fd);
                i += 2; // Skip '<' and the filename
            } else {
                cleanCommand[j++] = command[i++];
            }
        }
        cleanCommand[j] = NULL; 

        execvp(cleanCommand[0], cleanCommand); // Execute the actual command
        perror("Exec failed");
        exit(1);
    }
}


// 5. pipes to connect output of one command to input of another command
void handlePipes(char *command[]) {
    int fd[2];
    pipe(fd); 

    pid_t pid1 = fork();
    if (pid1 == 0) {  
        close(fd[0]); 
        dup2(fd[1], STDOUT_FILENO); 
        close(fd[1]);

        char *cmd1[10]; 
        int i = 0;
        while (command[i] != NULL && strcmp(command[i], "|") != 0) {
            cmd1[i] = command[i];
            i++;
        }
        cmd1[i] = NULL; 

        execvp(cmd1[0], cmd1);
        perror("Exec failed");
        exit(1);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {  
        close(fd[1]);  
        dup2(fd[0], STDIN_FILENO); 
        close(fd[0]);

        char *cmd2[10]; // to get the second command (after |)
        int i = 0, j = 0;
        while (command[i] != NULL && strcmp(command[i], "|") != 0) {
            i++;
        }
        i++;  
        while (command[i] != NULL) {
            cmd2[j++] = command[i++];
        }
        cmd2[j] = NULL; 

        execvp(cmd2[0], cmd2);
        perror("Exec failed");
        exit(1);
    }

    //close both ends of the pipe in the parent
    close(fd[0]);
    close(fd[1]);
    wait(NULL);
    wait(NULL);
}



int main() {
    char userInput[100];
    char *command[10];
    int i; // this is used to keep track of the number of arguments in the command

    while (1) {
        printf("$ ");
        fgets(userInput, 100, stdin);

        // Reset i for each command to make sure we start from the beginning after each command
        i = 0;
        command[i] = strtok(userInput, " \n");

        // as long as the command is not NULL, keep tokenizing the command
        while (command[i] != NULL) {
            i++; // increment i to keep track of the number of arguments in the command 
            command[i] = strtok(NULL, " \n");
        }

        // prevent segmentation fault if user just presses enter!
        if (command[0] == NULL) {
            continue;
        }

        if (strcmp(command[0], "exit") == 0) {
            exit(0);
        } 
        // Check for redirection and pipes
        else {
            int redirectFound = 0;
            int inputRedirectFound = 0;
            int pipeFound = 0;

            for (int j = 0; j < i; j++) {
                if (strcmp(command[j], ">") == 0 || strcmp(command[j], "2>") == 0 || strcmp(command[j], "2>&1") == 0) {
                    redirectFound = 1;
                    break;
                }
                else if (strcmp(command[j], "<") == 0) {
                    inputRedirectFound = 1;
                    break;
                }
                else if (strcmp(command[j], "|") == 0) { 
                    pipeFound = 1;
                    break;
                }
            }

            if (pipeFound) {
                handlePipes(command);
            }
            else if (redirectFound) {
                handleRedirect(command);
            }
            else if (inputRedirectFound) {
                inputRedirect(command);
            }
            else {
                noArgCommand(command);
            }
        }
    }
}

