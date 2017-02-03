#include <Arduino.h>
#include "Tlc5940.h"

typedef struct {
    const byte echo;
    const word inactive_time;
    const byte mindist;
    const byte reset;
    const byte trigger;
    byte ignore;
    unsigned long prev_time;  //время начала блокировки сонара inactive time миллисекунд
} sonar_opt_t;

sonar_opt_t sonar1 = {
    echo:2,
    inactive_time:1500,
    mindist:30,
    reset:4,
    trigger:8
};

sonar_opt_t sonar2 = {
    echo:7,
    inactive_time:1500,
    mindist:30,
    reset:5,
    trigger:6
};

sonar_opt_t *sonar1_p = NULL;
sonar_opt_t *sonar2_p = NULL;

const byte stairsCount = 12;         // количество ступенек
const byte initialPWMvalue = 2;      // only 0...5 (яркость первой и последней ступенек в ожидании)
const word waitForTurnOff = 7000;    // мс, время задержки подсветки во вкл состоянии после включения последней ступеньки

byte stairsArray[stairsCount];       // массив, где каждый элемент соответствует ступеньке. Все операции только с ним, далее sync2realLife
byte direction = 0;                  // 0 = снизу вверх, 1 = сверху вниз

boolean allLEDsAreOn = false;           // флаг, все светодиоды включены, требуется ожидание в таком состоянии
boolean need2LightOnBottomTop = false;  // флаг, требуется включение ступеней снизу вверх
boolean need2LightOffBottomTop = false; // флаг, требуется выключение ступеней снизу вверх
boolean need2LightOnTopBottom = false;  // флаг, требуется включение ступеней сверху вниз
boolean need2LightOffTopBottom = false; // флаг, требуется выключение ступеней сверху вниз
boolean sonar1trigged = false;          // флаг, участвующий в реакциях на сратабывание сонара в разных условиях
boolean sonar2trigged = false;          // флаг, участвующий в реакциях на сратабывание сонара в разных условиях
boolean nothingHappening = true;        // флаг, указывающий на "дежурный" режим лестницы, т.н. исходное состояние

unsigned long allLEDsAreOnTime;   //время начала горения ВСЕХ ступенек

static void sync2RealLife();
static void sonarPrepare(sonar_opt_t *sonar);
static void sonarDisable(sonar_opt_t *sonar);
static void startBottomTop();
static void stopBottomTop();
static void startTopBottom();
static void stopTopBottom();

static boolean sonarTrigged(sonar_opt_t *sonar);

// подготовка
void setup()
{
    sonar1_p = &sonar1;
    sonar2_p = &sonar2;

   //забить массив начальными значениями яркости ступенек
   for (byte i = 1; i <= stairsCount-2; i++)
       stairsArray[i] = 0;

   //выставление дефолтной яркости первой ступеньки
   stairsArray[0] = initialPWMvalue;

   //выставление дефолтной яркости последней ступеньки
   stairsArray[stairsCount-1] = initialPWMvalue;

   // инициализация TLC-шки
   Tlc.init();

   //нужно, чтобы предыдущая процедура (инициализация) не "подвесила" контроллер
   delay(1000);

   //"пропихнуть" начальные значения яркости ступенек в "реальную жизнь"
   sync2RealLife();

   // подготавливаем сонары
   sonarPrepare(sonar1_p);
   sonarPrepare(sonar2_p);
}

