#include <Arduino.h>
#include "stepper.h"

char myData[200]; // frame buffer for incoming serial packet

Stepper stepperA;
Stepper stepperB;
Stepper stepperC;
Stepper stepperD;

// for debug only
// volatile unsigned long isrDuration = 0;  // in microseconds

// emits the "I:" info/capabilities frame, at boot and on demand ('I' request)
void printInfoFrame()
{
    Serial.println("I: Moving Speaker V2.0 by Détourner");
    Serial.print("I:");
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
    Serial.print(stepperB.getAccelDegMax());
    Serial.print(",");

    Serial.print(stepperC.getMinPositionDeg());
    Serial.print(",");
    Serial.print(stepperC.getMaxPositionDeg());
    Serial.print(",");
    Serial.print(stepperC.getMaxSpeedDegMin());
    Serial.print(",");
    Serial.print(stepperC.getMaxSpeedDegMax());
    Serial.print(",");
    Serial.print(stepperC.getAccelDegMin()); 
    Serial.print(",");   
    Serial.print(stepperC.getAccelDegMax());
    Serial.print(",");

    Serial.print(stepperD.getMinPositionDeg()); 
    Serial.print(",");
    Serial.print(stepperD.getMaxPositionDeg());
    Serial.print(",");
    Serial.print(stepperD.getMaxSpeedDegMin());
    Serial.print(",");
    Serial.print(stepperD.getMaxSpeedDegMax());
    Serial.print(",");
    Serial.print(stepperD.getAccelDegMin()); 
    Serial.print(",");
    Serial.println(stepperD.getAccelDegMax());
    Serial.println("I: Ready");
}

void setup()
{
    Serial.begin(115200);
    delay(1000); // wait for serial monitor to open

    stepperA.Setup(D0, D1, 0, 480e-6,
                  32000, -8000, 8000);
    stepperB.Setup(D2, D3, 1, 480e-6,
                  8000, 0, 8000);

    stepperC.Setup(D4, D5, 2, 480e-6,
                  32000, -8000, 8000);
    stepperD.Setup(D7, D8, 3, 480e-6,
                  16000, 0, 16000);

    printInfoFrame();
}

void loop()
{

  static long updateSendSerial = 0;

  if (millis() - updateSendSerial > 100) 
  {
    updateSendSerial = millis();

    Serial.print("P: ");
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
    Serial.print(stepperB.getSpeedDeg());  

    Serial.print(",");
    Serial.print(stepperC.isRunning());
    Serial.print(",");    
    Serial.print(stepperC.getPositionDeg());  // position (two decimal places), consistent with A
    Serial.print(",");
    Serial.print(stepperC.getSpeedDeg());  

    Serial.print(",");
    Serial.print(stepperD.isRunning());
    Serial.print(",");    
    Serial.print(stepperD.getPositionModuloDeg());  // position modulo (two decimal places)
    Serial.print(",");
    Serial.println(stepperD.getSpeedDeg());  
  

    stepperB.renormalizePosition();
    stepperD.renormalizePosition();
    
  }

  if (Serial.available()) 
  {
    byte n = Serial.readBytesUntil('\n', myData, sizeof(myData) - 1);
    myData[n] = '\0'; // null terminator

    if (n == 1 && myData[0] == 'I') { // 'I' request: re-emit the info frame on demand
      printInfoFrame();
      return;
    }

    // Count the number of fields (commas)
    int fieldCount = 0;
    for (byte i = 0; i < n; i++) {
      if (myData[i] == ',') fieldCount++;
    }
    if (fieldCount != 14-1) { // Expecting 14 fields -> 13 commas
      Serial.println("E:Invalid frame: wrong number of fields");
      return;
    }

    // Extract fields
    // Frame format expected (comma separated):
    // motA_target, motA_speed, motA_accel, motB_target, motB_speed, motB_dir, motB_accel,
    // motC_target, motC_speed, motC_accel, motD_target, motD_speed, motD_dir, motD_accel
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

    token = strtok(NULL, ",");
    if (!token) return;
    double motC_target = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motC_speed = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motC_accel = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motD_target = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    double motD_speed = atof(token);

    token = strtok(NULL, ",");
    if (!token) return;
    RotaryMode motD_dir = (RotaryMode)atoi(token);
    
    token = strtok(NULL, ",");
    if (!token) return;
    double motD_accel = atof(token);

    stepperA.setAccelerationDeg(motA_accel);
    stepperA.setMaxSpeedDeg(motA_speed);
    stepperA.moveToWithLimitsDeg(motA_target);

    stepperB.setAccelerationDeg(motB_accel);
    stepperB.setMaxSpeedDeg(motB_speed);
    stepperB.moveToModuloDeg(motB_target, motB_dir);

    stepperC.setAccelerationDeg(motC_accel);
    stepperC.setMaxSpeedDeg(motC_speed);
    stepperC.moveToWithLimitsDeg(motC_target);

    stepperD.setAccelerationDeg(motD_accel);
    stepperD.setMaxSpeedDeg(motD_speed);
    stepperD.moveToModuloDeg(motD_target, motD_dir);


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
    Serial.print(stepperB.getAccelDeg());

    Serial.print(",");
    Serial.print(stepperC.isRunning());
    Serial.print(",");
    Serial.print(stepperC.getTargetPositionDeg());
    Serial.print(",");
    Serial.print(stepperC.getMaxSpeedDeg());
    Serial.print(",");
    Serial.print(stepperC.getAccelDeg());
    Serial.print(",");
    Serial.print(stepperD.isRunning());
    Serial.print(",");
    Serial.print(stepperD.getTargetPositionDeg());
    Serial.print(",");    
    Serial.print(stepperD.getMaxSpeedDeg());
    Serial.print(",");
    Serial.println(stepperD.getAccelDeg());

  }
}
