# Drone-simulator_arp1
First assignment of Advanced and Robot Programming: is a drone simulator implemented totally in c language and controllable in forces; using the key inputs the user is capable to apply different forces to the drone and see it moving, the application calculates the acceleration, the velocity and the position of the drone and shows it using the *ncurses* library.

## Description
The application consists in five differet processes that collaborate in real time using shared memory and pipes and two files, one for the possible errors and the other that describe the routine of the program.
The final result gives to the user the possibility to move a drone in a free environment where only the friction force and the forces intruduced to control it are acting.
Furthermore the drone is unable to go out the screen.

These are the key to control the robot, however the `master` process visualises them befor starting the application:
```
UP 'e'
UP_LEFT 'w'
UP_RIGHT 'r'
RIGHT 'f'
0 FORCES 'd'
LEFT 's'
DOWN 'c'
DOWN_LEFT 'x'
DOWN_RIGHT 'v'
QUIT 'q'
```

## Architecture
![Slide1](https://github.com/tommasodeangeli97/Drone-simulator_arp1/assets/92479113/bb67539f-e2b3-4413-9e59-c29798f91502)

The structure is given by five processes:

1: **master** -> The process that forks all the others and sends the pids to the watchdog; it gives to the user the information to use properly the application

2: **server** -> The main process, with pipe it gives to the *drone* process the maximum coordinates reaceble for the drone; it recieves from the shared memory the update position of the drone; it creates the window where the drone moves and visualizes the application

3: **keyboard** -> The process that takes the input from the user; it updates the shared memory with the values of the forces acting on the drone

4: **drone** -> The process that controls in fact the drone; it recieves the maximum coordinates from the pipe; it reads from the shared memory the values of the forces and compute the acceleration, the velocity and at the end the new position of the drone, these last information is shared in the shared memory

5: **watchdog** -> This is a control process that verifies if all the other processes are running without errors
