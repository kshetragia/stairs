#include <Arduino.h>
#include "Tlc5940.h"

typedef struct {
    const byte echo;
    const word inactive_time;
    const byte mindist;
    const byte reset;
    const byte trigger;
    byte ignore;
    unsigned long prev_time;  //����� ������ ���������� ������ inactive time �����������
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

const byte stairsCount = 12;         // ���������� ��������
const byte initialPWMvalue = 2;      // only 0...5 (������� ������ � ��������� �������� � ��������)
const word waitForTurnOff = 7000;    // ��, ����� �������� ��������� �� ��� ��������� ����� ��������� ��������� ���������

byte stairsArray[stairsCount];       // ������, ��� ������ ������� ������������� ���������. ��� �������� ������ � ���, ����� sync2realLife
byte direction = 0;                  // 0 = ����� �����, 1 = ������ ����

boolean allLEDsAreOn = false;           // ����, ��� ���������� ��������, ��������� �������� � ����� ���������
boolean need2LightOnBottomTop = false;  // ����, ��������� ��������� �������� ����� �����
boolean need2LightOffBottomTop = false; // ����, ��������� ���������� �������� ����� �����
boolean need2LightOnTopBottom = false;  // ����, ��������� ��������� �������� ������ ����
boolean need2LightOffTopBottom = false; // ����, ��������� ���������� �������� ������ ����
boolean sonar1trigged = false;          // ����, ����������� � �������� �� ������������ ������ � ������ ��������
boolean sonar2trigged = false;          // ����, ����������� � �������� �� ������������ ������ � ������ ��������
boolean nothingHappening = true;        // ����, ����������� �� "��������" ����� ��������, �.�. �������� ���������

unsigned long allLEDsAreOnTime;   //����� ������ ������� ���� ��������

static void sync2RealLife();
static void sonarPrepare(sonar_opt_t *sonar);
static void sonarDisable(sonar_opt_t *sonar);
static void startBottomTop();
static void stopBottomTop();
static void startTopBottom();
static void stopTopBottom();

static boolean sonarTrigged(sonar_opt_t *sonar);

// ����������
void setup()
{
    sonar1_p = &sonar1;
    sonar2_p = &sonar2;

   //������ ������ ���������� ���������� ������� ��������
   for (byte i = 1; i <= stairsCount-2; i++)
       stairsArray[i] = 0;

   //����������� ��������� ������� ������ ���������
   stairsArray[0] = initialPWMvalue;

   //����������� ��������� ������� ��������� ���������
   stairsArray[stairsCount-1] = initialPWMvalue;

   // ������������� TLC-���
   Tlc.init();

   //�����, ����� ���������� ��������� (�������������) �� "���������" ����������
   delay(1000);

   //"����������" ��������� �������� ������� �������� � "�������� �����"
   sync2RealLife();

   // �������������� ������
   sonarPrepare(sonar1_p);
   sonarPrepare(sonar2_p);
}

// �������� ������
void loop()
{
    sonar1trigged = sonarTrigged(sonar1_p); //����������� ����� ������ 1 ��� ����������� ����������� � ���
    sonar2trigged = sonarTrigged(sonar2_p); //����������� ����� ������ 2 ��� ����������� ����������� � ���

    nothingHappening = !((need2LightOnTopBottom)||(need2LightOffTopBottom)||(need2LightOnBottomTop)||(need2LightOffBottomTop)||(allLEDsAreOn));

    //���� �������� ��������� � ��������(�����������) ���������, ����� �������� �����-"���������" �� ������ ������
    if (nothingHappening) {
	sonar1_p->ignore = 0;
	sonar2_p->ignore = 0;
    }

    //������� ��������� ������������ ������, ����� ��������� ���� �������

    //������� ���������: ������� - ����� ����� (����������� ������ � ���������)
    if ((sonar1trigged) && (nothingHappening)) {
        need2LightOnBottomTop = true; //������ ��������� �������� ����� �����
        sonar2_p->ignore++;  //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ������ ����
    }
    else if ((sonar1trigged) && ((need2LightOnBottomTop)||(allLEDsAreOn))) {
        //���� ��������� ��� ���������� � ������ ����������� ��� ��� �����
        sonarDisable(sonar1_p); //������ ��������� ����� �������� ��������� ���������� �������� ����� �����
        allLEDsAreOnTime = millis();
        sonar2_p->ignore++;  //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ������ ����
        direction = 0; //����������� - ����� �����
    }
    else if ((sonar1trigged) && (need2LightOffBottomTop)) {
        // � ��� ���������� ������� ����� �����
        need2LightOffBottomTop = false; //���������� ������� �������� ����� �����
        need2LightOnBottomTop = true;   //������ ��������� �������� ����� �����
        sonar2_p->ignore++;  //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ������ ����
    }
    else if ((sonar1trigged) && (need2LightOnTopBottom)){
        //� ��� ���������� ��������� ������ ����
        need2LightOnTopBottom = false; //���������� ��������� �������� ������ ����
        need2LightOnBottomTop = true;  //������ ������� �������� ����� �����
        sonar2_p->ignore++;  //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ������ ����
    }
    else if ((sonar1trigged) && (need2LightOffTopBottom)){
        //� ��� ���������� ������� ������ ����
        need2LightOffTopBottom = false; //���������� ������� �������� ������ ����
        need2LightOnBottomTop = true;   //������ ������� �������� ����� �����
        sonar2_p->ignore++;  //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ������ ����
    }

    //������� ���������: ������ - ������ ���� (����������� ������ � ���������)
    if ((sonar2trigged) && (nothingHappening)){
        //������� ��������� �������� ������ ���� �� ��������� ��������� ��������
        need2LightOnTopBottom = true;//������ ��������� �������� ������ ����
        sonar1_p->ignore++; //�������� ��������������� �����, ����� ��� ��������������� �� ����������� "���������" ����� �����
    }
    else if ((sonar2trigged) && ((need2LightOnTopBottom)||(allLEDsAreOn))) {
        //���� ��������� ��� ���������� � ������ ����������� ��� ��� �����
        sonarDisable(sonar2_p);//�������� ������ ������� ��� ��������� �������� ������ ����
        allLEDsAreOnTime = millis();
        sonar1_p->ignore++; //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ����� �����
        direction = 1;//����������� - ������ ����
    }
    else if ((sonar2trigged) && (need2LightOffTopBottom)) {
        //� ��� ���������� ������� ������ ����
        need2LightOffTopBottom = false; // ���������� ������� �������� ������ ����
        need2LightOnTopBottom = true;   // ������ ��������� �������� ������ ����
        sonar1_p->ignore++; //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ����� �����
    }
    else if ((sonar2trigged) && (need2LightOnBottomTop)) {
        //� ��� ���������� ��������� ����� �����
        need2LightOnBottomTop = false; // ���������� ��������� �������� ����� �����
        need2LightOnTopBottom = true;  // ������ ������� �������� ������ ����
        sonar1_p->ignore++; //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ����� �����
    }
    else if ((sonar2trigged) && (need2LightOffBottomTop)) {
        // � ��� ���������� ������� ����� �����
        need2LightOffBottomTop = false; // ���������� ������� �������� ����� �����
        need2LightOnTopBottom = true;   // ������ ������� �������� ������ ����
        sonar1_p->ignore++; //�������� ��������������� �����, ����� ��� ��� ������������ �� ����������� "���������" ����� �����
    }

    //������� ���������� ������������ ����� - ����� ������ ����� �����������, � ���������� �����
    if ((allLEDsAreOn)&&((allLEDsAreOnTime + waitForTurnOff) <= millis())) {
        //���� ������ ��������� � ��������� �����������
        if (direction == 0) {
            need2LightOffBottomTop = true; // ����� �����
        } else {
            need2LightOffTopBottom = true; // ������ ����
        }
    }

    //���������������� ��������� ������ � "�������������" ������� �������� � "����������"
    if (need2LightOnBottomTop){
       //�������� ������� �� 4 ��������, ���������� � 400�� (BottomTop - ����� �����)
       for (byte i=0; i<=3;i++) {
           startBottomTop();
           sync2RealLife();
           delay(100);
       }
    }

    if (need2LightOffBottomTop) {
       //�������� ������� �� 4 ��������, ���������� � 400�� (BottomTop - ����� �����)
       for (byte i=0; i<=3;i++) {
           stopBottomTop();
           sync2RealLife();
           delay(100);
       }
    }

    if (need2LightOnTopBottom) {
       //�������� ������� �� 4 ��������, ���������� � 400�� (TopBottom - ������ ����)
       for (byte i=0; i<=3;i++) {
           startTopBottom();
           sync2RealLife();
           delay(100);
       }
    }

    if (need2LightOffTopBottom) {
       // �������� ������� �� 4 ��������, ���������� � 400�� (TopBottom - ������ ����)
       for (byte i=0; i<=3;i++) {
           stopTopBottom();
           sync2RealLife();
           delay(100);
       }
    }
} // loop

//��������� ��������� ����� �����
void startBottomTop()
{
   for (byte i=1; i<=stairsCount; i++) {
       //��������� ���� �������� �� �������, ���������� �� "1" ������� ��� ����� ��������� �� ���
       if (stairsArray[i-1] <=4) {
          //������, ����� ���������� ������ ����������
          stairsArray[i-1]++; //��������� �� ��� �������
          return;
       }
       else if ((i == stairsCount) && (stairsArray[stairsCount-1] == 5) && (!allLEDsAreOn)) {
          //���� ��������� �������� ��������� ��������� ���������
          allLEDsAreOnTime = millis(); // �������� ����� ������ ��������� "��� ��������� ��������"
          allLEDsAreOn = true; // ����, ��� ��������� ��������
          direction = 0; // ��� ������������ ������� �������� ����� �����
          need2LightOnBottomTop = false; // ��������� ��� - ���������, ���������� �� ����� ���� �������������
          return;
       }
   }
}

//��������� ���������� ����� �����
void stopBottomTop()
{
   if (allLEDsAreOn) allLEDsAreOn = false; //��� �� ��� ���������� ��������, ��������

   for (byte i=0; i<=stairsCount-1; i++) {
       //�������� ��������� ��� ��������� �� �������
       if ((i == 0)&&(stairsArray[i] > initialPWMvalue)) {
           //���� ��������� ������, ������� ������� �� "���������" ������ ���, � �� 0
           stairsArray[0]--; //������� �������
           return;
       }
       else if ((i == stairsCount-1) && (stairsArray[i] > initialPWMvalue)) {
           //���� ���������, �� ������� ������� �� ��������� ������ ���, � �� 0
           stairsArray[i]--;//������� �������

           //���� ��� ��������� ��������� � �� ��� ���������� ����������� �������
           if (stairsArray[stairsCount-1] == initialPWMvalue) need2LightOffBottomTop = false;

           return;
       }
       else if ((i != 0) && (i != (stairsCount-1)) && (stairsArray[i] >= 1)) {
           //��������� ���� ��������� ��������
           stairsArray[i]--;//������� �������
           return;
       }
   }
}

//��������� ��������� ������ ����
void startTopBottom()
{
   for (byte i=stairsCount; i>=1; i--) {
       //��������� ���� �������� �� �������, ���������� �� "1" ������� ��� ����� ��������� �� ���
       if (stairsArray[i-1] <=4) {
           // ������, ����� ���������� ������ ����������
           stairsArray[i-1]++;//��������� �� ��� �������
           return;
       }
       else if ((i == 1) && (stairsArray[0] == 5) && (!allLEDsAreOn)) {
           //���� ��������� �������� ��������� ��������� ���������
           allLEDsAreOnTime = millis();//�������� ����� ������ ��������� "��� ��������� ��������"
           allLEDsAreOn = true;//����, ��� ��������� ��������
           direction = 1;//��� ������������ ������� �������� ����� �����
           need2LightOnTopBottom = false; // ��������� ��� - ���������, ���������� �� ����� ���� �������������
           return;
       }
   }
}

//��������� ���������� ������ ����
void stopTopBottom()
{
   if (allLEDsAreOn) allLEDsAreOn = false; //��� �� ��� ���������� ��������, ��������

   for (byte i=stairsCount-1; i>=0; i--) {
       //�������� ��������� ��� ��������� �� �������
       if ((i == stairsCount-1) && (stairsArray[i] > initialPWMvalue)) {
           //���� ��������� ���������, �� ������� ������� �� ��������� ������ ���, � �� 0
           stairsArray[i]--;//������� �������
           return;
       }
       else if ((i == 0)&&(stairsArray[i] > initialPWMvalue)) {
           //���� ������, �� ������� ������� �� ��������� ������ ���, � �� 0
           stairsArray[0]--;//������� �������
           if (stairsArray[0] == initialPWMvalue) need2LightOffTopBottom = false; //���� ��� ������ ��������� � �� ��� ���������� ����������� �������
           return;
       }
       else if ((i != 0) && (i != (stairsCount-1)) && (stairsArray[i] >= 1)) {
           //��������� ���� ��������� ��������
           stairsArray[i]--; //������� �������
           return;
       }
   }
}

//��������� ������������� "��������" ������� � "�������� ������"
void sync2RealLife()
{
   for (int i = 0; i < stairsCount; i++) {
       // 0...5 ������� ������� * 800 = ������������ � 0...4096
       Tlc.set(i, stairsArray[i]*800);
   }
   Tlc.update();
}

//��������� �������������� "�������������" �������
void sonarPrepare(sonar_opt_t *sonar)
{
    pinMode(sonar->trigger, OUTPUT);
    pinMode(sonar->echo, INPUT);
    pinMode(sonar->reset, OUTPUT);
    digitalWrite(sonar->reset, HIGH); //������ ������ ���� HIGH, ��� ������������ ������ �������������� �������� � LOW

    sonar->prev_time = millis();
    sonar->ignore = 0;
}

// ��������� ������ ���������� ������, �� 100�� "��������" � ���� �������
void sonarReset(sonar_opt_t *sonar)
{
    digitalWrite(sonar->reset, LOW);
    delay(100);
    digitalWrite(sonar->reset, HIGH);
}

//��������� "�������" ������
void sonarDisable(sonar_opt_t *sonar)
{
    sonar->prev_time = millis();
}

//�������, ������ �����, �� "��������" �� ��� �����
boolean sonarEnabled(sonar_opt_t *sonar)
{
    if ((sonar->prev_time + sonar->inactive_time) <= millis()) {
        return true;
    }

    return false;
}

//��������� ��������, �������� �� ����� (� ������������� "����������" �������)
boolean sonarTrigged(sonar_opt_t *sonar)
{
    if (sonarEnabled(sonar)) {
        digitalWrite(sonar->trigger, LOW);
        delayMicroseconds(5);
        digitalWrite(sonar->trigger, HIGH);
        delayMicroseconds(15);
        digitalWrite(sonar->trigger, LOW);

        unsigned int time_us = pulseIn(sonar->echo, HIGH, 5000);//5000 - �������, �� ���� �� ����� ������� ����� 5��
        unsigned int distance = time_us / 58;

        if ((distance != 0) && (distance <= sonar->mindist)) {
            //����� ��������� �����������, ��������� "����"

            if (sonar->ignore > 0) {
                // ���� ��������� 1 ��� ����������� �����
                sonar->ignore--; // �����������, ���������� �� ����� ���� �������������
                sonarDisable(sonar); // �������� "���������" �����, ���� �� ����� �� �������� (���� � ����������), ����� ������ 400�� ����� "������ �� ����� ����"
                return false;
            }

            sonarDisable(sonar); // �������� "���������" �����, ����� ������ 400�� ����� "������ �� ����� ����"
            return true;
        }
        else if (distance == 0) {
            // sonar was freezed
            sonarReset(sonar);
        }

        return false;
    }
}
