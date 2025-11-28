#include <Arduino.h>
#include "timer.h"
#include "stepper.h"

volatile uint16_t a = 0;
volatile uint16_t b = 0;

CounterA counterA;
CounterB counterB;

Stepper stepperA;

ISR(TIMER1_COMPA_vect) {
  stepperA.DoStep();
  a++;
}

ISR(TIMER1_COMPB_vect) {
  b++;
}



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Counter::Setup(C250kHz);

  counterA.Enable();
  counterB.Enable();

  stepperA.Setup(5, 4, counterA);
  stepperA.setMaxSpeed(1000);
  stepperA.setAcceleration(500);
  stepperA.moveTo(20000);

}

char myData[64]; // buffer pour la trame

void loop() 
{
  static long updateSendSerial = 0;

  if (millis() - updateSendSerial > 100) 
  {
    updateSendSerial = millis();

    Serial.print("P:");
    Serial.print(stepperA.currentPosition());  // 2 décimales
    Serial.print(" i:");
    Serial.print(stepperA.stepInterval());  
    Serial.print(" s:");
    Serial.print(stepperA.speed()); 
    Serial.print(" t:");
    Serial.println(stepperA.debug()); 
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
    uint16_t Pan_target = atoi(token);

    token = strtok(NULL, ",");
    if (!token) return;
    float Pan_speed = atof(token);
    
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
      //Profile_b::SetSpeed((float)Pan_speed);
      Serial.print("I:");
      Serial.print(Pan_speed);  // 2 décimales
      Serial.print(",");
      Serial.print(Pan_target); // fin de trame avec '\n'
      Serial.println("end");
      stepperA.moveTo(Pan_target);
      stepperA.setMaxSpeed(Pan_speed);
    }
  }
}

