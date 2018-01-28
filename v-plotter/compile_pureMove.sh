#!/bin/bash
set -x

NAM="v-plotter_pureMove_01"

gcc ./${NAM}.c -o ${NAM} -I/usr/local/include -L/usr/local/lib -lwiringPi -lm
sudo ./${NAM}
