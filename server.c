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
#include <ncurses.h>
#include <semaphore.h>


bool sigint_rec = FALSE;

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
        fprintf(routine, "%s\n", "SERVER : started success");
        kill(watch_pid, SIGUSR1);
        fclose(routine);
    }

    if(signo == SIGUSR2){
        FILE* routine = fopen("files/routine.log", "a");
        fprintf(routine, "%s\n", "SERVER : program terminated by WATCHDOG");
        fclose(routine);
        exit(EXIT_SUCCESS);
    }
    if(signo == SIGINT){
        printf("server terminating return 0");
        FILE* routine = fopen("files/routine.log", "a");
        fprintf(routine, "%s\n", "SERVER : terminating");
        fclose(routine);
        sigint_rec = TRUE;
    }
}

int main(int argc, char* argv[]){
    //initialization of ncurses
    initscr();

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
    RegToLog(routine, "SERVER : start\n");
    
    SharedMemory *sm;  //shared memory pointer
    //shared memory opening and mapping
    const char * shm_name = "/shared_memory";
    const int SIZE = 4096;
    int i, shm_fd;
    shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if(shm_fd == 1){
        perror("shared memory faild\n");
        RegToLog(error, "SERVER : shared memory faild");
        exit(EXIT_FAILURE);
    }
    else{
        printf("SERVER : created the shared memory");
        RegToLog(routine, "SERVER : created the shared memory");
    }

    if(ftruncate(shm_fd, SIZE) == 1){
        perror("ftruncate");
        RegToLog(error, "SERVER : ftruncate faild");
        exit(EXIT_FAILURE);
    }

    sm = (SharedMemory *)mmap(0, SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if(sm == MAP_FAILED){
        perror("map failed");
        RegToLog(error, "SERVER : map faild");
        exit(EXIT_FAILURE);
    }

    //semaphore opening
    sem_t * sm_sem;
    sm_sem = sem_open("/sm_sem1", O_CREAT | O_RDWR, 0666, 1);
    if(sm_sem == SEM_FAILED){
        RegToLog(error, "SERVER : semaphore faild");
        perror("semaphore");
    }

    if(has_colors()){
        start_color();  //enables the color
        init_pair(1, COLOR_BLUE, COLOR_WHITE);  //define the window color
    }

    struct sigaction sa;  //initialize sigaction
    sa.sa_flags = SA_SIGINFO;  //use sigaction field instead of signalhandler
    sa.sa_sigaction = signalhandler;

    if(sigaction(SIGUSR1, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "SERVER: error in sigaction()");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGUSR2, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "SERVER : error in sigaction()");
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGINT, &sa, NULL) == -1){
        perror("sigaction");
        RegToLog(error, "SERVER : error in sigaction()");
        exit(EXIT_FAILURE);
    }

    //pipe to give to the drone process the max x and y
    int writesd;
    sscanf(argv[1], "%d", &writesd);

    int max_x, max_y;
    getmaxyx(stdscr, max_y, max_x);  //takes the max number of rows and colons

    write(writesd, &max_x, sizeof(int));
    fsync(writesd);
    sleep(1);
    write(writesd, &max_y, sizeof(int));
    fsync(writesd);

    WINDOW *win = newwin(max_y, max_x, 0, 0);  //creats the window
    box(win, 0, 0);  //adds the borders
    wbkgd(win, COLOR_PAIR(1));  //sets the color of the window
    wrefresh(win);  //prints and refreshes the window

    srand(time(NULL));  //initialise random seed

    int droneposx, droneposy;
    int droneforx, dronefory;

    droneposx = rand() % max_x;  //random column
    droneposy = rand() % max_y;  //random row
    if(droneposx <= 1)
        droneposx = 2;
    if(droneposx >= max_x-3)
        droneposx = max_x-4;
    if(droneposy <= 1)
        droneposy = 2;
    if(droneposy >= max_y)
        droneposy = max_y-1;

    //update the shared memory
    sem_wait(sm_sem);
    sm->drone_pos_x = droneposx;
    sm->drone_pos_y = droneposy;
    sem_post(sm_sem);
    sleep(1);
    
    int r = 0;
    while(!sigint_rec){  //print the drone in the new position given by the shared memory and delet the old one
        if(r >0){
            wattr_on(win, COLOR_PAIR(1), NULL);
            mvwprintw(win, droneposy, droneposx, "   ");  //prints the drone
            wattr_off(win, COLOR_PAIR(1), NULL);
            wrefresh(win);
            
            r--;
        }

        sem_wait(sm_sem); 
        droneposx = sm->drone_pos_x;
        droneposy = sm->drone_pos_y;
        droneforx = sm->drone_x;
        dronefory = sm->drone_y;
        sem_post(sm_sem);

        wattr_on(win, COLOR_PAIR(1), NULL);
        mvwprintw(win, max_y-1, 1, "force x:%d, force y:%d", droneforx, dronefory);
        mvwprintw(win, droneposy, droneposx, "}o{");  //prints the drone
        wattr_off(win, COLOR_PAIR(1), NULL);
        wrefresh(win);
        
        sleep(1);
        r++;
    }

    //routine to close the shared memory, the files and the semaphore
    if(shm_unlink(shm_name) == 1){
        printf("okok");
        exit(EXIT_FAILURE);
    }
    if(close(shm_fd) == 1){
        perror("close");
        RegToLog(error, "SERVER : close faild");
        exit(EXIT_FAILURE);
    }

    sem_close(sm_sem);
    munmap(sm, SIZE);
    fclose(error);
    fclose(routine);

    return 0;
}

