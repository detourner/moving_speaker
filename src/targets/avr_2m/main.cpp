#include <Arduino.h>
#include "timer.h"
#include "../../common/moving_speaker_protocol.h"

CounterA counterA;
CounterB counterB;

StepperCore stepperA;
StepperCore stepperB;

MotorChannel motors[] = {
    { &stepperA, false },
    { &stepperB, true },
};

MovingSpeakerProtocol protocol(
    Serial, motors, 2,
    "I: Moving Speaker V2.1 by D\xC3\xA9tourner",
    false,
    false);

ISR(TIMER1_COMPA_vect)
{
    stepperA.RunISR();
}

ISR(TIMER1_COMPB_vect)
{
    stepperB.RunISR();
}

static void setupCounter(Counter& counter, double timerPeriodSec)
{
    uint16_t timerSet = (timerPeriodSec * 1000000.0) /
                        Counter::getTicksPeruSec();
    counter.Set(timerSet);
    counter.Enable();
}

void setup()
{
    Serial.begin(115200);

    Counter::Setup(C250kHz);

    stepperA.Setup(3, 2, 480e-6, 32000, -8000, 8000);
    setupCounter(counterA, 480e-6);
    delayMicroseconds(100);
    stepperB.Setup(5, 4, 480e-6, 32000, 0, 32000);
    setupCounter(counterB, 480e-6);

    protocol.sendInfoFrame();
}

void loop()
{
    protocol.process();
}