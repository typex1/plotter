#!/bin/bash
set -x

vi ./v-plotter.c
mv v-plotter v-plotter.bkp

#svn commit .
git add --all
git commit --all -m "update"
git push

gcc ./v-plotter.c -o v-plotter -I/usr/local/include -L/usr/local/lib -lwiringPi -lm
sudo ./v-plotter
