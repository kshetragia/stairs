//Actualized July 2016
#include "Tlc5940.h"

const byte sonar1trig = 8;
const byte sonar1echo = 2;
const byte sonar1resetpin = 4;
const byte sonar2trig = 6;
const byte sonar2echo = 7;
const byte sonar2resetpin = 5;

const byte button1pin = 12;
const byte button2pin = 7;

const byte sonar1minLimit = 30;//см, если обнаружена дистанция меньше, чем это число, то сонар считается сработавшим
const byte sonar2minLimit = 30;//см, если обнаружена дистанция меньше, чем это число, то сонар считается сработавшим
const word sonarInactiveTime = 1500;//мс, время после срабатывания сонара, в течение которого сонар игнорит следующие срабатывания

const byte stairsCount = 12;//количество ступенек
const byte initialPWMvalue = 2;// only 0...5 (яркость первой и последней ступенек в ожидании)
const word waitForTurnOff = 7000;//мс, время задержки подсветки во вкл состоянии после включения последней ступеньки

byte stairsArray[stairsCount];//массив, где каждый элемент соответствует ступеньке. Все операции только с ним, далее sync2realLife
byte direction = 0;//0 = снизу вверх, 1 = сверху вниз
byte ignoreSonar1Count = 0;//счетчик-флаг, сколько раз игнорировать срабатывание сонара 1
byte ignoreSonar2Count = 0;//счетчик-флаг, сколько раз игнорировать срабатывание сонара 2

boolean allLEDsAreOn = false;//флаг, все светодиоды включены, требуется ожидание в таком состоянии
boolean need2LightOnBottomTop = false;//флаг, требуется включение ступеней снизу вверх
boolean need2LightOffBottomTop = false;//флаг, требуется выключение ступеней снизу вверх
boolean need2LightOnTopBottom = false;//флаг, требуется включение ступеней сверху вниз
boolean need2LightOffTopBottom = false;//флаг, требуется выключение ступеней сверху вниз
boolean sonar1trigged = false;//флаг, участвующий в реакциях на сратабывание сонара в разных условиях
boolean sonar2trigged = false;//флаг, участвующий в реакциях на сратабывание сонара в разных условиях
boolean nothingHappening = true;//флаг, указывающий на "дежурный" режим лестницы, т.н. исходное состояние

unsigned long sonar1previousTime;//время начала блокировки сонара 1 на sonarInactiveTime миллисекунд
unsigned long sonar2previousTime;//время начала блокировки сонара 2 на sonarInactiveTime миллисекунд
unsigned long allLEDsAreOnTime;//время начала горения ВСЕХ ступенек

void setup(){//подготовка
for (byte i = 1; i <= stairsCount-2; i++) stairsArray[i] = 0;//забить массив начальными значениями яркости ступенек
stairsArray[0] = initialPWMvalue;//выставление дефолтной яркости первой ступеньки
stairsArray[stairsCount-1] = initialPWMvalue;//выставление дефолтной яркости последней ступеньки
Tlc.init();//инициализация TLC-шки
delay(1000);//нужно, чтобы предыдущая процедура (инициализация) не "подвесила" контроллер
sync2RealLife();//"пропихнуть" начальные значения яркости ступенек в "реальную жизнь"
sonarPrepare(1);//подготавливаем сонар 1
sonarPrepare(2);//подготавливаем сонар 2
}

