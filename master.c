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

//function to start the programs and return the pid
int spawn(const char * program, char ** arg_list){
    FILE* error = fopen("files/error.log", "a");
    pid_t child_pid = fork();
    if(child_pid != 0)
        return child_pid;
    else{
        execvp(program, arg_list);
        perror("exec failed");
        RegToLog(error, "MASTER: execvp failed");
        exit(EXIT_FAILURE);
    }
    fclose(error);
}

int main(int argc, char* argv[]){

    FILE* routine = fopen("files/routine.log", "a");
    FILE* error = fopen("files/error.log", "w");
    if(error == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    if(routine == NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    RegToLog(routine, "MASTER : started");

    //pid for the programs
    pid_t server, drone, key, watchdog;

    int pipe_sd[2]; //pipe from server to drone
    if(pipe(pipe_sd) == -1){
        perror("error in pipe_sd");
        RegToLog(error, "MASTER : error in opening pipe_sd");
    }

    char piperd2[10]; //readable pipe for server-drone
    char pipewr2[10]; //writtable pipe for server-drone
    
    sprintf(piperd2, "%d", pipe_sd[0]);
    sprintf(pipewr2, "%d", pipe_sd[1]);

    //process path
    char * drone_path[] = {"./drone", piperd2, NULL};
    char * key_path[] = {"./keyboard", NULL};
    char * server_path[] = {"./server", pipewr2, NULL};

    int i = 0;
    char button;

    while(i < 2){  //just gives the initial information for the application
        if(i == 0){
            printf("\t\tWELCOME TO DRONE SIMULATOR BY Tommaso De Angeli\n\n");
            printf("\t\tfirst assignment of Advance and Robot Programming\n\n");
            printf("press q to stop the simulation or any other button to continue...\n\n\n\n");
            scanf("%c", &button);
            if(button == 'q'){
                RegToLog(routine, "MASTER : end by user");
                exit(EXIT_FAILURE);
            }
            else
                i++;
        }
        if(i == 1){
            printf("\t\tKEYS INSTRUCTIONS\n");
            printf("\tUP 'e'\n");
            printf("\tUP_LEFT 'w'\n");
            printf("\tUP_RIGHT 'r'\n");
            printf("\tRIGHT 'f'\n");
            printf("\t0 FORCES 'd'\n");
            printf("\tLEFT 's'\n");
            printf("\tDOWN 'c'\n");
            printf("\tDOWN_LEFT 'x'\n");
            printf("\tDOWN_RIGHT 'v'\n");
            printf("\tQUIT 'q'\n\n\n");
            printf("\t\tOK, LET'S START!!");
            sleep(10);
            i++;
                
        }
        
    }

    //execute the programs
    server = spawn("./server", server_path);
    usleep(500000);
    key = spawn("./keyboard", key_path);
    usleep(500000);
    drone = spawn("./drone", drone_path);
    usleep(500000);
    
    pid_t pids[] = {server, drone, key};
    char pidsstring[3][50];

    //insert all pids inside the pidsstring
    for(size_t i = 0; i < sizeof(pids)/sizeof(pids[0]); i++){
        sprintf(pidsstring[i], "%d", pids[i]);
    }

    //put in the watchdog path all the pids of the other processes
    char* watch_path[] = {"./watchdog", pidsstring[0], pidsstring[1], pidsstring[2], NULL};
    sleep(1);
    watchdog = spawn("./watchdog", watch_path);

    //wait the finish of all the processes
    for(int n = 0; n < 4; n++){
        wait(NULL);
    }

    RegToLog(routine, "MASTER : finish");

    close(pipe_sd[0]);
    close(pipe_sd[1]);
    //close the file
    fclose(error);
    fclose(routine);

    return 0;
}