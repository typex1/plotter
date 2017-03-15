Unpack the zip file in the directory /home/pi or in any other path you'd like.

Open a terminal:

change to your home direction e.g.:
cd /home/pi

install wiringPi (if not already installed):
sudo apt-get update
sudo apt-get install git-core
git clone git://git.drogon.net/wiringPi
cd wiringPi
git pull origin
./build
---------- wiringPi installed -------------

Change into the direction of your installation e.g.
cd /home/pi/v-plotter

Compile the program with:
gcc v-plotter.c -o v-plotter -I/usr/local/include -L/usr/local/lib -lwiringPi -lm

start the program with:
sudo ./v-plotter
