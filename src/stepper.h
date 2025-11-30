#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include "timer.h"

 

/**
 * Rotation mode enumeration for stepper movement
 */
enum RotaryMode : uint8_t {
    ROT_SHORTEST,    // shortest path to target
    ROT_CW,         // clockwise only
    ROT_CCW,        // counter-clockwise only
};

class Stepper 
{
    protected:

    public:
        void Setup ( uint8_t stepPin, uint8_t dirPin, 
                     Counter& counter, double timerPeriodSec,
                     long steps_per_rev,
                     long minPos, long maxPos );

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

        void renormalizePosition();
        bool homePosition();
        void setMaxSpeed(double vm);
        void setAcceleration(double a);


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
        double getMaxSpeed(void) {return _vmax;}
        double getAccel(void) { return _accel; }


    private:
        // GPIO pins for stepper motor control
        uint8_t _stepPin;         // pin for step signal
        uint8_t _dirPin;          // pin for direction signal
        Counter* _counter;        // pointer to timer counter for ISR timing

        // Position and movement state variables
        volatile long _position = 0;      // current position in steps
        volatile double _curSpeed = 0.0;  // current velocity in steps/sec
        volatile double _accSteps = 0.0;  // accumulator for fractional steps

        // Speed and acceleration parameters
        double _vmax = 1500.0;            // maximum velocity in steps/sec - can be changed dynamically
        double _accel = 8000.0;           // acceleration in steps/sec²
        long _targetPos = 0;              // target position to reach (steps)

        // Timer configuration
        double _timerPeriod = 480e-6;     // timer period in seconds (480 microseconds)
        uint16_t _timerSet = 120;         // timer counter reload value
        
        // Stepper configuration
        long _steps_per_rev = 32000;      // steps per revolution
        long _minPos = 0;                 // minimum allowed position
        long _maxPos = 32000;             // maximum allowed position

};    

#endif  // STEPPER_H