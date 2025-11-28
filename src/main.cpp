#include <Arduino.h>
#include "timer.h"
#include "digitalWriteFast.h"
/********************************************************************
 * Stepper Trapézoïdal Dynamique — Timer1 + ISR
 * Vitesse max dynamique, accélération constante
 ********************************************************************/

// ------------- USER CONFIG --------------------
#define STEP_PIN 5
#define DIR_PIN 4

double vmax = 1500.0;        // PAS/S — modifiable à tout moment ➜ dynamique !
double accel = 8000.0;       // PAS/S²
long targetPos = 20000;      // cible à atteindre (pas)
// ----------------------------------------------

// État du moteur
volatile long position = 0;      // position actuelle en pas
volatile double v = 0.0;         // vitesse actuelle
volatile double accSteps = 0.0;  // accumulateur pas fractionnaires
volatile double v_target = 0.0; // vitesse cible instantanée

// Timer interval
const double ISR_PERIOD_SEC = 480e-6;   // 50 µs = 20 kHz

char myData[64]; // buffer pour la trame

volatile uint16_t a = 0;

CounterA counterA;

// ----------------- HARDWARE STEP -------------------
inline void doStepISR(int dir)
{
    if (dir > 0) digitalWriteFast(DIR_PIN, HIGH);
    else         digitalWriteFast(DIR_PIN, LOW);

    digitalWriteFast(STEP_PIN, HIGH);
    // très court délai (1–2 cycles)
    digitalWriteFast(STEP_PIN, LOW);
}


// ------------------ TRAPÉZOÏDE DYNAMIQUE -------------------
ISR(TIMER1_COMPA_vect)
{
    a++;
    counterA.Set(120); // 4us x 120 = 480µs
 long dist = targetPos - position;
    double d = abs(dist);

    if (dist == 0 && fabs(v) < 1e-6) {
        v = 0;
        accSteps = 0;
        return;
    }

    int dir = (dist >= 0 ? 1 : -1);

    // vitesse maximale atteignable
    double v_peak = sqrt(2.0 * accel * d);
    double v_target = min(vmax, v_peak);

    // ---- changement de sens ----
    if (v * dir < 0) {
        // vitesse opposée → décélération uniquement
        double dv = accel * ISR_PERIOD_SEC;
        if (fabs(v) <= dv) {
            v = 0;
            accSteps = 0;  // reset accumulateur pour éviter steps fantômes
        } else {
            v += (v > 0 ? -dv : dv);  // décélérer vers 0
        }
    }
    else {
        // approche progressive vers v_target
        if (fabs(v) < v_target) {
            v += dir * accel * ISR_PERIOD_SEC;
            if (fabs(v) > v_target) v = dir * v_target;
        } else if (fabs(v) > v_target) {
            v -= dir * accel * ISR_PERIOD_SEC;
            if (fabs(v) < v_target) v = dir * v_target;
        }

        // accumulation fractionnaire seulement après vitesse compatible
        accSteps += v * ISR_PERIOD_SEC;

        // step unique si accumulateur >= 1 ou <= -1
        if (accSteps >= 1.0 || accSteps <= -1.0) {
            int stepDir = (accSteps > 0 ? 1 : -1);
            long nextPos = position + stepDir;

            if ((stepDir > 0 && nextPos >= targetPos) ||
                (stepDir < 0 && nextPos <= targetPos)) {
                position = targetPos;
                accSteps = 0;
                v = 0;
            } else {
                doStepISR(stepDir);
                position = nextPos;
                accSteps -= stepDir;
            }
        }
    }
}


// ------------------------ API -------------------------
void moveTo(long newPos)
{
    targetPos = newPos;
}

void setMaxSpeed(double vm)
{
    vmax = vm;
}

void setAcceleration(double a)
{
    accel = a;
}


// ------------------------ DEMO ------------------------
void setup()
{
    pinModeFast(STEP_PIN, OUTPUT);
    pinModeFast(DIR_PIN, OUTPUT);

    Serial.begin(115200);

    Counter::Setup(C250kHz);
    counterA.Enable();
    counterA.Set(10); // 50µs

    //moveTo(20000);
}

void loop()
{




  static long updateSendSerial = 0;

  if (millis() - updateSendSerial > 100) 
  {
    updateSendSerial = millis();

    Serial.print("t:");
    Serial.print(a);  // 2 décimales
    Serial.print("P:");
    Serial.print(position);  // 2 décimales
    Serial.print(" v:");
    Serial.println(v);  
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
      moveTo(Pan_target);
      setMaxSpeed(Pan_speed);
    }
  }
}
