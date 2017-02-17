
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "stairs.h"
#include "test.hpp"

TlcTest Tlc;

// class members
void TlcTest::update() {
	for (byte i = 0; i < STAIRS_COUNT; i++) {
		printf(" %i ", stair[i]);
	}
	printf("\n");
};

void TlcTest::init() {
	for (byte i = 0; i < STAIRS_COUNT; i++) {
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
	printf("pulse pin %i: ", pin);

	if (pin == 2) {
		printf("sonar1 %s\n", Tlc.is_enabled_sonar1() ? "on" : "off");
		return Tlc.is_enabled_sonar1() ? 1200 : 2000 ;
	}

	// sonar 2
	if (pin == 7) {
		printf("sonar2 %s\n", Tlc.is_enabled_sonar2() ? "on" : "off");
		return Tlc.is_enabled_sonar2() ? 1200 : 2000 ;
	}
	sleep(1);

	printf("nothing trigged\n");
	return 2000;
}

int main()
{
	printf("---------------- Begin Setup ---------------\n");
	setup();
	printf("------------------End Setup ----------------\n\n");
	printf("---------------- Begin Waiting Loop ---------------\n");
	loop();
	printf("---------------- End Waiting Loop ---------------\n\n");
	printf("---------------- Begin Sonar1 Loop ---------------\n");
	Tlc.sonar1enable();
	loop();
	Tlc.sonar1disable();
	sleep(5);
	loop();
	printf("---------------- End Sonar1 Loop ---------------\n\n");
	printf("---------------- Begin Sonar2 Loop ---------------\n");
	Tlc.sonar2enable();
	loop();
	Tlc.sonar2disable();
	sleep(5);
	loop();
	printf("---------------- End Sonar2 Loop ---------------\n\n");
}