// основной воркер
void loop()
{
    sonar1trigged = sonarTrigged(sonar1_p); //выставление флага сонара 1 для последующих манипуляций с ним
    sonar2trigged = sonarTrigged(sonar2_p); //выставление флага сонара 2 для последующих манипуляций с ним

    nothingHappening = !((need2LightOnTopBottom)||(need2LightOffTopBottom)||(need2LightOnBottomTop)||(need2LightOffBottomTop)||(allLEDsAreOn));

    //если лестница находится в исходном(выключенном) состоянии, можно сбросить флаги-"потеряшки" на всякий случай
    if (nothingHappening) {
	sonar1_p->ignore = 0;
	sonar2_p->ignore = 0;
    }

    //процесс включения относительно сложен, нужно проверять кучу условий

    //процесс ВКЛючения: сначала - снизу вверх (выставление флагов и счетчиков)
    if ((sonar1trigged) && (nothingHappening)) {
        need2LightOnBottomTop = true; //начать освещение ступенек снизу вверх
        sonar2_p->ignore++;  //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
    }
    else if ((sonar1trigged) && ((need2LightOnBottomTop)||(allLEDsAreOn))) {
        //если ступеньки уже загоряются в нужном направлении или уже горят
        sonarDisable(sonar1_p); //просто увеличить время ожидания полностью включенной лестницы снизу вверх
        allLEDsAreOnTime = millis();
        sonar2_p->ignore++;  //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
        direction = 0; //направление - снизу вверх
    }
    else if ((sonar1trigged) && (need2LightOffBottomTop)) {
        // а уже происходит гашение снизу вверх
        need2LightOffBottomTop = false; //прекратить гашение ступенек снизу вверх
        need2LightOnBottomTop = true;   //начать освещение ступенек снизу вверх
        sonar2_p->ignore++;  //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
    }
    else if ((sonar1trigged) && (need2LightOnTopBottom)){
        //а уже происходит освещение сверху вниз
        need2LightOnTopBottom = false; //прекратить освещение ступенек сверху вниз
        need2LightOnBottomTop = true;  //начать освение ступенек снизу вверх
        sonar2_p->ignore++;  //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
    }
    else if ((sonar1trigged) && (need2LightOffTopBottom)){
        //а уже происходит гашение сверху вниз
        need2LightOffTopBottom = false; //прекратить гашение ступенек сверху вниз
        need2LightOnBottomTop = true;   //начать освение ступенек снизу вверх
        sonar2_p->ignore++;  //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
    }

    //процесс ВКЛючения: теперь - сверху вниз (выставление флагов и счетчиков)
    if ((sonar2trigged) && (nothingHappening)){
        //простое включение ступенек сверху вниз из исходного состояния лестницы
        need2LightOnTopBottom = true;//начать освещение ступенек сверху вниз
        sonar1_p->ignore++; //игнорить противоположный сонар, чтобы при егосрабатывании не запустилось "загорание" снизу вверх
    }
    else if ((sonar2trigged) && ((need2LightOnTopBottom)||(allLEDsAreOn))) {
        //если ступеньки уже загоряются в нужном направлении или уже горят
        sonarDisable(sonar2_p);//обновить отсчет времени для освещения ступенек сверху вниз
        allLEDsAreOnTime = millis();
        sonar1_p->ignore++; //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
        direction = 1;//направление - сверху вниз
    }
    else if ((sonar2trigged) && (need2LightOffTopBottom)) {
        //а уже происходит гашение сверху вниз
        need2LightOffTopBottom = false; // прекратить гашение ступенек сверху вниз
        need2LightOnTopBottom = true;   // начать освещение ступенек сверху вниз
        sonar1_p->ignore++; //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
    }
    else if ((sonar2trigged) && (need2LightOnBottomTop)) {
        //а уже происходит освещение снизу вверх
        need2LightOnBottomTop = false; // прекратить освещение ступенек снизу вверх
        need2LightOnTopBottom = true;  // начать освение ступенек сверху вних
        sonar1_p->ignore++; //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
    }
    else if ((sonar2trigged) && (need2LightOffBottomTop)) {
        // а уже происходит гашение снизу вверх
        need2LightOffBottomTop = false; // прекратить гашение ступенек снизу вверх
        need2LightOnTopBottom = true;   // начать освение ступенек сверху вниз
        sonar1_p->ignore++; //игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
    }

    //процесс ВЫКлючения относительно прост - нужно только знать направление, и выставлять флаги
    if ((allLEDsAreOn)&&((allLEDsAreOnTime + waitForTurnOff) <= millis())) {
        //пора гасить ступеньки в указанном направлении
        if (direction == 0) {
            need2LightOffBottomTop = true; // снизу вверх
        } else {
            need2LightOffTopBottom = true; // сверху вниз
        }
    }

    //непосредственная обработка флагов с "пропихиванием" массива ступенек в "реальность"
    if (need2LightOnBottomTop){
       //увеличим яркость за 4 итерации, уложившись в 400мс (BottomTop - снизу вверх)
       for (byte i=0; i<=3;i++) {
           startBottomTop();
           sync2RealLife();
           delay(100);
       }
    }

    if (need2LightOffBottomTop) {
       //уменьшим яркость за 4 итерации, уложившись в 400мс (BottomTop - снизу вверх)
       for (byte i=0; i<=3;i++) {
           stopBottomTop();
           sync2RealLife();
           delay(100);
       }
    }

    if (need2LightOnTopBottom) {
       //увеличим яркость за 4 итерации, уложившись в 400мс (TopBottom - сверху вниз)
       for (byte i=0; i<=3;i++) {
           startTopBottom();
           sync2RealLife();
           delay(100);
       }
    }

    if (need2LightOffTopBottom) {
       // уменьшим яркость за 4 итерации, уложившись в 400мс (TopBottom - сверху вниз)
       for (byte i=0; i<=3;i++) {
           stopTopBottom();
           sync2RealLife();
           delay(100);
       }
    }
} // loop

