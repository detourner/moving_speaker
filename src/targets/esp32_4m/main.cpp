#include <Arduino.h>
#include "../../common/moving_speaker_protocol.h"

StepperCore stepperA;
StepperCore stepperB;
StepperCore stepperC;
StepperCore stepperD;

MotorChannel motors[] = {
    { &stepperA, false },
    { &stepperB, true },
    { &stepperC, false },
    { &stepperD, true },
};

MovingSpeakerProtocol protocol(
    Serial, motors, 4,
    "I: Moving Speaker V2.1 by D\xC3\xA9tourner",
    true,
    true);

static hw_timer_t* timerGroup0 = nullptr;
static hw_timer_t* timerGroup1 = nullptr;

void IRAM_ATTR timerGroupISR0()
{
    stepperA.RunISR();
    stepperB.RunISR();
}

void IRAM_ATTR timerGroupISR1()
{
    stepperC.RunISR();
    stepperD.RunISR();
}

static void setupMotorTimers()
{
    constexpr uint64_t timerPeriodUs = 480;

    timerGroup0 = timerBegin(1000000);
    if (timerGroup0) {
        timerAttachInterrupt(timerGroup0, timerGroupISR0);
        timerAlarm(timerGroup0, timerPeriodUs, true, 0);
        timerStart(timerGroup0);
    }

    timerGroup1 = timerBegin(1000000);
    if (timerGroup1) {
        timerAttachInterrupt(timerGroup1, timerGroupISR1);
        timerAlarm(timerGroup1, timerPeriodUs, true, 0);
        timerStart(timerGroup1);
    }
}

void setup()
{
    Serial.begin(115200);
    delay(1000);

    stepperA.Setup(D0, D1, 480e-6, 32000, -8000, 8000);
    stepperB.Setup(D2, D3, 480e-6, 8000, 0, 8000);
    stepperC.Setup(D4, D5, 480e-6, 32000, -8000, 8000);
    stepperD.Setup(D7, D8, 480e-6, 16000, 0, 16000);
    setupMotorTimers();

    protocol.sendInfoFrame();
}

void loop()
{
    protocol.process();
}