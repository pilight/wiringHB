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
	
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <setjmp.h>

#include "wiringHB.h"

// Chapter 4, table 4.1 Pin Assignments
#define FUNC_GPIO					0x5

// GPIO 4, 5, 7
#define MX6Q_IOMUXC_BASE_ADDR		0x02000000
// Dual / Quad
#define GPIO_MUX_REG				0x000e0224
#define GPIO_MUX_CTRL				0x000e05f4

// Single
// #define GPIO_MUX_REG				0x000e0210
// #define GPIO_MUX_CTRL				0x000e05e0

#define NUM_PINS					8
#define MAP_SIZE 					1024*1024

static void gc_catch(void);
void wiringHBGC(void);

static int error = 0;

volatile void *gpio = NULL;

static int pinModes[NUM_PINS] = { 0 };
static int sysFds[NUM_PINS] = { 0 };
static int pinsToGPIO[NUM_PINS] = { 73, 72, 71, 70, 194, 195, 67, 1 };
static int pinToBin[NUM_PINS] = { 9, 8, 7, 6, 2, 3, 3, 1 } ;
// Dual / Quad
static int pinToMuxAddr[NUM_PINS] = { 0, 0, 0, 0, 4, 4, 4, 4 };
// Single
// static int pinToMuxAddr[NUM_PINS] = { 0, 0, 0, 0, 48, 48, 48, 48 };

static int pinToGPIOAddr[NUM_PINS] = { 
	0xa4000, 0xa4000, 0xa4000, 0xa4000, // GPIO 0, 1, 2, 3 --> GPIO3_DR
	0xb4000, 0xb4000, // GPIO 4, 5 --> GPIO7_DR
	0xa4000, // GPIO 6 --> GPIO3_DR
	0x9c000 // GPIO 7 --> GPIO1_DR
};

static unsigned short gc_enable = 1;

static void changeOwner(char *file) {
	uid_t uid = getuid();
	uid_t gid = getgid();

	if(chown(file, uid, gid) != 0) {
		if(errno == ENOENT)	{
			fprintf(stderr, "wiringHB: File not present: %s\n", file);
		} else {
			fprintf(stderr, "wiringHB: Unable to change ownership of %s: %s\n", file, strerror (errno));
			error = 1;
			wiringHBGC();
		}
	}
}

int wiringHBSetup(void) {
	int fd = 0;

	gc_catch();
	if((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0) {
		fprintf(stderr, "wiringHB: Unable to open /dev/mem: %s\n", strerror(errno));
		return -1;
	}

	if((int32_t)(gpio = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, MX6Q_IOMUXC_BASE_ADDR)) == -1) {
		fprintf(stderr, "wiringHB: mmap (GPIO) failed: %s\n", strerror (errno));
		close(fd);
		return -1;
	}

	return 0;
}

void pinMode(int pin, int direction) {
	if(!gpio) {
		fprintf(stderr, "wiringHB: Please run wiringHBSetup before running pinMode\n");
		exit(error);
	}
	*((unsigned long *)(gpio + GPIO_MUX_REG + pinToMuxAddr[pin] )) = FUNC_GPIO;
	*((unsigned long *)(gpio + GPIO_MUX_CTRL + pinToMuxAddr[pin] )) = 0xF;

	if(direction == INPUT) {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) = *((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) & ~(1 << pinToBin[pin]);
	} else if(direction == OUTPUT) {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) = *((unsigned long *)(gpio + pinToGPIOAddr[pin] + 4)) | (1 << pinToBin[pin]);
	} else {
		fprintf(stderr, "wiringHB: Pin modes can only be OUTPUT or INPUT\n");
		error = 1;
		wiringHBGC();
	}
	pinModes[pin] = direction;
}

void digitalWrite(int pin, int value) {
	if(pinModes[pin] != OUTPUT) {
		fprintf(stderr, "wiringHB: Trying to write to pin %d, but it's not configured as output\n", pin);
		exit(error);
	}
	if(value) {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin])) = *((unsigned long *)(gpio + pinToGPIOAddr[pin])) | (1 << pinToBin[pin]);
	} else {
		*((unsigned long *)(gpio + pinToGPIOAddr[pin])) = *((unsigned long *)(gpio + pinToGPIOAddr[pin])) & ~(1 << pinToBin[pin]);
	}
}

int digitalRead(int pin) {
	if(pinModes[pin] != INPUT) {
		fprintf(stderr, "wiringHB: Trying to write to pin %d, but it's not configured as input\n", pin);
		exit(error);
	}

	if((*((int *)(gpio + pinToGPIOAddr[pin] + 8)) & (1 << pinToBin[pin])) != 0) {
		return 1;
	} else {
		return 0;
	}
}

