#ifndef STEPPER_CORE_H
#define STEPPER_CORE_H

#include <Arduino.h>
#include <stdint.h>
#include <math.h>

#ifdef IRAM_ATTR
#define STEPPER_IRAM_ATTR IRAM_ATTR
#else
#define STEPPER_IRAM_ATTR
#endif

enum RotaryMode : uint8_t {
    ROT_SHORTEST,
    ROT_CW,
    ROT_CCW,
};

struct StepperState
{
    long position;
    long positionModulo;
    long targetPosition;
    long stepsPerRev;
    double speed;
    double maxSpeed;
    double acceleration;
    bool running;
};

class StepperCore
{
    public:
        void Setup(uint8_t stepPin, uint8_t dirPin,
                   double timerPeriodSec,
                   long steps_per_rev,
                   long minPos, long maxPos);

        void readState(StepperState& state);
        void applyCommandDegrees(double targetDeg, double speedDeg,
                     double accelerationDeg, RotaryMode mode,
                     bool modulo);

        void STEPPER_IRAM_ATTR RunISR();

        void renormalizePosition();

        double getMaxSpeedMax(void) { return _vmaxMax; }
        double getMaxSpeedDegMax()
        {
            return _vmaxMax * 360.0 / (double)_steps_per_rev;
        }
        double getMaxSpeedMin(void) { return _vmaxMin; }
        double getMaxSpeedDegMin()
        {
            return _vmaxMin * 360.0 / (double)_steps_per_rev;
        }
        double getAccelMin(void) { return _accelMin; }
        double getAccelDegMin()
        {
            return _accelMin * 360.0 / (double)_steps_per_rev;
        }
        double getAccelMax(void) { return _accelMax; }
        double getAccelDegMax()
        {
            return _accelMax * 360.0 / (double)_steps_per_rev;
        }

        double getMaxPositionDeg()
        {
            return (double)_maxPos * 360.0 / (double)_steps_per_rev;
        }
        double getMinPositionDeg()
        {
            return (double)_minPos * 360.0 / (double)_steps_per_rev;
        }

    protected:
        bool isRunning()
        {
            return !(_position == _targetPos && _curSpeed == 0.0 &&
                     _accSteps == 0 && _reversing == false);
        }

        bool homePosition();

        void configurePins(uint8_t stepPin, uint8_t dirPin)
        {
            _stepPin = stepPin;
            _dirPin = dirPin;
        }

        void configureMotion(double timerPeriodSec, long stepsPerRev,
                             long minPos, long maxPos);

        void STEPPER_IRAM_ATTR emitStep(int direction);
        void enterCritical();
        void leaveCritical();

        uint8_t _stepPin = 0;
        uint8_t _dirPin = 0;
        volatile long _position = 0;
        volatile double _curSpeed = 0.0;
        volatile double _accSteps = 0.0;
        volatile bool _reversing = false;

        double _vmax = 1500.0;
        double _accel = 8000.0;
        long _targetPos = 0;
        long _targetDuringReverse = 0;

        double _timerPeriod = 480e-6;
        long _steps_per_rev = 32000;
        long _minPos = 0;
        long _maxPos = 32000;
        double _vmaxMin = 1;
        double _vmaxMax = 4000.0;
        double _accelMin = 100.0;
        double _accelMax = 10000.0;
};

#endif