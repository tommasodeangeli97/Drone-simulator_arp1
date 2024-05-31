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