void loop(){//бесконечный цикл

sonar1trigged = sonarTrigged(1);//выставление флага сонара 1 для последующих манипуляций с ним
sonar2trigged = sonarTrigged(2);//выставление флага сонара 2 для последующих манипуляций с ним
nothingHappening = !((need2LightOnTopBottom)||(need2LightOffTopBottom)||(need2LightOnBottomTop)||(need2LightOffBottomTop)||(allLEDsAreOn));

if (nothingHappening){//если лестница находится в исходном (выключенном) состоянии, можно сбросить флаги-"потеряшки" на всякий случай
  ignoreSonar1Count = 0;//сколько раз игнорировать сонар 1
  ignoreSonar2Count = 0;//сколько раз игнорировать сонар 2
}

//процесс включения относительно сложен, нужно проверять кучу условий

//процесс ВКЛючения: сначала - снизу вверх (выставление флагов и счетчиков)

if ((sonar1trigged) && (nothingHappening)){//простое включение ступенек снизу вверх из исходного состояния лестницы
  need2LightOnBottomTop = true;//начать освение ступенек снизу вверх
  ignoreSonar2Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
}
else if ((sonar1trigged) && ((need2LightOnBottomTop)||(allLEDsAreOn))){//если ступеньки уже загоряются в нужном направлении или уже горят
  sonarDisable(1);//просто увеличить время ожидания полностью включенной лестницы снизу вверх
  allLEDsAreOnTime = millis();
  ignoreSonar2Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
  direction = 0;//направление - снизу вверх
}
else if ((sonar1trigged) && (need2LightOffBottomTop)){//а уже происходит гашение снизу вверх
  need2LightOffBottomTop = false;//прекратить гашение ступенек снизу вверх
  need2LightOnBottomTop = true;//начать освещение ступенек снизу вверх
  ignoreSonar2Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
}
else if ((sonar1trigged) && (need2LightOnTopBottom)){//а уже происходит освещение сверху вниз
  need2LightOnTopBottom = false;//прекратить освещение ступенек сверху вниз
  need2LightOnBottomTop = true;//начать освение ступенек снизу вверх
  ignoreSonar2Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
}
else if ((sonar1trigged) && (need2LightOffTopBottom)){//а уже происходит гашение сверху вниз
  need2LightOffTopBottom = false;//прекратить гашение ступенек сверху вниз
  need2LightOnBottomTop = true;//начать освение ступенек снизу вверх
  ignoreSonar2Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" сверху вниз
}

//процесс ВКЛючения: теперь - сверху вниз (выставление флагов и счетчиков)

if ((sonar2trigged) && (nothingHappening)){//простое включение ступенек сверху вниз из исходного состояния лестницы
  need2LightOnTopBottom = true;//начать освещение ступенек сверху вниз
  ignoreSonar1Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
}
else if ((sonar2trigged) && ((need2LightOnTopBottom)||(allLEDsAreOn))){//если ступеньки уже загоряются в нужном направлении или уже горят
  sonarDisable(2);//обновить отсчет времени для освещения ступенек сверху вниз
  allLEDsAreOnTime = millis();
  ignoreSonar1Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
  direction = 1;//направление - сверху вниз
}
else if ((sonar2trigged) && (need2LightOffTopBottom)){//а уже происходит гашение сверху вниз
  need2LightOffTopBottom = false;//прекратить гашение ступенек сверху вниз
  need2LightOnTopBottom = true;//начать освещение ступенек сверху вниз
  ignoreSonar1Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
}
else if ((sonar2trigged) && (need2LightOnBottomTop)){//а уже происходит освещение снизу вверх
  need2LightOnBottomTop = false;//прекратить освещение ступенек снизу вверх
  need2LightOnTopBottom = true;//начать освение ступенек сверху вних
  ignoreSonar1Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
}
else if ((sonar2trigged) && (need2LightOffBottomTop)){//а уже происходит гашение снизу вверх
  need2LightOffBottomTop = false;//прекратить гашение ступенек снизу вверх
  need2LightOnTopBottom = true;//начать освение ступенек сверху вниз
  ignoreSonar1Count++;//игнорить противоположный сонар, чтобы при его срабатывании не запустилось "загорание" снизу вверх
}

//процесс ВЫКлючения относительно прост - нужно только знать направление, и выставлять флаги

if ((allLEDsAreOn)&&((allLEDsAreOnTime + waitForTurnOff) <= millis())){//пора гасить ступеньки в указанном направлении
  if (direction == 0) need2LightOffBottomTop = true;//снизу вверх
  else if (direction == 1) need2LightOffTopBottom = true;//сверху вниз
}

//непосредственная обработка флагов с "пропихиванием" массива ступенек в "реальность"

if (need2LightOnBottomTop){//увеличим яркость за 4 итерации, уложившись в 400мс (BottomTop - снизу вверх)
  for (byte i=0; i<=3;i++){
    startBottomTop();
    sync2RealLife();
    delay(100);
  }//for
}//if

if (need2LightOffBottomTop){//уменьшим яркость за 4 итерации, уложившись в 400мс (BottomTop - снизу вверх)
  for (byte i=0; i<=3;i++){
    stopBottomTop();
    sync2RealLife();
    delay(100);
  }//for
}//if

if (need2LightOnTopBottom){//увеличим яркость за 4 итерации, уложившись в 400мс (TopBottom - сверху вниз)
  for (byte i=0; i<=3;i++){
    startTopBottom();
    sync2RealLife();
    delay(100);
  }//for
}//if

if (need2LightOffTopBottom){//уменьшим яркость за 4 итерации, уложившись в 400мс (TopBottom - сверху вниз)
  for (byte i=0; i<=3;i++){
    stopTopBottom();
    sync2RealLife();
    delay(100);
  }//for
}//if
}//procedure

