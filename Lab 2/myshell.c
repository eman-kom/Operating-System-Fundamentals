#include "myshell.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include <fcntl.h>
#include <errno.h>

void info();
int find_pid(pid_t pid);
void wait_pid(pid_t pid);
void terminate(pid_t pid);
bool program_exist(char *token);
int run_command(bool background, char **tokens);
bool run_background(size_t num_tokens, char **tokens);

#define SIZE 55
#define RUNNING INT_MIN
#define TERMINATING INT_MIN+1
#define FILE_NOT_FOUND INT_MIN+3

int pidIdx;
int pids[SIZE][2];

int running;
void fg(pid_t pid);
void sig_handler(int sig);
#define SUSPENDED INT_MIN+5

void my_init(void) {
    pidIdx = 0;
    signal(SIGINT, sig_handler);
    signal(SIGTSTP, sig_handler);
}

void my_process_command(size_t num_tokens, char **tokens) {

    if (strcmp(tokens[0], "info") == 0) {
        info();
    }

    else if (strcmp(tokens[0], "wait") == 0) {
        wait_pid(atoi(tokens[1]));
    }

    else if (strcmp(tokens[0], "fg") == 0) {
        fg(atoi(tokens[1]));
    }

    else if (strcmp(tokens[0], "terminate") == 0) {
        terminate(atoi(tokens[1]));
    } 

    else {

        bool background = run_background(num_tokens, tokens);

        char **ptr = tokens;
        for (size_t i = 0; i < num_tokens; i++) {

            if (tokens[i] == NULL) {
                if (!program_exist(ptr[0])) break;
                run_command(background, ptr);
                break;
            }

            else if (strcmp(tokens[i], "&&") == 0) {
                
                tokens[i] = NULL;
                if (!program_exist(ptr[0])) break;

                int exit = run_command(false, ptr);
                if (exit != 0) {
                    if (exit != FILE_NOT_FOUND) 
                        printf("%s failed\n", ptr[0]);
                    break;
                }

                ptr = tokens+i+1;
            }
        }
    }
}

void my_quit(void) {
    for (int i = 0; i < pidIdx; i++) {
        kill(pids[i][0], SIGCONT);
        kill(pids[i][0], SIGTERM);
        waitpid(pids[i][0], NULL, 0);
    }
    printf("Goodbye!\n");
}

bool run_background(size_t num_tokens, char **tokens) {

    if (strcmp(tokens[num_tokens-2], "&") == 0) {
        tokens[num_tokens-2] = NULL;
        return true;
    }
    return false;
}

bool program_exist(char *token) {

    if (access(token,F_OK) != 0) {
        printf("%s not found\n", token);
        return false;
    }
    return true;
}

int run_command(bool background, char **tokens) {

    int idx = 0;
    int errfd = FILE_NOT_FOUND;
    int readfd = FILE_NOT_FOUND;
    int writefd = FILE_NOT_FOUND;

    while (tokens[idx] != NULL) {

        if (strcmp(tokens[idx], ">") == 0) {
            writefd = creat(tokens[idx+1], 0644);
            tokens[idx] = NULL;
        }

        else if ((strcmp(tokens[idx], "2>") == 0)) {
            errfd = creat(tokens[idx+1], 0644);
            tokens[idx] = NULL;
        }

        else if ((strcmp(tokens[idx], "<") == 0)) {
            readfd = open(tokens[idx+1], O_RDONLY);
            tokens[idx] = NULL;

            if (errno == ENOENT) {
                printf("%s does not exist\n", tokens[idx+1]);
                return FILE_NOT_FOUND;
            }
        }

        idx++;
    }

    pid_t child = fork();

    if (child == 0) {
        if (writefd != FILE_NOT_FOUND) {dup2(writefd, STDOUT_FILENO); close(writefd);}
        if (errfd != FILE_NOT_FOUND) {dup2(errfd, STDERR_FILENO); close(errfd);}
        if (readfd != FILE_NOT_FOUND) {dup2(readfd, STDIN_FILENO); close(readfd);}
        execv(tokens[0], tokens);
    }

    else {

        close(writefd);
        close(errfd);
        close(readfd);

        int current = pidIdx;
        pids[current][0] = child;
        pids[current][1] = RUNNING;
        pidIdx++;

        if (background) {
            printf("Child[%d] in background\n", child);
        } 

        else {
            int stat;
            running = child;
            waitpid(child, &stat, WUNTRACED);
            if (WIFEXITED(stat)) {
                pids[current][1] = WEXITSTATUS(stat);
                return WEXITSTATUS(stat);
            }
        }
    }

    return 0;
}

