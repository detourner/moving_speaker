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

    Serial.print("I:");
    Serial.print(isrDuration);
    Serial.print("P:");
    Serial.print(stepperA.getPositionDeg());  // 2 décimales
    Serial.print(" v:");
    Serial.println(stepperA.getSpeed());  

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
    if (fieldCount != 2-1) { // 4 virgules = 5 champs
      Serial.println("I:Invalid frame: wrong number of fields");
      return;
    }

    // Extraire les champs
    char* token = strtok(myData, ",");
    if (!token) return;
    double Pan_target = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double Pan_speed = atof(token);

    /*token = strtok(NULL, ",");
    if (!token) return;
    uint8_t dir = atoi(token);*/
    
    /*token = strtok(NULL, ",");
    if (!token) return;
    uint16_t Tilt_target = atoi(token);

    token = strtok(NULL, ",");
    if (!token) return;
    uint16_t Tilt_speed = atoi(token);   

    token = strtok(NULL, ",");
    if (!token) return;
    uint16_t Homing = atoi(token);*/

    // Saturation des valeurs
    //if (Tilt_target < TILT_MIN) Tilt_target = TILT_MIN;
    //if (Tilt_target > TILT_MAX) Tilt_target = TILT_MAX;
    //if (Tilt_speed < TILT_SPEED_MIN) Tilt_speed = TILT_SPEED_MIN;
    //if (Tilt_speed > TILT_SPEED_MAX) Tilt_speed = TILT_SPEED_MAX;
    //if (Pan_target < PAN_MIN) Pan_target = PAN_MIN;
    //if (Pan_target > PAN_MAX) Pan_target = PAN_MAX;
    //if (Pan_speed < PAN_SPEED_MIN) Pan_speed = PAN_SPEED_MIN;
    //if (Pan_speed > PAN_SPEED_MAX) Pan_speed = PAN_SPEED_MAX;

    // Traitement
    /*if (Homing == 1) {
   
      Serial.println("I:Homing done!");
    } else*/ {

      //Profile_a::SetSpeed(Tilt_speed);
      //Profile_a::MoveTo(Tilt_target);
      Serial.print("I:");
      Serial.print(Pan_speed);  // 2 décimales
      Serial.print(",");      
      Serial.print(Pan_target); // fin de trame avec '\n'
      Serial.println("end");
      stepperA.moveToWithLimitsDeg(Pan_target);
      stepperA.setMaxSpeed(Pan_speed);
    }
  }
}
