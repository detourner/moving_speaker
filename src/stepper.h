#ifndef STEPPER_H
#define STEPPER_H

#include <stdint.h>
#include "timer.h"

class Stepper 
{
    protected:
       
        typedef enum
        {
            DIRECTION_CCW = 0,  ///< Counter-Clockwise
            DIRECTION_CW  = 1   ///< Clockwise
        } Direction;
    
    public:
        void Setup(uint8_t stepPin, uint8_t dirPin, Counter& counter);
        void DoStep(void);

        float maxSpeed() const { return _maxSpeed; }
        void setMaxSpeed(float speed);



        float acceleration() const { return _acceleration; }
        void setAcceleration(float acceleration);

        int32_t distanceToGo() const { return _targetPos - _currentPos; }

        int32_t currentPosition() const { return _currentPos; }

        void moveTo(int32_t absolute);
        void stop();

        uint16_t debug() const { return _debug; }
        unsigned long stepInterval() const { return _stepInterval; } 
        float speed() const { return _speed; }  


    private:

        bool runSpeed();
        void computeNewSpeed();

        uint8_t _stepPin;
        uint8_t _dirPin;
        Counter* _counter;


        int32_t _targetPos;
        int32_t _currentPos;

    /// Current direction motor is spinning in
    /// Protected because some peoples subclasses need it to be so
    boolean _direction; // 1 == CW, 0 == CCW

    /// The current motos speed in steps per second
    /// Positive is clockwise
    volatile float          _speed;         // Steps per second

    /// The maximum permitted speed in steps per second. Must be > 0.
    volatile float          _maxSpeed;

    /// The acceleration to use to accelerate or decelerate the motor in steps
    /// per second per second. Must be > 0
    volatile float          _acceleration;
    volatile float          _sqrt_twoa; // Precomputed sqrt(2*_acceleration)

    /// The current interval between steps in microseconds.
    /// 0 means the motor is currently stopped with _speed == 0
    volatile unsigned long  _stepInterval;

     /// The step counter for speed calculations
    volatile long _n;

    /// Initial step size in microseconds
    volatile float _c0;

    /// Last step size in microseconds
    volatile float _cn;

    /// Min step size in microseconds based on maxSpeed
    volatile float _cmin; // at max speed

    volatile uint16_t _debug;

};    

#endif  // STEPPER_H