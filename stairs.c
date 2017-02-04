#include <Arduino.h>
#include "Tlc5940.h"

// Направление включения лестницы
typedef enum {
	TO_UP,     // Снизу вверх
	TO_DOWN,   // Сверу вниз
	TO_MIDDLE, // С обоих сторон к середине
	TO_UNKNOWN // Лестница выключена. Дежурная подсветка крайних ступеней.
} direction_t;

struct sonar_opt_t {
	const byte echo;    // pin сонара ловим отраженный сигнал
	const byte reset;   // pin сонара для включения/выключения. (HIGH - включен)
	const byte trigger; // pin сонара посылаем сигнал
};

struct stair_t {
	byte state[12];         // Состояние освещенности ступенек 0..5.
	byte count;             // Количество ступенек
	direction_t direction;  // Направление включения/выключения.
	unsigned long timeout;  // Как долго включена лестница. Когда начинать выключать в миллисекундах.
	byte steplight;         // За сколько шагов зажечь ступеньку до полной яркости
	byte stepwide;          // Ширина шага. Чтобы за все шаги не превысить разрядность на выходе(0..4096)
	byte twilight;          // Дежурная яркость крайних ступеней
	byte wide;              // Максимальная дистанция срабатывания. Примерно 60-70% от ширины лестницы.
};

// Сонар снизу
struct sonar_opt_t sonar1 = {
	echo:2,
	reset:4,
	trigger:8
};

// Сонар сверху
struct sonar_opt_t sonar2 = {
	echo:7,
	reset:5,
	trigger:6
};

struct stair_t stair;

// Здесь будем хранить время когда тушить.
unsigned long wait_to = 0;

static void sync2_real_life();
static void do_init(struct sonar_opt_t *sonar);
static void do_reset(struct sonar_opt_t *sonar);
static void do_action(boolean start);
static boolean is_trigged(struct sonar_opt_t *sonar);

// подготовка
void setup()
{
	// Разберемся с лестницей.
	stair.timeout   = 7000;
	stair.twilight  = 2;
	stair.wide      = 30;
	stair.count     = sizeof(stair.state)/sizeof(*stair.state);
	stair.steplight = 5;
	stair.stepwide  = 4095 / stair.steplight; // с шагом 819
	stair.direction = TO_UNKNOWN;

	for (byte i = 0; i < stair.count; i++)
		stair.state[i] = 0;

	// немного подсветить первую и последнюю ступеньку
	stair.state[0] = stair.twilight;
	stair.state[stair.count - 1] = stair.twilight;

	// инициализация TLC-шки
	Tlc.init();

	//нужно, чтобы предыдущая процедура(инициализация) не "подвесила" контроллер
	delay(1000);

	// Подготавливаем сонары
	do_init(&sonar1);
	do_init(&sonar2);

	// На всякий случай сбросим их
	do_reset(&sonar1);
	do_reset(&sonar2);

	//"пропихнуть" начальные значения яркости ступенек в "реальную жизнь"
	sync2_real_life();
}

// основной воркер
// 1. Если всё выключено, то дождаться включения любого из сонаров. Возможно включение обоих.
// 2. Зажигаем лесницу сверху, снизу, или с обоих концов одновременно. Запоминаем время когда нужно выключить
// 3. Если лесница горит и сработал тот же сонар, то обновляем время выключения.
// 4. Если долго ничего не происходит, то погасить зажженую лестницу и ждать отстоя пены.
// 5. К сожалению, у нас всего два сонара. И мы не можем понять направление движения по лестнице.
//    К примеру, один человек уже идет, а второй начал движение с другого конца. Таким образом мы не можем
//    различать по сонарам начало движения другого человека и завершение движения того кто уже на лестнице.
//    Косвенно можно опираться на таймер. Очевидно, что для прохода по лестнице нужно время. И оно не может
//    быть меньше определенного значения. Будем считать, что оно равно timeout / 2.
//    Если сработал другой датчик до этого времени, то скорее всего это новый человек. иначе завершает движение
//    тот что уже на лестнице.
void loop()
{
	boolean is_finished = (wait_to >= millis()) ? false : true;

	// Всё включено, но прошло недостаточно времени, чтобы дойти до конца.
	boolean need_update = ((wait_to / 2) >= millis()) ? false : true;

	// Проверяем сонары.
	byte status = is_trigged(&sonar1) + (is_trigged(&sonar2) << 1);
	switch (status) {
		case 1: // sonar1
			if (stair.direction == TO_UNKNOWN) {
				// Свеженький. Жжем снизу вверх.
				stair.direction = TO_UP;
				do_action(true);
				wait_to = millis() + stair.timeout;
			} else if (stair.direction == TO_UP) {
				// Сработал два раза подряд. Это точно разные люди.
				// (либо один развернулся на лестнице. Хе-хе..)
				wait_to = millis() + stair.timeout;
			} else if (need_update) {
				// Кто-то пришел с другой стороны раньше чем предыдущий закончил подъем.
				// Продлим горение и поменяем направление.
				stair.direction = TO_UP;
				wait_to = millis() + stair.timeout;
			}
			break;

		case 2: // sonar2
			if (stair.direction == TO_UNKNOWN) {
				// Свеженький. Жжем сверу вниз.
				stair.direction = TO_DOWN;
				do_action(true);
				wait_to = millis() + stair.timeout;
			} else if (stair.direction == TO_DOWN) {
				// Сработал два раза подряд. Это точно разные люди.
				// (либо один развернулся на лестнице. Хе-хе..)
				wait_to = millis() + stair.timeout;
			} else if (need_update) {
				// Кто-то пришел с другой стороны раньше чем предыдущий закончил Спуск.
				// Продлим горение и поменяем направление.
				stair.direction = TO_DOWN;
				wait_to = millis() + stair.timeout;
			}
			break;

		case 3: // Сработали оба сонара
			if (stair.direction == TO_UNKNOWN) {
				// Оппа.. окружают. Жжем с обоих концов.
				stair.direction = TO_MIDDLE;
				do_action(true);
				wait_to = millis() + stair.timeout;
			} else if (stair.direction == TO_MIDDLE) {
				// Сработали оба два раза подряд. тысячи их..
				// Не-е.. в такие чудеса не верим. Скорее они оба завершают движение.
				// Ничо делать не будем.
			} else if (need_update) {
				// Кто-то тупит с обоих сторон. Стоят и общаются через лестницу.
				// Или просто зоопарк.
				// Продлим горение и поменяем направление.
				stair.direction = TO_MIDDLE;
				wait_to = millis() + stair.timeout;
			}
			break;

		case 0: // Глушняк или всё включено. Ждем или тушим лестницу.
		default:
			if (is_finished && stair.direction != TO_UNKNOWN) {
				do_action(false);
				stair.direction = TO_UNKNOWN;
			}
			break;
	}
} // loop