void info() {

    int stat;
    for (int i = 0; i < pidIdx; i++) {

        if (pids[i][1] == SUSPENDED) {
            printf("[%d] Stopped\n", pids[i][0]);
            continue;
        }

        //running or have not been reaped
        if (pids[i][1] < 0) {

            //running or terminated
            pid_t ret = waitpid(pids[i][0], &stat, WNOHANG | WUNTRACED | WCONTINUED);

            if (ret == 0) {
                char* state = (pids[i][1] == RUNNING) ? "Running" : "Terminating";
                printf("[%d] %s\n", pids[i][0], state);
            }

            else {

                //freshly reaped
                if (WIFEXITED(stat) || (WIFSIGNALED(stat) && WTERMSIG(stat) == SIGTERM)) {
                    pids[i][1] = WEXITSTATUS(stat);
                }

                printf("[%d] Exited %d\n", pids[i][0], pids[i][1]);
            }

        }

        else {
            printf("[%d] Exited %d\n", pids[i][0], pids[i][1]);
        }
    }
}

int find_pid(pid_t pid) {
    for (int i = 0; i < pidIdx; i++) {
        if (pids[i][0] == pid) return i;
    }
    return -1;
}

void wait_pid(pid_t pid) {

    int stat;
    int idx = find_pid(pid);

    if (idx != -1 && pids[idx][1] < 0) {
        running = pid;
        waitpid(pid, &stat, WUNTRACED);
        if (WIFEXITED(stat)) 
            pids[idx][1] = WEXITSTATUS(stat);
    }
}

void terminate(pid_t pid) {

    int idx = find_pid(pid);

    if (idx != -1 && pids[idx][1] < 0) {
        pids[idx][1] = TERMINATING;
        kill(pid, SIGTERM);
    }
}

void sig_handler(int sig) {

    int idx = find_pid(running);
    int stat;

    switch (sig) {
        case SIGINT:
            if (idx != -1 && waitpid(pids[idx][0], NULL, WNOHANG) == 0) {
                killpg(getpgid(pids[idx][0]), SIGINT);
                sleep(1);
                waitpid(pids[idx][0], &stat, WNOHANG);
                if (WIFSIGNALED(stat) && WTERMSIG(stat) == SIGINT)
                    pids[idx][1] = WEXITSTATUS(stat);
                printf("\n[%d] interrupted\n", running);
                running = -1;
            }
            break;

        case SIGTSTP:
            if (idx != -1 && waitpid(pids[idx][0], NULL, WNOHANG) == 0) {
                kill(pids[idx][0], SIGTSTP);
                sleep(1);
                pids[idx][1] = SUSPENDED;
                printf("\n[%d] stopped\n", running);
                running = -1;
            }
            break;
    }
}

void fg(pid_t pid) {
    int idx = find_pid(pid);
    if (idx != -1 && pids[idx][1] == SUSPENDED) {
        tcsetpgrp(STDIN_FILENO, getpgid(pid));
        if (killpg(getpgid(pid), SIGCONT) == 0) {
            pids[idx][1] = RUNNING;
            wait_pid(pids[idx][0]);
        }
    }
}