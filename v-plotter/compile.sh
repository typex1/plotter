#!/bin/bash
set -x

gcc v-plotter.c -o v-plotter -I/usr/local/include -L/usr/local/lib -lwiringPi -lm
sudo ./v-plotter