void startBottomTop(){//процедура ВКЛючения снизу вверх
  for (byte i=1; i<=stairsCount; i++){//обработка всех ступенек по очереди, добавление по "1" яркости для одной ступеньки за раз
    if (stairsArray[i-1] <=4){//узнать, какой ступенькой сейчас заниматься
       stairsArray[i-1]++;//увеличить на ней яркость
       return;//прямо сейчас "свалить" из процедуры
    }//if
    else if ((i == stairsCount)&&(stairsArray[stairsCount-1] == 5)&&(!allLEDsAreOn)){//если полностью включена последняя требуемая ступенька
      allLEDsAreOnTime = millis();//сохраним время начала состояния "все ступеньки включены"
      allLEDsAreOn = true;//флаг, все ступеньки включены
      direction = 0;//для последующего гашения ступенек снизу вверх
      need2LightOnBottomTop = false;//поскольку шаг - последний, сбрасываем за собой флаг необходимости
      return;//прямо сейчас "свалить" из процедуры
    }//if
  }//for
}//procedure

void stopBottomTop(){//процедура ВЫКЛючения снизу вверх
  if (allLEDsAreOn) allLEDsAreOn = false;//уже Не все светодиоды включены, очевидно

  for (byte i=0; i<=stairsCount-1; i++){//пытаемся перебрать все ступеньки по очереди
    if ((i == 0)&&(stairsArray[i] > initialPWMvalue)){//если ступенька первая, снижать яркость до "дежурного" уровня ШИМ, а не 0
      stairsArray[0]--;//снизить яркость
      return;//прямо сейчас "свалить" из процедуры
    }
    else if ((i == stairsCount-1)&&(stairsArray[i] > initialPWMvalue)){//если последняя, то снижать яркость до дежурного уровня ШИМ, а не 0
      stairsArray[i]--;//снизить яркость
      if (stairsArray[stairsCount-1] == initialPWMvalue) need2LightOffBottomTop = false; //если это последняя ступенька и на ней достигнута минимальная яркость
      return;//прямо сейчас "свалить" из процедуры
    }
    else if ((i != 0) && (i != (stairsCount-1)) && (stairsArray[i] >= 1)){//обработка всех остальных ступенек
      stairsArray[i]--;//снизить яркость
      return;//прямо сейчас "свалить" из процедуры
    }//if i == 0
  }//for
}//procedure

void startTopBottom(){//процедура ВКЛючения сверху вниз
  for (byte i=stairsCount; i>=1; i--){//обработка всех ступенек по очереди, добавление по "1" яркости для одной ступеньки за раз
    if (stairsArray[i-1] <=4){//узнать, какой ступенькой сейчас заниматься
       stairsArray[i-1]++;//уменьшить на ней яркость
       return;//прямо сейчас "свалить" из процедуры
    }//if
    else if ((i == 1)&&(stairsArray[0] == 5)&&(!allLEDsAreOn)){//если полностью включена последняя требуемая ступенька
      allLEDsAreOnTime = millis();//сохраним время начала состояния "все ступеньки включены"
      allLEDsAreOn = true;//флаг, все ступеньки включены
      direction = 1;//для последующего гашения ступенек снизу вверх
      need2LightOnTopBottom = false;//поскольку шаг - последний, сбрасываем за собой флаг необходимости
      return;//прямо сейчас "свалить" из процедуры
    }//if
  }//for
}//procedure

void stopTopBottom(){//процедура ВЫКЛючения сверху вниз
  if (allLEDsAreOn) allLEDsAreOn = false;//уже Не все светодиоды включены, очевидно

  for (byte i=stairsCount-1; i>=0; i--){//пытаемся перебрать все ступеньки по очереди
    if ((i == stairsCount-1)&&(stairsArray[i] > initialPWMvalue)){//если ступенька последняя, то снижать яркость до дежурного уровня ШИМ, а не 0
      stairsArray[i]--;//снизить яркость
      return;//прямо сейчас "свалить" из процедуры
    }
    else if ((i == 0)&&(stairsArray[i] > initialPWMvalue)){//если первая, то снижать яркость до дежурного уровня ШИМ, а не 0
      stairsArray[0]--;//снизить яркость
      if (stairsArray[0] == initialPWMvalue) need2LightOffTopBottom = false; //если это первая ступенька и на ней достигнута минимальная яркость
      return;//прямо сейчас "свалить" из процедуры
    }
    else if ((i != 0) && (i != (stairsCount-1)) && (stairsArray[i] >= 1)){//обработка всех остальных ступенек
      stairsArray[i]--;//снизить яркость
      return;//прямо сейчас "свалить" из процедуры
    }//if i == 0
  }//for
}//procedure

void sync2RealLife(){//процедуры синхронизации "фантазий" массива с "реальной жизнью"
for (int i = 0; i < stairsCount; i++) Tlc.set(i, stairsArray[i]*800);//0...5 степени яркости * 800 = вкладываемся в 0...4096
Tlc.update();
}//procedure

