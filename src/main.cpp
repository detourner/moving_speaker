#include <Arduino.h>
#include "timer.h"
#include "stepper.h"

char myData[64]; // buffer pour la trame

volatile uint16_t a = 0;

CounterA counterA;
CounterB counterB;

Stepper stepperA;
Stepper stepperB;

uint16_t duration;

volatile unsigned long isrDuration = 0;  // en microsecondes
// ------------------ TRAPÉZOÏDE DYNAMIQUE -------------------
ISR(TIMER1_COMPA_vect)
{
  unsigned long start = micros();
  stepperA.RunISR();

  isrDuration = micros() - start;

}

ISR(TIMER1_COMPB_vect)
{
  stepperB.RunISR();
}

// ------------------------ DEMO ------------------------
void setup()
{
    Serial.begin(115200);

    Counter::Setup(C250kHz);

    stepperA.Setup(3, 2, counterA, 480e-6, 
                  32000, -8000, 8000); 
    delayMicroseconds(100); // wait a bit to avoid conflict on Timer1 compare A and B
    stepperB.Setup(5, 4, counterB, 480e-6, 
                  32000, 0, 32000); 

    stepperA.setMaxSpeed(1500.0);
    stepperA.setAcceleration(8000.0);

    stepperB.setMaxSpeed(1500.0);
    stepperB.setAcceleration(8000.0);

}

void loop()
{

  static long updateSendSerial = 0;

  if (millis() - updateSendSerial > 100) 
  {
    updateSendSerial = millis();

    Serial.print("P:");
    Serial.print(stepperA.isRunning());
    Serial.print(",");    
    Serial.print(stepperA.getPositionDeg());  // 2 décimales
    Serial.print(",");
    Serial.print(stepperA.getSpeed());
    Serial.print(",");
    Serial.print(stepperB.isRunning());
    Serial.print(",");    
    Serial.print(stepperB.getPositionModuloDeg());  // 2 décimales
    Serial.print(",");
    Serial.println(stepperB.getSpeed());  

    stepperB.renormalizePosition();
    
  }

  if (Serial.available()) 
  {
    byte n = Serial.readBytesUntil('\n', myData, sizeof(myData) - 1);
    myData[n] = '\0'; // null-byte

    // Compter le nombre de champs (virgules)
    int fieldCount = 0;
    for (byte i = 0; i < n; i++) {
      if (myData[i] == ',') fieldCount++;
    }
    if (fieldCount != 5-1) { // 4 virgules = 5 champs
      Serial.println("I:Invalid frame: wrong number of fields");
      return;
    }

    /*Serial.print("I:Received data: ");
    Serial.println(myData);*/

    // Extraire les champs
    char* token = strtok(myData, ",");
    if (!token) return;
    double motA_target = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motA_speed = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motB_target = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motB_speed = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    RotaryMode motB_dir = (RotaryMode)atoi(token);

    /*Serial.print("I:Received targets: MotA ");;
    Serial.print(motA_target);
    Serial.print("° @ ");
    Serial.print(motA_speed);
    Serial.print(" | MotB ");
    Serial.print(motB_target);
    Serial.print("° @ ");
    Serial.print(motB_speed);
    Serial.print(" Dir ");
    Serial.println(motB_dir);*/
    
    stepperA.setMaxSpeed(motA_speed);
    stepperA.moveToWithLimitsDeg(motA_target);

    stepperB.setMaxSpeed(motB_speed);
    stepperB.moveToModuloDeg(motB_target, motB_dir);

  }
}