//процедура ВКЛючения снизу вверх
void startBottomTop()
{
   for (byte i=1; i<=stairsCount; i++) {
       //обработка всех ступенек по очереди, добавление по "1" яркости для одной ступеньки за раз
       if (stairsArray[i-1] <=4) {
          //узнать, какой ступенькой сейчас заниматься
          stairsArray[i-1]++; //увеличить на ней яркость
          return;
       }
       else if ((i == stairsCount) && (stairsArray[stairsCount-1] == 5) && (!allLEDsAreOn)) {
          //если полностью включена последняя требуемая ступенька
          allLEDsAreOnTime = millis(); // сохраним время начала состояния "все ступеньки включены"
          allLEDsAreOn = true; // флаг, все ступеньки включены
          direction = 0; // для последующего гашения ступенек снизу вверх
          need2LightOnBottomTop = false; // поскольку шаг - последний, сбрасываем за собой флаг необходимости
          return;
       }
   }
}

//процедура ВЫКЛючения снизу вверх
void stopBottomTop()
{
   if (allLEDsAreOn) allLEDsAreOn = false; //уже Не все светодиоды включены, очевидно

   for (byte i=0; i<=stairsCount-1; i++) {
       //пытаемся перебрать все ступеньки по очереди
       if ((i == 0)&&(stairsArray[i] > initialPWMvalue)) {
           //если ступенька первая, снижать яркость до "дежурного" уровня ШИМ, а не 0
           stairsArray[0]--; //снизить яркость
           return;
       }
       else if ((i == stairsCount-1) && (stairsArray[i] > initialPWMvalue)) {
           //если последняя, то снижать яркость до дежурного уровня ШИМ, а не 0
           stairsArray[i]--;//снизить яркость

           //если это последняя ступенька и на ней достигнута минимальная яркость
           if (stairsArray[stairsCount-1] == initialPWMvalue) need2LightOffBottomTop = false;

           return;
       }
       else if ((i != 0) && (i != (stairsCount-1)) && (stairsArray[i] >= 1)) {
           //обработка всех остальных ступенек
           stairsArray[i]--;//снизить яркость
           return;
       }
   }
}

//процедура ВКЛючения сверху вниз
void startTopBottom()
{
   for (byte i=stairsCount; i>=1; i--) {
       //обработка всех ступенек по очереди, добавление по "1" яркости для одной ступеньки за раз
       if (stairsArray[i-1] <=4) {
           // узнать, какой ступенькой сейчас заниматься
           stairsArray[i-1]++;//уменьшить на ней яркость
           return;
       }
       else if ((i == 1) && (stairsArray[0] == 5) && (!allLEDsAreOn)) {
           //если полностью включена последняя требуемая ступенька
           allLEDsAreOnTime = millis();//сохраним время начала состояния "все ступеньки включены"
           allLEDsAreOn = true;//флаг, все ступеньки включены
           direction = 1;//для последующего гашения ступенек снизу вверх
           need2LightOnTopBottom = false; // поскольку шаг - последний, сбрасываем за собой флаг необходимости
           return;
       }
   }
}