void sonarPrepare(byte sonarNo){//процедура первоначальной "инициализации" сонаров
if (sonarNo == 1){
  pinMode(sonar1trig, OUTPUT);
  pinMode(sonar1echo, INPUT);
  pinMode(sonar1resetpin, OUTPUT);
  digitalWrite(sonar1resetpin, HIGH);//всегда должен быть HIGH, для перезагрузки сонара кратковременно сбросить в LOW
}
else if (sonarNo == 2){
  pinMode(sonar2trig, OUTPUT);
  pinMode(sonar2echo, INPUT);
  pinMode(sonar2resetpin, OUTPUT);
  digitalWrite(sonar2resetpin, HIGH);//всегда должен быть HIGH, для перезагрузки сонара кратковременно сбросить в LOW
}
}//procedure

void sonarReset(byte sonarNo){//процедура ресета подвисшего сонара, на 100 мс "отбирает" у него питание
  if (sonarNo == 1){
    digitalWrite(sonar1resetpin, LOW);
    delay(100);
    digitalWrite(sonar1resetpin, HIGH);
  }
  else if (sonarNo == 2){
    digitalWrite(sonar2resetpin, LOW);
    delay(100);
    digitalWrite(sonar2resetpin, HIGH);
  }//if
}//procedure

void sonarDisable(byte sonarNo){//процедура "запрета" сонара
  if (sonarNo == 1) sonar1previousTime = millis();
  if (sonarNo == 2) sonar2previousTime = millis();
}

boolean sonarEnabled(byte sonarNo){//функция, дающая знать, не "разрешен" ли уже сонар

  if ((sonarNo == 1)&&((sonar1previousTime + sonarInactiveTime) <= millis())) return true;
  if ((sonarNo == 2)&&((sonar2previousTime + sonarInactiveTime) <= millis())) return true;
  else return false;
}

boolean sonarTrigged(byte sonarNo){//процедура проверки, сработал ли сонар (с отслеживанием "подвисания" сонаров)
  if ((sonarNo == 1)&&(sonarEnabled(1))){
    digitalWrite(sonar1trig, LOW);
    delayMicroseconds(5);
    digitalWrite(sonar1trig, HIGH);
    delayMicroseconds(15);
    digitalWrite(sonar1trig, LOW);
    unsigned int time_us = pulseIn(sonar1echo, HIGH, 5000);//5000 - таймаут, то есть не ждать сигнала более 5мс
    unsigned int distance = time_us / 58;
    if ((distance != 0)&&(distance <= sonar1minLimit)){//сонар считается сработавшим, принимаем "меры"

      if (ignoreSonar1Count > 0) {//если требуется 1 раз проигнорить сонар 2
        ignoreSonar1Count--;//проигнорили, сбрасываем за собой флаг необходимости
        sonarDisable(1);//временно "запретить" сонар, ведь по факту он сработал (хотя и заигнорили), иначе каждые 400мс будут "ходить всё новые люди"
        return false;
      }
      sonarDisable(1);//временно "запретить" сонар, иначе каждые 400мс будут "ходить всё новые люди"
      return true;
    }
    else if (distance == 0){//сонар 1 "завис"
      sonarReset(1);
      return false;
    }//endelse
    else return false;
  }//if sonar 1

  if ((sonarNo == 2)&&(sonarEnabled(2))){
    digitalWrite(sonar2trig, LOW);
    delayMicroseconds(5);
    digitalWrite(sonar2trig, HIGH);
    delayMicroseconds(15);
    digitalWrite(sonar2trig, LOW);
    unsigned int time_us = pulseIn(sonar2echo, HIGH, 5000);//5000 - таймаут, то есть не ждать сигнала более 5мс
    unsigned int distance = time_us / 58;
    if ((distance != 0)&&(distance <= sonar2minLimit)){//сонар считается сработавшим, принимаем "меры"

      if (ignoreSonar2Count > 0) {//если требуется 1 раз проигнорить сонар 2
        ignoreSonar2Count--;//проигнорили, сбрасываем за собой флаг необходимости
        sonarDisable(2);//временно "запретить" сонар, ведь по факту он сработал (хотя и заигнорили), иначе каждые 400мс будут "ходить всё новые люди"
        return false;
      }
      sonarDisable(2);//временно "запретить" сонар, иначе каждые 400мс будут "ходить всё новые люди"
      return true;
    }
    else if (distance == 0){//сонар 2 "завис"
      sonarReset(2);
      return false;
    }//endelse
    else return false;
  }//if sonar 2
}//procedure
