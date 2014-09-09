wiringHB
========

wiringHB is a wiringPi port for the Hummingboard.

###Donations

donate@pilight.org

###Installation:

* Let it automatically generate a deb package:
```
sudo ./gen.package.sh
sudo dpkg -i libwiringhb*.deb
```
* Or by just compiling and running make install:
```
sudo cmake .
sudo make install
```
Make sure you have cmake in both cases installed:
```
sudo apt-get install cmake
```

Pin numbering is the same as with wiringPi:
https://projects.drogon.net/raspberry-pi/wiringpi/pins/