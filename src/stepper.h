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
        /*
        Setup the stepper motor with specified parameters
        stepPin: GPIO pin for step signal
        dirPin: GPIO pin for direction signal
        counter: Timer counter to use for ISR timing
        timerPeriodSec: Timer period in seconds
        steps_per_rev: Number of steps per full revolution
        minPos: Minimum allowed position (steps)
        maxPos: Maximum allowed position (steps)
        */
        void Setup ( uint8_t stepPin, uint8_t dirPin, 
                     Counter& counter, double timerPeriodSec,
                     long steps_per_rev,
                     long minPos, long maxPos );

        /*
         Run the stepper control logic in the ISR
         This function is called from the Timer1 compare interrupt
        */
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
            return !(_position == _targetPos && _curSpeed == 0.0 && _accSteps == 0 ); 
        }

        void renormalizePosition();
        bool homePosition();
        void setMaxSpeed(double vm);
        void setMaxSpeedDeg(double vmDeg)
        {
            setMaxSpeed( vmDeg * (double)_steps_per_rev / 360.0 );
        }
        void setAcceleration(double a);
        void setAccelerationDeg(double aDeg)
        {
            setAcceleration( aDeg * (double)_steps_per_rev / 360.0 );
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
        double getMaxSpeed(void) {return _vmax;}
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
        double _vmaxMin = 1;              // minimum allowed max speed (steps/sec)   
        double _vmaxMax = 4000.0;         // maximum allowed max speed (steps/sec)
        double _accelMin = 100.0;         // minimum allowed acceleration (steps/sec²)
        double _accelMax = 10000.0;       // maximum allowed acceleration (steps/sec

};    

#endif  // STEPPER_H