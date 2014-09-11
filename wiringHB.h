/*
	Copyright (c) 2014 CurlyMo <curlymoo1@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef	__WIRING_HB_H__
#define	__WIRING_HB_H__

#ifndef HIGH
	#define HIGH		1
#endif

#ifndef LOW
	#define LOW			0
#endif

#ifndef INPUT
	#define INPUT		0
#endif

#ifndef OUTPUT
	#define OUTPUT		1
#endif

#define SYS			7

#ifndef INT_EDGE_SETUP
	#define	INT_EDGE_SETUP		0
#endif

#ifndef INT_EDGE_FALLING
	#define INT_EDGE_FALLING	1
#endif

#ifndef INT_EDGE_RISING
	#define INT_EDGE_RISING		2
#endif

#ifndef INT_EDGE_BOTH
	#define INT_EDGE_BOTH 		3
#endif

int wiringHBSetup(void);
void pinMode(int pin, int direction);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int wiringHBISR(int pin, int mode);
int waitForInterrupt(int pin, int mS);
void wiringHBGC(void);
int wiringHBI2CRead(int fd);
int wiringHBI2CReadReg8(int fd, int reg);
int wiringHBI2CReadReg16(int fd, int reg);
int wiringHBI2CWrite(int fd, int data);
int wiringHBI2CWriteReg8(int fd, int reg, int data);
int wiringHBI2CWriteReg16(int fd, int reg, int data);
int wiringHBI2CSetup(int devId);

#endif