void do_action(boolean start)
{
	// Середина лестницы
	byte middle = stair.count / 2;

	// Реверсивный счетчик для операций в обратном направлении
	byte reverse_step = 0;

	// Издеваемся над всеми ступеньками по очереди
	for (byte i = 0; i < stair.count; i++) {
		reverse_step = stair.count - i - 1;

		// Зажигаем/гасим за steplight число итераций.
		for (byte j = 0; j < stair.steplight; j++) {
			switch (stair.direction) {
				case TO_UP:
					// Жгем снизу
					if (start) {
						if (stair.state[i] < stair.steplight)
							stair.state[i]++;
						break;
					}
					// Гасим крайние до дежурного уровня
					if ((i == 0 || i == (stair.count - 1)) && stair.state[i] > stair.twilight) {
						stair.state[i]--;
						break;
					}
					// Гасим середину
					if (stair.state[i] > 0)
						stair.state[i]--;
					break;

				case TO_DOWN:
					// Жгем сверху
					if (start) {
						if (stair.state[reverse_step] < stair.steplight)
							stair.state[stair.count - i - 1]++;
						break;
					}
					// Гасим крайние до дежурного уровня
					if ((i == 0 || i == (stair.count - 1)) && stair.state[reverse_step] > stair.twilight) {
						stair.state[reverse_step]--;
						break;
					}
					// Гасим середину
					if (stair.state[reverse_step] > 0)
						stair.state[reverse_step]--;
					break;

				case TO_MIDDLE:
					// уже всё горит или уже всё потухло
					if (i > middle) break;

					// Жгем c обоих концов
					if (start) {
						if (stair.state[i] < stair.steplight)
							stair.state[i]++;

						if (stair.state[reverse_step] < stair.steplight)
							stair.state[reverse_step]++;
						break;
					}

					// Гасим из середины к краям
					if ((middle - i) == 0) {
						if (stair.state[0] > stair.twilight) {
							stair.state[0]--;
							stair.state[stair.count - 1]--;
						}
						break;
					}

					// Гасим середину
					if (stair.state[middle - i] > 0) {
						stair.state[middle - i]--;
						stair.state[middle + i]--;
					}
					break;

				default:
					break;
			}

			sync2_real_life();
			delay(100);
		}
	}
}

// Синхронизируем освещение лестницы с состоянием программы.
void sync2_real_life()
{
	for (byte i = 0; i < stair.count; i++) {
		Tlc.set(i, stair.state[i] * stair.stepwide);
	}
	Tlc.update();
	delay(100);
}

// Инициализируем железку на нужные pin-ы
// Поддерживаем высокий импенданс, чтобы включить сонар.
void do_init(struct sonar_opt_t *sonar)
{
	pinMode(sonar->trigger, OUTPUT);
	pinMode(sonar->echo, INPUT);
	pinMode(sonar->reset, OUTPUT);

	// всегда должен быть HIGH, для перезагрузки сонара кратковременно сбросить в LOW
	digitalWrite(sonar->reset, HIGH);
}

// Сбрасываем подвисший сонар - на 100мс "отбираем" у него питание
void do_reset(struct sonar_opt_t *sonar)
{
	digitalWrite(sonar->reset, LOW);
	delay(100);
	digitalWrite(sonar->reset, HIGH);
}

// Ходит ли кто-нибудь?
boolean is_trigged(struct sonar_opt_t *sonar)
{
        digitalWrite(sonar->trigger, LOW);
        delayMicroseconds(5);
        digitalWrite(sonar->trigger, HIGH);
        delayMicroseconds(15);
        digitalWrite(sonar->trigger, LOW);

	// Ждем сигнала не более 5мс
        unsigned int time_us = pulseIn(sonar->echo, HIGH, 5000);

	// Дистанцию определим по времени приема отраженного сигнала.
        unsigned int distance = time_us / 58;

	// Ок. Чья-то нога попала на ступеньку.
	if (distance > 0 && distance <= stair.wide) {
		return true;
	}

	// Похоже сонар завис. Сбросим его и будем считать что никто не ходил.
	if (distance <= 0)
		do_reset(sonar);

	return false;
}
