
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "stairs.h"

TlcTest Tlc;

// class members
void TlcTest::update() {
	for (byte i = 0; i < 16; i++) {
		printf("%" PRIu8 " ", stair[i]);
	}
	printf("\n");
};

void TlcTest::init() {
	for (byte i = 0; i < sizeof(stair)/sizeof(stair[0]); i++) {
		stair[i] = 0;
	}

	sonar1 = false;
	sonar2 = false;
}

// Yet another MOC-s

void delay(useconds_t delay)
{
	usleep(delay);
}

unsigned long millis()
{
	time_t now;
	return (unsigned long)time(&now);
}

void pinMode(const byte pin, const byte mode)
{
//	printf("Set mode '%i' for pin '%" PRIu8 "'\n", mode, pin);
}

void digitalWrite(const byte pin, const byte mode)
{
//	printf("Write mode '%i' for pin '%" PRIu8 "'\n", mode, pin);
}

void delayMicroseconds(useconds_t delay)
{
	usleep(delay);
}

unsigned int pulseIn(const byte pin, const byte mode, useconds_t timeout)
{
	// sonar 1
	if (pin == 8) {
		return Tlc.is_enabled_sonar1() ? 1200 : 2000 ;
	}

	// sonar 2
	if (pin == 6) {
		return Tlc.is_enabled_sonar2() ? 1200 : 2000 ;
	}
	sleep(1);

	return 2000;
}

int main()
{
	printf("---------------- Begin Setup ---------------\n");
	setup();
	printf("------------------End Setup ----------------\n\n");
	sleep(5);
	printf("---------------- Begin Loop ---------------\n");
	loop();
}
