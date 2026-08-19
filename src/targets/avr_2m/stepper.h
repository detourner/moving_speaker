#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include <cmath>
#include "timer.h"

enum RotaryMode : uint8_t {
    ROT_SHORTEST,
    ROT_CW,
    ROT_CCW,
};

class Stepper
{
    public:
        void Setup(uint8_t stepPin, uint8_t dirPin,
                   Counter& counter, double timerPeriodSec,
                   long steps_per_rev,
                   long minPos, long maxPos);

        void RunISR(void);

        void moveToSteps(long absolute);
        void moveToDeg(double absolute)
        {
            moveToSteps((long)round(absolute * _steps_per_rev / 360.0));
        }

        void moveToModuloSteps(long targetModulo, RotaryMode mode = ROT_SHORTEST);
        void moveToModuloDeg(double targetModulo, RotaryMode mode = ROT_SHORTEST)
        {
            moveToModuloSteps((long)round(targetModulo * _steps_per_rev / 360.0), mode);
        }

        void moveToWithLimitsSteps(long absolute);
        void moveToWithLimitsDeg(double absolute)
        {
            moveToWithLimitsSteps((long)round(absolute * _steps_per_rev / 360.0));
        }

        bool isRunning()
        {
            return !(_position == _targetPos && _curSpeed == 0.0 && _accSteps == 0 && _reversing == false);
        }

        void renormalizePosition();
        bool homePosition();
        void setMaxSpeed(double vm);
        void setMaxSpeedDeg(double vmDeg)
        {
            setMaxSpeed(vmDeg * (double)_steps_per_rev / 360.0);
        }
        void setAcceleration(double a);
        void setAccelerationDeg(double aDeg)
        {
            setAcceleration(aDeg * (double)_steps_per_rev / 360.0);
        }

        long getPositionSteps(void) { return _position; }
        double getPositionDeg(void)
        {
            return (double)_position * 360.0 / (double)_steps_per_rev;
        }
        long getPositionModuloSteps(void)
        {
            long posModulo = _position % _steps_per_rev;
            if (posModulo < 0) posModulo += _steps_per_rev;
            return posModulo;
        }
        double getPositionModuloDeg(void)
        {
            long posModulo = getPositionModuloSteps();
            return (double)posModulo * 360.0 / (double)_steps_per_rev;
        }

        double getSpeed(void) { return _curSpeed; }
        double getSpeedDeg(void)
        {
            return _curSpeed * 360.0 / (double)_steps_per_rev;
        }
        double getMaxSpeed(void) { return _vmax; }
        double getMaxSpeedDeg(void)
        {
            return _vmax * 360.0 / (double)_steps_per_rev;
        }
        double getAccel(void) { return _accel; }
        double getAccelDeg(void)
        {
            return _accel * 360.0 / (double)_steps_per_rev;
        }
        double getTargetPosition(void) { return _targetPos; }
        double getTargetPositionDeg(void)
        {
            return (double)_targetPos * 360.0 / (double)_steps_per_rev;
        }

        double getMaxSpeedMax(void) { return _vmaxMax; }
        double getMaxSpeedDegMax(void)
        {
            return _vmaxMax * 360.0 / (double)_steps_per_rev;
        }
        double getMaxSpeedMin(void) { return _vmaxMin; }
        double getMaxSpeedDegMin(void)
        {
            return _vmaxMin * 360.0 / (double)_steps_per_rev;
        }
        double getAccelMin(void) { return _accelMin; }
        double getAccelDegMin(void)
        {
            return _accelMin * 360.0 / (double)_steps_per_rev;
        }
        double getAccelMax(void) { return _accelMax; }
        double getAccelDegMax(void)
        {
            return _accelMax * 360.0 / (double)_steps_per_rev;
        }

        double getMaxPosition(void) { return _maxPos; }
        double getMaxPositionDeg(void)
        {
            return (double)_maxPos * 360.0 / (double)_steps_per_rev;
        }
        double getMinPosition(void) { return _minPos; }
        double getMinPositionDeg(void)
        {
            return (double)_minPos * 360.0 / (double)_steps_per_rev;
        }

    private:
        uint8_t _stepPin;
        uint8_t _dirPin;
        Counter* _counter;

        volatile long _position = 0;
        volatile double _curSpeed = 0.0;
        volatile double _accSteps = 0.0;
        volatile bool _reversing = false;

        double _vmax = 1500.0;
        double _accel = 8000.0;
        long _targetPos = 0;
        long _targetDuringReverse = 0;

        double _timerPeriod = 480e-6;
        uint16_t _timerSet = 120;

        long _steps_per_rev = 32000;
        long _minPos = 0;
        long _maxPos = 32000;
        double _vmaxMin = 1;
        double _vmaxMax = 4000.0;
        double _accelMin = 100.0;
        double _accelMax = 10000.0;
};

#endif