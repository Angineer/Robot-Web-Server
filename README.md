# robot-web

A very simple HTTP server for interacting with my service robot. Based on https://github.com/eidheim/Simple-Web-Server.

Dependencies:
- robot-base (https://github.com/Angineer/robot-base)
- `libboost-dev`
- `libboost-system-dev`
- `libboost-thread-dev`
- `libboost-filesystem-dev`

To install, ensure you have the dependencies installed, then build with CMake:

`cd robot-mobile`\
`mkdir build && cd build`\
`cmake ..`\
`make`\
`sudo make install`

There is also a systemd service file included for running the program as a service.

See https://www.adtme.com/projects/Robot.html for more info about the overall project.
