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
#include <sys/ipc.h>
#include <semaphore.h>


#define MASS 1
#define T_STEP 0.1
#define VISCOSITY 0.1
#define FORCE 25

typedef struct{  //shared memory
    double drone_x, drone_y;  //force acting on the drone in the two directions
    int drone_pos_x, drone_pos_y;  //position of the drone inside the window 
} SharedMemory;

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

pid_t watch_pid = -1;  //declaration pid of the watchdog

void signalhandler(int signo, siginfo_t* info, void* contex){
    if(signo == SIGUSR1){  //SIGUSR1 
        FILE* routine = fopen("files/routine.log", "a");
        watch_pid = info->si_pid;  //initialisation watchdog's pid
        fprintf(routine, "%s\n", "DRONE : started success");
        kill(watch_pid, SIGUSR1);
        fclose(routine);
    }

    if(signo == SIGUSR2){
        FILE* routine = fopen("files/routine.log", "a");
        fprintf(routine, "%s\n", "DRONE : program terminated by WATCHDOG");
        fclose(routine);
        exit(EXIT_FAILURE);
    }
}

double acceleration(int force){  //function to find the acceleration
    return force / MASS;
}

double velocity(double acc, double vel){
    return vel + acc * T_STEP - (vel * VISCOSITY / MASS) * T_STEP;  //the velocity is given by the inizitial velocity + acceleration*time - the viscosity*velocity
}

int position(int pos, double vel){
    return (int)round(pos + vel * T_STEP);  //position is given by actual position + velocity*time
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
    RegToLog(routine, "DRONE : start\n");

    SharedMemory *sm;  //shared memory pointer

    //shared memory opening and mapping
    const char * shm_name = "/shared_memory";
    const int SIZE = 4096;
    int i, shm_fd;
    shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if(shm_fd == -1){
        perror("shared memory faild\n");
        RegToLog(error, "DRONE : shared memory faild");
        exit(EXIT_FAILURE);
    }

    sm = (SharedMemory *)mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(sm == MAP_FAILED){
        perror("map failed");
        RegToLog(error, "DRONE : map faild");
        return 1;
    }

    //semaphore for the shared memory
    sem_t* sm_sem;
    sm_sem = sem_open("/sm_sem1", 0);
    if(sm_sem == SEM_FAILED){
        RegToLog(error, "DRONE : semaphore faild");
        perror("semaphore");
    }

    struct sigaction sa;  //initialize sigaction
    sa.sa_flags = SA_SIGINFO;  //use sigaction field instead of signalhandler
    sa.sa_sigaction = signalhandler;

    if(sigaction(SIGUSR1, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "DRONE: error in sigaction()");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "DRONE : error in sigaction()");
        exit(EXIT_FAILURE);
    }

    //use of pipe to take from the server the max of x and y
    int readsd;
    int varre;
    int maxx, maxy;
    sscanf(argv[1], "%d", &readsd);

    varre = read(readsd, &maxx, sizeof(int));
    if( varre == -1){
        perror("readsd");
        RegToLog(error, "DRONE : error in readsd");
    }
    sleep(1);
    varre = read(readsd, &maxy, sizeof(int));
    if( varre == -1){
        perror("readsd");
        RegToLog(error, "DRONE : error in readsd");
    }

    int x = 0;
    int y = 0;
    double accx, accy;
    double velx = 0.0;
    double vely = 0.0;
    int posx, posy;
    int forx, fory;

    x = sm->drone_pos_x;
    y = sm->drone_pos_y;

    sleep(1);

    while(1){  //takes from the keyboard the force acting on the drone and calculates the accelleration, the velocity and the position

        sem_wait(sm_sem);
        forx = sm->drone_x;
        fory = sm->drone_y;
        sem_post(sm_sem);

        accx = acceleration(forx*FORCE);
        velx = velocity(accx, velx);
        posx = position(x, velx);
        //the drone can't go out of the screen
        if(posx >= maxx-3){
            x = maxx-4;
            RegToLog(routine, "DRONE : x lower limit");
        }
        else if(posx <= 1){
            x = 2;
            RegToLog(routine, "DRONE : x upper limit");
        }
        else{
            x = posx;
        }
        
        accy = acceleration(fory*FORCE);
        vely = velocity(accy, vely);
        posy = position(y, vely);
        //the drone can't go out of the screen
        if(posy >= maxy){
            y = maxy-2;
            RegToLog(routine, "DRONE : y lower limit");
        }
        else if(posy <= 1){
            y = 2;
            RegToLog(routine, "DRONE : y upper limit");
        }
        else{
            y = posy;
        }
        
        //updates the shared memory
        sem_wait(sm_sem);
        sm->drone_pos_x = x;
        sm->drone_pos_y = y;
        sem_post(sm_sem);       
        fprintf(routine, "%d %d\n", x, y);
        sleep(1);
    }

    //routine to close the shared memory, the files and the semaphore
    if(shm_unlink(shm_name) == 1){
        printf("okok");
        exit(EXIT_FAILURE);
    }
    if(close(shm_fd) == 1){
        perror("close");
        RegToLog(error, "DRONE : close faild");
        exit(EXIT_FAILURE);
    }

    sem_close(sm_sem);
    munmap(sm, SIZE);
    fclose(error);
    fclose(routine);

    return 0;
}