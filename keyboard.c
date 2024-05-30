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
#include <termios.h>
#include <semaphore.h>

//definition of the special keys
#define UP 'e'
#define UP_L 'w'
#define UP_R 'r'
#define RIGHT 'f'
#define BRAKE 'd'
#define LEFT 's'
#define DOWN 'c'
#define DOWN_L 'x'
#define DOWN_R 'v'
#define QUIT 'q'

typedef struct{  //shared memory
    double drone_x, drone_y;  //force acting on the drone in the two directions
    int drone_pos_x, drone_pos_y;  //position of the drone inside the window 
} SharedMemory;

SharedMemory* sm;  //shared memory pointer
int exit_value = 0;

pid_t watch_pid = -1;  //declaration pid of the watchdog
void signalhandler(int signo, siginfo_t* info, void* contex){
    if(signo == SIGUSR1){  //SIGUSR1 
        FILE* routine = fopen("files/routine.log", "a");
        watch_pid = info->si_pid;  //initialisation watchdog's pid
        fprintf(routine, "%s\n", "KEYBOARD : command from WATCHDOG");
        kill(watch_pid, SIGUSR1);
        fclose(routine);
    }

    if(signo == SIGUSR2){
        FILE* routine = fopen("files/routine.log", "a");
        fprintf(routine, "%s\n", "KEYBOARD : program terminated by WATCHDOG");
        fclose(routine);
        exit(EXIT_FAILURE);
    }
}

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

int drone_x = 0, drone_y = 0;

void input_handler(char key){  //function to handle the input recieved from the user and updates the shared memory
    FILE* routine = fopen("files/routine.log", "a");

    switch(key){
        case UP:
            drone_y -= 1;
            break;
        case UP_L:
            drone_y -= 1;
            drone_x -= 1;
            break;
        case UP_R:
            drone_y -= 1;
            drone_x += 1;
            break;
        case RIGHT:
            drone_x += 1;
            break;
        case LEFT:
            drone_x -= 1;
            break;
        case BRAKE:
            drone_x = 0;
            drone_y = 0;
            break;
        case DOWN:
            drone_y += 1;
            break;
        case DOWN_L:
            drone_y += 1;
            drone_x -= 1;
            break;
        case DOWN_R:
            drone_y += 1;
            drone_x += 1;
            break;
        case QUIT:
            printf("exiting the program");
            sleep(2);
            kill(watch_pid, SIGUSR2);
            exit_value = 1;
            break;
        default:
            break;
    }
    
    RegToLog(routine, "recieved key");
    sm->drone_x = drone_x;
    sm->drone_y = drone_y;
    fclose(routine);
}

char GetInput(){  //function to acquire the input from the user
    struct termios old, new;
    char button;

    if(tcgetattr(STDIN_FILENO, &old) == -1){
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    new = old;
    new.c_lflag &= ~(ICANON | ECHO);

    if(tcsetattr(STDIN_FILENO, TCSANOW, &new) == -1){
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }

    button = getchar();

    if(tcsetattr(STDIN_FILENO, TCSANOW, &old) == -1){
        perror("tcsetattr2");
        exit(EXIT_FAILURE);
    }

    return button;
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
    RegToLog(routine, "KAYBOARD : start\n");

    struct sigaction sa;  //initialize sigaction
    sa.sa_flags = SA_SIGINFO;  //use sigaction field instead of signalhandler
    sa.sa_sigaction = signalhandler;

    if(sigaction(SIGUSR1, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "KEYBOARD : error in sigaction()");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "KEYBOARD : error in sigaction()");
        exit(EXIT_FAILURE);
    }

    //shared memory opening and mapping
    const char * shm_name = "/shared_memory";
    const int SIZE = 4096;
    int i, shm_fd;
    shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if(shm_fd == 1){
        perror("shared memory faild\n");
        RegToLog(error, "KEYBOARD : shared memory faild");
        exit(EXIT_FAILURE);
    }

    sm = (SharedMemory *)mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(sm == MAP_FAILED){
        perror("map failed");
        RegToLog(error, "KEYBOARD : map memory faild");
        return 1;
    }

    //semaphore
    sem_t* sm_sem;
    sm_sem = sem_open("/sm_sem1", 0);
    if(sm_sem == SEM_FAILED){
        RegToLog(error, "KEYBOARD : semaphore faild");
        perror("semaphore");
    }

    while(!exit_value){
        char ch = GetInput();  //acquire continously the keypad
        sem_wait(sm_sem);
        input_handler(ch);
        sem_post(sm_sem);
    }

    RegToLog(routine, "KEYBOARD : terminated by input");

    //routine to close the shared memory, the files and the semaphore
    if(shm_unlink(shm_name) == 1){
        printf("okok");
        exit(EXIT_FAILURE);
    }
    if(close(shm_fd) == 1){
        perror("close");
        RegToLog(error, "KEYBOARD : close faild");
        exit(EXIT_FAILURE);
    }

    sem_close(sm_sem);

    munmap(sm, SIZE);
    fclose(error);
    fclose(routine);
    return 0;
}