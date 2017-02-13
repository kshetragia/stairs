#include "stairs.h"

// Почему-то этот блок не может быть обработан корректно в IDE если поместить его в stairs.h
#ifdef IS_TEST
#include "test.hpp"
#else
#include <Arduino.h>
#include "Tlc5940.h"
#endif

#include <stdio.h>

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
	stair.twilight  = 2;
	stair.wide      = 30;
	stair.count     = sizeof(stair.state)/sizeof(*stair.state);
	stair.steplight = 4;
	stair.stepwide  = 4095 / stair.steplight; // с шагом 1024
	stair.direction = TO_UNKNOWN;

	// Нужно помнить, что человек наступил на лестницу и она уже начала загораться.
	// Значит нам нет смысла долго ждать когда он пройдет. Средний шаг может длиться секунду-полторы на ступеньку.
	// Но это очень долго. Обычно гораздо быстрее. С учетом времени включения лестницы нет смысла ждать больше чем полсекунды на ступеньку.
	// Или даже быстрее. Скорость загорания 16(ступенек) * 100мс * 3(steplight). ~= 4.8сек Плюс она ещё гаснет столько же.
	// итого timeout выбран равным четверти от числа ступенек. хотя лучше чтобы был константной величиной.
	stair.timeout   = stair.count / 4;

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

	// Поприветствуем хозяина
	for (byte i = 0; i < stair.steplight; i++) {
		for (byte j = 0; j < stair.count; j++) {
			stair.state[j] = i;
		}
		sync2_real_life();
	}

	delay(1000);

	for (byte i = 0; i < stair.steplight; i++) {
		for (byte j = 0; j < stair.count; j++)
			stair.state[j] = stair.steplight - i;
		sync2_real_life();
	}

	for (byte i = 0; i < stair.count; i++)
		stair.state[i] = 0;
	sync2_real_life();

	// Переход в рабочий режим. Немного подсветить первую и последнюю ступеньку
	stair.state[0] = stair.twilight;
	stair.state[stair.count - 1] = stair.twilight;

	//"пропихнуть" начальные значения яркости ступенек в "реальную жизнь"
	sync2_real_life();
}

// основной воркер
// 1. Если всё выключено, то дождаться включения любого из сонаров. Возможно включение обоих.
// 2. Зажигаем лесницу сверху или снизу. Запоминаем время когда нужно выключить
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
		case 3: // Или сработали оба сонара(редкий случай. Ведь датчика ждем всего 5мс, к тому же по очереди.)
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
	// Реверсивный счетчик для операций в обратном направлении
	byte reverse_step = 0;

	// Будем мигать крайней ступенькой с той стороны куда уже зажигается лестница.
	// Типа ждите, за вами выехали. Мигать будем в силу дежурного освещения.
	boolean blink = false;

	// Издеваемся над всеми ступеньками по очереди
	for (byte i = 0; i < stair.count; i++) {
		reverse_step = stair.count - i - 1;

		// Мигаем через раз так как ступенька зажигается всего за 300..400мс.
		if (i % 2 == 0) {
			if ((start && stair.direction == TO_UP) || (!start && stair.direction == TO_DOWN)) {
				// Мигаем верхней ступенькой когда идем вверх или тушим вниз
				stair.state[stair.count - 1] = blink * stair.twilight;
			} else if ((start && stair.direction == TO_DOWN) || (!start && stair.direction == TO_UP)) {
				// Мигаем нижней ступенькой когда идем или тушим вниз
				stair.state[0] = blink * stair.twilight;
			}
		}

		// Зажигаем/гасим за steplight число итераций.
		for (byte j = 0; j < stair.steplight; j++) {
			switch (stair.direction) {
				case TO_UP:
					// Жгем снизу вверх
					if (start) {
						if (stair.state[i] < stair.steplight)
							stair.state[i]++;
						break;
					}

					// Гасим снизу вверх
					if (stair.state[i] > 0)
						stair.state[i]--;
					break;

				case TO_DOWN:
					// Жгем сверху вниз
					if (start) {
						if (stair.state[reverse_step] < stair.steplight)
							stair.state[reverse_step]++;
						break;
					}

					// Гасим сверху вниз
					if (stair.state[reverse_step] > 0)
						stair.state[reverse_step]--;
					break;

				default: // че-то не то делаем
					break;
			}
			sync2_real_life();
		}

		if ( i % 2 == 0)
			blink ^= blink;
	}

	// Лестница погашена. Включаем дежурное освещение.
	if (stair.direction == TO_DOWN) {
		stair.state[0] = stair.twilight;
		stair.state[stair.count - 1] = stair.twilight;
	}

	// Долго мигали. Включим последнюю ступеньку если она оказалась выключенной
	if (stair.direction == TO_UP)
		stair.state[stair.count - 1] = stair.twilight;

	sync2_real_life();
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
