#pragma once

#define STAIRS_COUNT 16

#ifndef IS_TEST
#include <Arduino.h>
#include "Tlc5940.h"
#endif

#ifndef byte
#define byte    uint8_t
#endif

#ifndef boolean
#define boolean bool
#endif

// Направление включения лестницы
enum direction_t {
	TO_UP,     // Снизу вверх
	TO_DOWN,   // Сверу вниз
	TO_UNKNOWN // Лестница выключена. Дежурная подсветка крайних ступеней.
};

struct sonar_opt_t {
	const byte echo;    // pin сонара ловим отраженный сигнал
	const byte reset;   // pin сонара для включения/выключения. (HIGH - включен)
	const byte trigger; // pin сонара посылаем сигнал
};

struct stair_t {
	byte state[STAIRS_COUNT];   // Состояние освещенности ступенек 0..5.
	byte steplight;             // За сколько шагов зажечь ступеньку до полной яркости
	unsigned int stepwide;      // Ширина шага. Чтобы за все шаги не превысить разрядность на выходе(0..4095)
	byte twilight;              // Дежурная яркость крайних ступеней
	byte wide;                  // Максимальная дистанция срабатывания. Примерно 60-70% от ширины лестницы.
	unsigned long timeout;      // Как долго включена лестница. Когда начинать выключать в миллисекундах.
	enum direction_t direction; // Направление включения/выключения.
};
