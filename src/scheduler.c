#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <errno.h>
#include "scheduler.h"
#include "parser.h"
#include "executor.h"

// these define our scheduling quantum (time slice) for each round
#define FIRST_ROUND_QUANTUM 3   // first time a task runs, it gets 3 seconds
#define NEXT_ROUND_QUANTUM 7    // subsequent rounds get 7 seconds
#define BUFFER_SIZE 4096        // for reading/writing data

// global variables for our task management
Task* task_queue = NULL;        // our linked list of tasks starts empty
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex to protect the queue
int task_id_counter = 1;        // we start counting tasks from 1

// Function declarations
void execute_shell_command(Task* task);

// this function adds a new task to our queue (basic version without socket)
void add_task(const char* command, int client_id, int burst_time, int is_shell) {
    Task* new_task = (Task*)malloc(sizeof(Task)); // allocate memory for the new task
    new_task->task_id = task_id_counter++;       // increment the task id counter as we create a new task
    new_task->client_id = client_id;
    new_task->burst_time = burst_time;           // total time needed for the task
    new_task->remaining_time = burst_time;       // initially, remaining time equals burst time
    new_task->is_shell = is_shell;
    new_task->round_count = 0;                   // task hasn't run yet
    new_task->socket_fd = -1;                    // no socket in this version
    new_task->current_iteration = 0;             // for demo tasks, start at iteration 0
    strncpy(new_task->command, command, sizeof(new_task->command) - 1);
    new_task->command[sizeof(new_task->command) - 1] = '\0';
    new_task->next = NULL;

    pthread_mutex_lock(&queue_mutex);            // protect the queue while we add the task
    if (task_queue == NULL) {
        task_queue = new_task;                   // if queue is empty, new task becomes head
    } else {
        Task* temp = task_queue;
        while (temp->next != NULL) temp = temp->next;
        temp->next = new_task;                   // add new task to the end of queue
    }
    pthread_mutex_unlock(&queue_mutex);

    printf("[QUEUE] Added Task ID %d (Client #%d), Burst Time: %d, Shell: %d\n",
           new_task->task_id, client_id, burst_time, is_shell);
}

// this function adds a task with socket information (used for remote execution)
void add_task_with_socket(const char* command, int client_id, int burst_time, int is_shell, int socket_fd) {
    Task* new_task = (Task*)malloc(sizeof(Task));
    new_task->task_id = task_id_counter++;
    new_task->client_id = client_id;
    new_task->burst_time = burst_time;
    new_task->remaining_time = burst_time;
    new_task->is_shell = is_shell;
    new_task->round_count = 0;
    new_task->socket_fd = socket_fd;             // store the socket to send results back
    new_task->current_iteration = 0;             // start at iteration 0 for demo tasks
    strncpy(new_task->command, command, sizeof(new_task->command) - 1);
    new_task->command[sizeof(new_task->command) - 1] = '\0';
    new_task->next = NULL;

    pthread_mutex_lock(&queue_mutex);
    if (task_queue == NULL) {
        task_queue = new_task;
    } else {
        Task* temp = task_queue;
        while (temp->next != NULL) temp = temp->next;
        temp->next = new_task;
    }
    pthread_mutex_unlock(&queue_mutex);

    printf("[QUEUE] Added Task ID %d (Client #%d), Burst Time: %d, Shell: %d\n",
           new_task->task_id, client_id, burst_time, is_shell);
}

