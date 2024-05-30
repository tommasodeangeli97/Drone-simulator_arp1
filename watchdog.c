#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>


#define TIMEOUT 10

typedef enum {FALSE=0, TRUE=1} BOOL;

BOOL server_check, drone_check, keyboard_check;  //variables to indicate if the process are correcly working
pid_t drone_pid, server_pid, keyboard_pid;  //variables to store the pids of the processes

//function to write on the files
void RegToLog(FILE* fname, const char * message){
    time_t act_time;
    time(&act_time);
    int lock_file = flock(fileno(fname), LOCK_EX);
    if(lock_file == -1){
        perror("failed to lock the file");
        return;
    }
    fprintf(fname, "%s : ", ctime(&act_time));
    fprintf(fname, "%s\n", message);
    fflush(fname);

    int unlock_file = flock(fileno(fname), LOCK_UN);
    if(unlock_file == -1){
        perror("failed to unlock the file");
    }
}

void signalhandler(int signo, siginfo_t* info, void* context){
    if(signo == SIGUSR1){
        FILE* routine = fopen("files/routine.log", "a");
        if(routine == NULL){
            perror("fopen");
            exit(EXIT_FAILURE);
        }

        pid_t pid;
        pid = info->si_pid;
        if(pid == drone_pid){
            RegToLog(routine, "WATCHDOG : DRONE signal");
            drone_check = TRUE;
        }
        if(pid == keyboard_pid){
            RegToLog(routine, "WATCHDOG : KEYBOARD signal");
            keyboard_check = TRUE;
        }
        if(pid == server_pid){
            RegToLog(routine, "WATCHDOG : SERVER signal");
            server_check = TRUE;
        }
        fclose(routine);
    }
    if(signo == SIGUSR2){
        printf("terminating the watchdog");
        kill(server_pid, SIGINT);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[]){

    FILE* routine = fopen("files/routine.log", "a");
    FILE* error = fopen("files/error.log", "a");
    if(error == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    if(routine == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    RegToLog(routine, "WATCHDOG : start");

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signalhandler;

    if(sigaction(SIGUSR1, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "WATCHDOG : error in sigaction");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "WATCHDOG : error in sigaction");
        exit(EXIT_FAILURE);
    }

    //take the arguments passed by the master 
    char* inport_serv = argv[1];
    char* inport_drone = argv[2];
    char* inport_key = argv[3];
    drone_pid = atoi(inport_drone);
    server_pid = atoi(inport_serv);
    keyboard_pid = atoi(inport_key);

    while(1){  //it checks if the processes are working sending the SIGUSR1 to all

        server_check = FALSE;
        drone_check = FALSE;
        keyboard_check = FALSE;

        if(kill(server_pid, SIGUSR1) == -1){
            perror("killed server");
            RegToLog(error, "WATCHDOG : error in kill server");
        }
        sleep(1);

        if(kill(drone_pid, SIGUSR1) == -1){
            perror("killed drone");
            RegToLog(error, "WATCHDOG : error in kill drone");
        }
        sleep(1);

        if(kill(keyboard_pid, SIGUSR1) == -1){
            perror("killed keyboard");
            RegToLog(error, "WATCHDOG : error in kill keyboard");
        }
        sleep(1);

        sleep(TIMEOUT);

        if(server_check == FALSE){
            if(kill(server_pid, SIGUSR2) == -1){
                perror("kill server");
                RegToLog(error, "WATCHDOG : error kill server");
            }
            if(kill(drone_pid, SIGUSR2) == -1){
                perror("kill drone");
                RegToLog(error, "WATCHDOG : error kill drone");
            }
            if(kill(keyboard_pid, SIGUSR2) == -1){
                perror("kill keyboard");
                RegToLog(error, "WATCHDOG : error kill keyboard");
            }
            exit(EXIT_FAILURE);
        }
        else
            printf("WATCHDOG : SERVER recieved signal");

        if(drone_check == FALSE){
            if(kill(server_pid, SIGUSR2) == -1){
                perror("kill server");
                RegToLog(error, "WATCHDOG : error kill server");
            }
            if(kill(drone_pid, SIGUSR2) == -1){
                perror("kill drone");
                RegToLog(error, "WATCHDOG : error kill drone");
            }
            if(kill(keyboard_pid, SIGUSR2) == -1){
                perror("kill keyboard");
                RegToLog(error, "WATCHDOG : error kill keyboard");
            }
            exit(EXIT_FAILURE);
        }
        else
            printf("WATCHDOG : DRONE recieved signal");
        
        if(keyboard_check == FALSE){
            if(kill(server_pid, SIGUSR2) == -1){
                perror("kill server");
                RegToLog(error, "WATCHDOG : error kill server");
            }
            if(kill(drone_pid, SIGUSR2) == -1){
                perror("kill drone");
                RegToLog(error, "WATCHDOG : error kill drone");
            }
            if(kill(keyboard_pid, SIGUSR2) == -1){
                perror("kill keyboard");
                RegToLog(error, "WATCHDOG : error kill keyboard");
            }
            exit(EXIT_FAILURE);
        }
        else
            printf("WATCHDOG : KEYBOARD recieved signal");

    }

    fclose(error);
    fclose(routine);
    return 0;
}