int wiringHBISR(int pin, int mode) {
	int i = 0, fd = 0, match = 0, count = 0;
	const char *sMode = NULL;
	char path[30], c;
	pinModes[pin] = SYS;

	if(mode == INT_EDGE_FALLING) {
		sMode = "falling" ;
	} else if(mode == INT_EDGE_RISING) {
		sMode = "rising" ;
	} else if(mode == INT_EDGE_BOTH) {
		sMode = "both";
	} else {
		fprintf(stderr, "wiringHB: Invalid mode. Should be INT_EDGE_BOTH, INT_EDGE_RISING, or INT_EDGE_FALLING\n");
		error = 1;
		wiringHBGC();
	}

	FILE *f = NULL;
	for(i=0;i<NUM_PINS;i++) {
		if(pin == i) {
			sprintf(path, "/sys/class/gpio/gpio%d/value", pinsToGPIO[i]);
			fd = open(path, O_RDWR);
			match = 1;
		}
	}

	if(!match) {
		fprintf(stderr, "wiringHB: Invalid GPIO: %d\n", pin);
		error = 1;
		wiringHBGC();
	}

	if(fd < 0) {
		if((f = fopen("/sys/class/gpio/export", "w")) == NULL) {
			fprintf(stderr, "wiringHB: Unable to open GPIO export interface: %s\n", strerror(errno));
			error = 1;
			wiringHBGC();
		}

		fprintf(f, "%d\n", pinsToGPIO[pin]);
		fclose(f);
	}

	sprintf(path, "/sys/class/gpio/gpio%d/direction", pinsToGPIO[pin]);
	if((f = fopen(path, "w")) == NULL) {
		fprintf(stderr, "wiringHB: Unable to open GPIO direction interface for pin %d: %s\n", pin, strerror(errno));
		error = 1;
		wiringHBGC();
	}

	fprintf(f, "in\n");
	fclose(f);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinsToGPIO[pin]);
	if((f = fopen(path, "w")) == NULL) {
		fprintf(stderr, "wiringHB: Unable to open GPIO edge interface for pin %d: %s\n", pin, strerror(errno));
		error = 1;
		wiringHBGC();
	}

	if(strcasecmp(sMode, "none") == 0) {
		fprintf(f, "none\n");
	} else if(strcasecmp(sMode, "rising") == 0) {
		fprintf(f, "rising\n");
	} else if(strcasecmp(sMode, "falling") == 0) {
		fprintf(f, "falling\n");
	} else if(strcasecmp (sMode, "both") == 0) {
		fprintf(f, "both\n");
	} else {
		fprintf(stderr, "wiringHB: Invalid mode: %s. Should be rising, falling or both\n", sMode);
		error = 1;
		wiringHBGC();
	}

	sprintf(path, "/sys/class/gpio/gpio%d/value", pinsToGPIO[pin]);
	if((sysFds[pin] = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "wiringHB: Unable to open GPIO value interface: %s\n", strerror(errno));
		error = 1;
		wiringHBGC();
	}
	changeOwner(path);

	sprintf(path, "/sys/class/gpio/gpio%d/edge", pinsToGPIO[pin]);
	changeOwner(path);

	fclose(f);

	ioctl(fd, FIONREAD, &count);
	for(i=0; i<count; ++i) {
		read(fd, &c, 1);
	}
	close(fd);

	return 0;
}

int waitForInterrupt(int pin, int mS) {
	int x = 0;
	uint8_t c = 0;
	struct pollfd polls;

	if(pinModes[pin] != SYS) {
		fprintf(stderr, "wiringHB: Trying to read from pin %d, but it's not configured as interrupt\n", pin);
		error = 1;
		wiringHBGC();
	}

	polls.fd = sysFds[pin];
	polls.events = POLLPRI;

	x = poll(&polls, 1, mS);

	(void)read(sysFds[pin], &c, 4);
	lseek(sysFds[pin], 0, SEEK_SET);

	return x;
}

void wiringHBGC(void) {
	int i = 0, fd = 0;
	char path[30];
	FILE *f = NULL;

	for(i=0;i<NUM_PINS;i++) {
		if(pinModes[i] == OUTPUT) {
			pinMode(i, INPUT);
		} else if(pinModes[i] == SYS) {
			sprintf(path, "/sys/class/gpio/gpio%d/value", pinsToGPIO[i]);
			if((fd = open(path, O_RDWR)) > 0) {
				if((f = fopen("/sys/class/gpio/unexport", "w")) == NULL) {
					fprintf(stderr, "wiringHB: Unable to open GPIO unexport interface: %s\n", strerror(errno));
				}

				fprintf(f, "%d\n", pinsToGPIO[i]);
				fclose(f);
				close(fd);
			}
		}
	}

	if(gpio) {
		munmap((void *)gpio, MAP_SIZE);
	}
	gc_enable = 0;
	exit(error);
}

static void gc_handler(int sig) {
	if(((sig == SIGINT || sig == SIGTERM || sig == SIGTSTP) && gc_enable == 1) ||
	  (!(sig == SIGINT || sig == SIGTERM || sig == SIGTSTP) && gc_enable == 0)) {
		wiringHBGC();
		gc_enable = 0;
	}
}

static void gc_catch(void) {
	struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = gc_handler;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGQUIT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    sigaction(SIGABRT, &act, NULL);
    sigaction(SIGTSTP, &act, NULL);

    sigaction(SIGBUS,  &act, NULL);
    sigaction(SIGILL,  &act, NULL);
    sigaction(SIGSEGV, &act, NULL);
    sigaction(SIGFPE,  &act, NULL);
}