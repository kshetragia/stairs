#pragma once

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <time.h>
#include <unistd.h>
#include <stdio.h>

#define byte    uint8_t
#define boolean bool

#define INPUT   1
#define OUTPUT  2
#define HIGH    3
#define LOW     4

struct TlcTest
{
	void init();
	void update();

	void set(byte pin, byte mode) { stair[pin] = mode; };

	void sonar1enable()  { sonar1 = true; }
	void sonar1disable() { sonar1 = false; }

	void sonar2enable()  { sonar2 = true; }
	void sonar2disable() { sonar2 = false; }

	bool is_enabled_sonar1() { return sonar1; }
	bool is_enabled_sonar2() { return sonar2; };

private:
	byte stair[16];

	bool sonar1;
	bool sonar2;
};

extern TlcTest Tlc;

void setup();

void loop();

void delay(useconds_t delay);

unsigned long millis();

void pinMode(const byte pin, const byte mode);

void digitalWrite(const byte pin, const byte mode);

void delayMicroseconds(useconds_t delay);

unsigned int pulseIn(const byte pin, const byte mode, useconds_t timeout);
