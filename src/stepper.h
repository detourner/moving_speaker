#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include "timer.h"

class Stepper 
{
    protected:

    public:
        void Setup(uint8_t stepPin, uint8_t dirPin, Counter& counter, double timerPeriodUs);
        void RunISR(void);

        void moveTo(long absolute);
        void setMaxSpeed(double vm);
        void setAcceleration(double a);

        long getPosition(void) { return _position; }
        long getSpeed(void) { return _curSpeed; }
        long getMaxSpeed(void) {return _vmax;}
        long getAccel(void) { return _accel; }


    private:

       
        uint8_t _stepPin;
        uint8_t _dirPin;
        Counter* _counter;


      

    volatile long _position = 0;      // position actuelle en pas
    volatile double _curSpeed = 0.0;         // vitesse actuelle
    volatile double _accSteps = 0.0;  // accumulateur pas fractionnaires

    double _vmax = 1500.0;        // PAS/S — modifiable à tout moment ➜ dynamique !
    double _accel = 8000.0;       // PAS/S²
    long _targetPos = 0;      // cible à atteindre (pas)

    double _timerPeriod = 480e-6; // 480us
    uint16_t _timerSet = 120;

};    

#endif  // STEPPER_H