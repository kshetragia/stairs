
#ifdef IS_TEST
#include "test.hpp"
#else
#include <Arduino.h>
#include "Tlc5940.h"
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
	byte state[16];        // Состояние освещенности ступенек 0..5.
	byte count;            // Количество ступенек
	byte steplight;        // За сколько шагов зажечь ступеньку до полной яркости
	byte stepwide;         // Ширина шага. Чтобы за все шаги не превысить разрядность на выходе(0..4096)
	byte twilight;         // Дежурная яркость крайних ступеней
	byte wide;             // Максимальная дистанция срабатывания. Примерно 60-70% от ширины лестницы.
	unsigned long timeout; // Как долго включена лестница. Когда начинать выключать в миллисекундах.
	enum direction_t direction; // Направление включения/выключения.
};
