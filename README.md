# Drone-simulator_arp1
First assignment of Advanced and Robot Programming: is a drone simulator implemented totally in c language and controllable in forces; using the key inputs the user is capable to apply different forces to drone and see it moving, the application calculates the acceleration, the velocity and the position od the drone and shows it using the ncurses library.

## Description
The application consists in five differet processes that collaborates in real time using shared memory and pipes and two files, one for the possible errors and the other that describe the routine of the program.
The final result gives to the user the possibility to move a drone in a free environment where only the friction force and the forces intruduced to control it are acting.
Furthermore the drone is unable to go out the screen.

These are the key to control the robot, however the `master` process visualises them befor starting the application:
`UP 'e'
UP_LEFT 'w'
UP_RIGHT 'r'
RIGHT 'f'
0 FORCES 'd'
LEFT 's'
DOWN 'c'
DOWN_LEFT 'x'
DOWN_RIGHT 'v'
QUIT 'q`'