// removes a specific task from our queue
void remove_task(Task* task) {
    if (!task) return;

    Task* curr = task_queue;
    Task* prev = NULL;

    while (curr) {
        if (curr == task) {
            if (prev == NULL) {
                task_queue = curr->next;         // if it's the first task, update head
            } else {
                prev->next = curr->next;         // skip this task in the linked list
            }
            free(curr);                          // free the memory
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

// removes all tasks for a specific client (used when client disconnects)
void remove_tasks_by_client(int client_id) {
    pthread_mutex_lock(&queue_mutex);            // protect the queue while we modify it
    Task* curr = task_queue;
    Task* prev = NULL;

    while (curr) {
        if (curr->client_id == client_id) {
            Task* to_delete = curr;
            if (prev == NULL) {
                task_queue = curr->next;
                curr = task_queue;
            } else {
                prev->next = curr->next;
                curr = curr->next;
            }
            free(to_delete);
        } else {
            prev = curr;
            curr = curr->next;
        }
    }

    pthread_mutex_unlock(&queue_mutex);
}

// this is our main scheduling loop that runs in a separate thread
void* scheduler_loop(void* arg) {
    while (1) {
        pthread_mutex_lock(&queue_mutex);
        if (task_queue == NULL) {
            pthread_mutex_unlock(&queue_mutex);
            usleep(100000);                      // sleep a bit if no tasks
            continue;
        }

        // select the next task to run (shortest remaining time for demo, priority for shell)
        Task* selected = task_queue;
        Task* curr = task_queue;
        while (curr) {
            if (!curr->is_shell && curr->remaining_time < selected->remaining_time) {
                selected = curr;                  // pick task with least time remaining
            }
            if (curr->is_shell && !selected->is_shell) {
                selected = curr;                  // shell commands get priority
            }
            curr = curr->next;
        }

        // calculate how long this task should run
        int quantum = (selected->round_count == 0) ? FIRST_ROUND_QUANTUM : NEXT_ROUND_QUANTUM;
        int runtime = (selected->is_shell || selected->remaining_time < quantum)
                      ? selected->remaining_time : quantum;

        printf("[SCHEDULER] Running Task ID %d (Client #%d)... Remaining Time: %d, Round: %d\n",
               selected->task_id, selected->client_id, selected->remaining_time, selected->round_count + 1);

        pthread_mutex_unlock(&queue_mutex);

        if (!selected->is_shell) {
            // handle demo command: show progress for N iterations
            int total_iterations = selected->burst_time;
            int iterations_this_round = (runtime > total_iterations - selected->current_iteration) ? 
                                      total_iterations - selected->current_iteration : runtime;
            
            // run for the calculated number of iterations
            for (int i = 0; i < iterations_this_round && selected->current_iteration < total_iterations; i++) {
                char output[BUFFER_SIZE];
                snprintf(output, sizeof(output), "Demo %d/%d\n", selected->current_iteration, total_iterations - 1);
                send(selected->socket_fd, output, strlen(output), 0);
                sleep(1);                        // sleep 1 second between iterations
                selected->current_iteration++;
            }
        } else {
            // handle shell command execution
            execute_shell_command(selected);
        }

        pthread_mutex_lock(&queue_mutex);
        if (!selected->is_shell) {
            selected->remaining_time -= runtime;  // update remaining time for demo tasks
        }
        selected->round_count++;

        // check if task is complete
        if (selected->remaining_time <= 0 || selected->is_shell) {
            printf("[DONE] Task ID %d completed.\n", selected->task_id);
            send(selected->socket_fd, "__TASK_DONE__", strlen("__TASK_DONE__"), 0);
            remove_task(selected);
        } else {
            printf("[PREEMPT] Task ID %d paused, %d seconds remaining\n",
                   selected->task_id, selected->remaining_time);
        }

        pthread_mutex_unlock(&queue_mutex);
    }

    return NULL;
}

// starts up our scheduler thread
void init_scheduler() {
    pthread_t tid;
    pthread_create(&tid, NULL, scheduler_loop, NULL);
    pthread_detach(tid);                         // thread will clean itself up when done
}

// cleanup function (not really used but good practice)
void shutdown_scheduler() {
    // cleanup logic if needed
}

void execute_shell_command(Task* task) {
    char* parsedCommand[50];
    int argCount;
    
    // Parse the command
    parseInput(task->command, parsedCommand, &argCount);
    
    if (parsedCommand[0] == NULL) {
        send(task->socket_fd, "\n", 1, 0);
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        // Check for pipes and redirections
        int pipeFound = 0, redirectFound = 0;
        int inputRedirectFound = 0, outputRedirectFound = 0, errorRedirectFound = 0;

        for (int j = 0; j < argCount; j++) {
            if (strcmp(parsedCommand[j], "|") == 0) pipeFound = 1;
            if (strcmp(parsedCommand[j], "<") == 0) {
                redirectFound = 1;
                inputRedirectFound = 1;
            }
            if (strcmp(parsedCommand[j], ">") == 0) {
                redirectFound = 1;
                outputRedirectFound = 1;
            }
            if (strcmp(parsedCommand[j], "2>") == 0 || strcmp(parsedCommand[j], "2>&1") == 0) {
                redirectFound = 1;
                errorRedirectFound = 1;
            }
        }

        // Execute command based on type
        if (pipeFound && redirectFound) {
            handlePipeRedirect(parsedCommand);
        } else if (pipeFound) {
            int pipeCount = 0;
            for (int j = 0; j < argCount; j++) {
                if (strcmp(parsedCommand[j], "|") == 0) pipeCount++;
            }
            if (pipeCount > 1) {
                handleMultiplePipes(parsedCommand);
            } else {
                handlePipes(parsedCommand);
            }
        } else if (redirectFound) {
            if ((inputRedirectFound && outputRedirectFound) ||
                (inputRedirectFound && errorRedirectFound) ||
                (outputRedirectFound && errorRedirectFound)) {
                handleCombinedRedirect(parsedCommand);
            } else if (inputRedirectFound) {
                inputRedirect(parsedCommand);
            } else {
                handleRedirect(parsedCommand);
            }
        } else {
            // For simple commands, use execvp directly
            execvp(parsedCommand[0], parsedCommand);
            // If execvp fails, print error and exit
            char error_msg[BUFFER_SIZE];
            if (argCount > 1) {
                snprintf(error_msg, sizeof(error_msg), "%s: %s: No such file or directory\n", 
                        parsedCommand[0], parsedCommand[1]);
            } else {
                snprintf(error_msg, sizeof(error_msg), "%s: missing operand\n", parsedCommand[0]);
            }
            write(STDERR_FILENO, error_msg, strlen(error_msg));
            exit(1);
        }
        exit(0);
    } else if (pid > 0) {
        // Parent process
        close(pipefd[1]);

        char buffer[BUFFER_SIZE];
        ssize_t bytes;
        size_t total_bytes = 0;
        
        // Read and send output
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes] = '\0';
            send(task->socket_fd, buffer, bytes, 0);
            total_bytes += bytes;
        }

        close(pipefd[0]);
        
        // Wait for child process
        int status;
        waitpid(pid, &status, 0);

        // Only send error message if no output was received
        if (total_bytes == 0 && WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            // Let the error from the child process handle it
            // Don't send additional error message here
        }
    }
}


