#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <pthread.h>

// this struct represents a task in our scheduler, could be either a demo program or shell command
typedef struct Task {
    int task_id;              // unique identifier for each task
    int client_id;            // to track which client submitted this task
    int burst_time;           // total time needed to complete the task
    int remaining_time;       // how much time is left for this task to finish
    int is_shell;            // flag to differentiate between demo (0) and shell commands (1)
    int round_count;         // keeps track of how many rounds this task has been scheduled
    int socket_fd;           // socket to send output back to the client
    int current_iteration;   // for demo tasks: tracks which iteration we're on (0/N, 1/N, etc)
    char command[1024];      // the actual command string to execute
    struct Task* next;       // pointer to next task in our linked list queue
} Task;

// core functions for our scheduler implementation
void init_scheduler();                        // starts up the scheduler thread to process tasks
void shutdown_scheduler();                    // cleanup when we're done (not really used but good practice)

// helper functions to manage tasks
void add_task(const char* command, int client_id, int burst_time, int is_shell);  // basic task addition
void remove_tasks_by_client(int client_id);   // removes all tasks when a client disconnects
void add_task_with_socket(const char* command, int client_id, int burst_time, int is_shell, int socket_fd);  // adds task with socket info

// these need to be accessible from other files
extern pthread_mutex_t queue_mutex;           // mutex to protect our task queue from concurrent access
extern Task* task_queue;                      // head of our task queue linked list

#endif
