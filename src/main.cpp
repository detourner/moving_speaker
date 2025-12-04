#include <Arduino.h>
#include "timer.h"
#include "stepper.h"

char myData[64]; // frame buffer for incoming serial packet

CounterA counterA;
CounterB counterB;

Stepper stepperA;
Stepper stepperB;

// for debug only
// volatile unsigned long isrDuration = 0;  // in microseconds

ISR(TIMER1_COMPA_vect)
{
  // for debug only, measure ISR duration
  //unsigned long start = micros();
  
  stepperA.RunISR();

  //isrDuration = micros() - start;

}

ISR(TIMER1_COMPB_vect)
{
  stepperB.RunISR();
}


void setup()
{
    Serial.begin(115200);

    Counter::Setup(C250kHz);

    stepperA.Setup(3, 2, counterA, 480e-6, 
                  32000, -8000, 8000); 
    delayMicroseconds(100); // wait a bit to avoid conflict on Timer1 compare A and B
    stepperB.Setup(5, 4, counterB, 480e-6, 
                  32000, 0, 32000); 

    Serial.println("I: Moving Speaker V1.0 by Détourner");
    Serial.println("I:");
    Serial.print(stepperA.getMinPositionDeg());
    Serial.print(",");
    Serial.print(stepperA.getMaxPositionDeg());
    Serial.print(",");
    Serial.print(stepperA.getMaxSpeedDegMin());
    Serial.print(",");
    Serial.print(stepperA.getMaxSpeedDegMax());
    Serial.print(",");
    Serial.print(stepperA.getAccelDegMin()); 
    Serial.print(",");   
    Serial.print(stepperA.getAccelDegMax());
    Serial.print(",");
    Serial.print(stepperB.getMinPositionDeg());
    Serial.print(",");
    Serial.print(stepperB.getMaxPositionDeg());
    Serial.print(",");
    Serial.print(stepperB.getMaxSpeedDegMin());
    Serial.print(",");
    Serial.print(stepperB.getMaxSpeedDegMax());
    Serial.print(",");
    Serial.print(stepperB.getAccelDegMin()); 
    Serial.print(",");   
    Serial.println(stepperB.getAccelDegMax());
    Serial.println("I: Ready");
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
    Serial.print(stepperA.getPositionDeg());  // position (two decimal places)
    Serial.print(",");
    Serial.print(stepperA.getSpeedDeg());
    Serial.print(",");
    Serial.print(stepperB.isRunning());
    Serial.print(",");    
    Serial.print(stepperB.getPositionModuloDeg());  // position modulo (two decimal places)
    Serial.print(",");
    Serial.println(stepperB.getSpeedDeg());  

    stepperB.renormalizePosition();
    
  }

  if (Serial.available()) 
  {
    byte n = Serial.readBytesUntil('\n', myData, sizeof(myData) - 1);
    myData[n] = '\0'; // null terminator

    // Count the number of fields (commas)
    int fieldCount = 0;
    for (byte i = 0; i < n; i++) {
      if (myData[i] == ',') fieldCount++;
    }
    if (fieldCount != 7-1) { // Expecting 7 fields -> 6 commas
      Serial.println("E:Invalid frame: wrong number of fields");
      return;
    }

    // Extract fields
    // Frame format expected (comma separated):
    // motA_target, motA_speed, motA_accel, motB_target, motB_speed, motB_dir, motB_accel
    char* token = strtok(myData, ",");
    if (!token) return;
    double motA_target = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motA_speed = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motA_accel = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motB_target = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motB_speed = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    RotaryMode motB_dir = (RotaryMode)atoi(token);

    
    token = strtok(NULL, ",");
    if (!token) return;
    double motB_accel = atof(token);

    stepperA.setAccelerationDeg(motA_accel);
    stepperA.setMaxSpeedDeg(motA_speed);
    stepperA.moveToWithLimitsDeg(motA_target);

    stepperB.setAccelerationDeg(motB_accel);
    stepperB.setMaxSpeedDeg(motB_speed);
    stepperB.moveToModuloDeg(motB_target, motB_dir);

    Serial.print("S: ");
    Serial.print(stepperA.isRunning());
    Serial.print(",");
    Serial.print(stepperA.getTargetPositionDeg());
    Serial.print(",");
    Serial.print(stepperA.getMaxSpeedDeg());
    Serial.print(",");
    Serial.print(stepperA.getAccelDeg());
    Serial.print(",");
    Serial.print(stepperB.isRunning());
    Serial.print(",");
    Serial.print(stepperB.getTargetPositionDeg());
    Serial.print(",");    
    Serial.print(stepperB.getMaxSpeedDeg());
    Serial.print(",");
    Serial.println(stepperB.getAccelDeg());

  }
}