//процедура ВЫКЛючения сверху вниз
void stopTopBottom()
{
   if (allLEDsAreOn) allLEDsAreOn = false; //уже Не все светодиоды включены, очевидно

   for (byte i=stairsCount-1; i>=0; i--) {
       //пытаемся перебрать все ступеньки по очереди
       if ((i == stairsCount-1) && (stairsArray[i] > initialPWMvalue)) {
           //если ступенька последняя, то снижать яркость до дежурного уровня ШИМ, а не 0
           stairsArray[i]--;//снизить яркость
           return;
       }
       else if ((i == 0)&&(stairsArray[i] > initialPWMvalue)) {
           //если первая, то снижать яркость до дежурного уровня ШИМ, а не 0
           stairsArray[0]--;//снизить яркость
           if (stairsArray[0] == initialPWMvalue) need2LightOffTopBottom = false; //если это первая ступенька и на ней достигнута минимальная яркость
           return;
       }
       else if ((i != 0) && (i != (stairsCount-1)) && (stairsArray[i] >= 1)) {
           //обработка всех остальных ступенек
           stairsArray[i]--; //снизить яркость
           return;
       }
   }
}

//процедуры синхронизации "фантазий" массива с "реальной жизнью"
void sync2RealLife()
{
   for (int i = 0; i < stairsCount; i++) {
       // 0...5 степени яркости * 800 = вкладываемся в 0...4096
       Tlc.set(i, stairsArray[i]*800);
   }
   Tlc.update();
}

//процедура первоначальной "инициализации" сонаров
void sonarPrepare(sonar_opt_t *sonar)
{
    pinMode(sonar->trigger, OUTPUT);
    pinMode(sonar->echo, INPUT);
    pinMode(sonar->reset, OUTPUT);
    digitalWrite(sonar->reset, HIGH); //всегда должен быть HIGH, для перезагрузки сонара кратковременно сбросить в LOW

    sonar->prev_time = millis();
    sonar->ignore = 0;
}

// процедура ресета подвисшего сонара, на 100мс "отбирает" у него питание
void sonarReset(sonar_opt_t *sonar)
{
    digitalWrite(sonar->reset, LOW);
    delay(100);
    digitalWrite(sonar->reset, HIGH);
}

//процедура "запрета" сонара
void sonarDisable(sonar_opt_t *sonar)
{
    sonar->prev_time = millis();
}

//функция, дающая знать, не "разрешен" ли уже сонар
boolean sonarEnabled(sonar_opt_t *sonar)
{
    if ((sonar->prev_time + sonar->inactive_time) <= millis()) {
        return true;
    }

    return false;
}

//процедура проверки, сработал ли сонар (с отслеживанием "подвисания" сонаров)
boolean sonarTrigged(sonar_opt_t *sonar)
{
    if (sonarEnabled(sonar)) {
        digitalWrite(sonar->trigger, LOW);
        delayMicroseconds(5);
        digitalWrite(sonar->trigger, HIGH);
        delayMicroseconds(15);
        digitalWrite(sonar->trigger, LOW);

        unsigned int time_us = pulseIn(sonar->echo, HIGH, 5000);//5000 - таймаут, то есть не ждать сигнала более 5мс
        unsigned int distance = time_us / 58;

        if ((distance != 0) && (distance <= sonar->mindist)) {
            //сонар считается сработавшим, принимаем "меры"

            if (sonar->ignore > 0) {
                // если требуется 1 раз проигнорить сонар
                sonar->ignore--; // проигнорили, сбрасываем за собой флаг необходимости
                sonarDisable(sonar); // временно "запретить" сонар, ведь по факту он сработал (хотя и заигнорили), иначе каждые 400мс будут "ходить всё новые люди"
                return false;
            }

            sonarDisable(sonar); // временно "запретить" сонар, иначе каждые 400мс будут "ходить всё новые люди"
            return true;
        }
        else if (distance == 0) {
            // sonar was freezed
            sonarReset(sonar);
        }

        return false;
    }
}
