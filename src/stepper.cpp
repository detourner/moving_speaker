#include "stepper.h"

#include "digitalWriteFast.h"

void Stepper::Setup(uint8_t stepPin, uint8_t dirPin, Counter& counter)
{
    _stepPin = stepPin;
    _dirPin = dirPin;
    _counter = &counter;
    pinModeFast(_stepPin, OUTPUT);
    pinModeFast(_dirPin, OUTPUT);
}

void Stepper::DoStep(void)
{
    digitalWriteFast(_dirPin, _direction == DIRECTION_CW );
    
    if(_stepInterval != 0)
    {
        digitalWriteFast(_stepPin, HIGH);

        if (_direction == DIRECTION_CW)
        {
            // Clockwise
            _currentPos += 1;
        }
        else
        {
            // Anticlockwise  
            _currentPos -= 1;
        }

        computeNewSpeed();
    

        if(_speed != 0.0 || distanceToGo() != 0)
        {
            //_debug = ((float)_stepInterval * (float)Counter::getTicksPerSec() / 1000000.0f);
            //_counter->Set((float)_stepInterval * (float)Counter::getTicksPerSec() / 1000000.0f);
            _debug = _stepInterval / Counter::getTicksPeruSec();
            _counter->Set(_stepInterval / Counter::getTicksPeruSec());
        }
        digitalWriteFast(_stepPin, LOW);
    }
}

void Stepper::moveTo(int32_t absolute)
{
    if (absolute != _targetPos)
    {
        _targetPos = absolute;
        cli();
        computeNewSpeed();
        DoStep();
        sei();
    }
}


void Stepper::setMaxSpeed(float speed)
{
    if (speed < 0.0)
       speed = -speed;
    if (_maxSpeed != speed)
    {
        _maxSpeed = speed;
        _cmin = 1000000.0 / speed;
        

        cli();
        computeNewSpeed();
        DoStep();
        sei();
    }
}



void Stepper::setAcceleration(float acceleration)
{
    if (acceleration == 0.0)
	    return;
    if (acceleration < 0.0)
      acceleration = -acceleration;
    if (_acceleration != acceleration)
    {
        // Recompute _n per Equation 17
        _n = _n * (_acceleration / acceleration);
        // New c0 per Equation 7, with correction per Equation 15
        _c0 = 0.676 * sqrt(2.0 / acceleration) * 1000000.0; // Equation 15
        _acceleration = acceleration;
        cli();
        computeNewSpeed();
        DoStep();
        sei();
    }
}

void Stepper::stop()
{
    if (_speed != 0.0)
    {    
	long stepsToStop = (long)((_speed * _speed) / (2.0 * _acceleration)) + 1; // Equation 16 (+integer rounding)
	if (_speed > 0)
	    moveTo(_currentPos + stepsToStop);
	else
	    moveTo(_currentPos - stepsToStop);
    }
}

void Stepper::computeNewSpeed()
{
    int32_t distanceTo = distanceToGo(); // +ve is clockwise from curent location

    int32_t stepsToStop = (int32_t)((_speed * _speed) / (2.0 * _acceleration)); // Equation 16
    

    if (distanceTo == 0 && stepsToStop <= 1)
    {
        // We are at the target and its time to stop
        _stepInterval = 0;
        _speed = 0.0;
        _n = 0;
        return;
    }

    if (distanceTo > 0)
    {
        // We are anticlockwise from the target
        // Need to go clockwise from here, maybe decelerate now
        if (_n > 0)
        {
            // Currently accelerating, need to decel now? Or maybe going the wrong way?
            if ((stepsToStop >= distanceTo) || _direction == DIRECTION_CCW)
            _n = -stepsToStop; // Start deceleration
        }
        else if (_n < 0)
        {
            // Currently decelerating, need to accel again?
            if ((stepsToStop < distanceTo) && _direction == DIRECTION_CW)
            _n = -_n; // Start accceleration
        }
    }
    else if (distanceTo < 0)
    {
        // We are clockwise from the target
        // Need to go anticlockwise from here, maybe decelerate
        if (_n > 0)
        {
            // Currently accelerating, need to decel now? Or maybe going the wrong way?
            if ((stepsToStop >= -distanceTo) || _direction == DIRECTION_CW)
            _n = -stepsToStop; // Start deceleration
        }
        else if (_n < 0)
        {
            // Currently decelerating, need to accel again?
            if ((stepsToStop < -distanceTo) && _direction == DIRECTION_CCW)
            _n = -_n; // Start accceleration
        }
    }

    // Need to accelerate or decelerate
    if (_n == 0)
    {
        // First step from stopped
        _cn = _c0;
        _direction = (distanceTo > 0) ? DIRECTION_CW : DIRECTION_CCW;
    }
    else
    {
        // Subsequent step. Works for accel (n is +_ve) and decel (n is -ve).
        _cn = _cn - ((2.0 * _cn) / ((4.0 * _n) + 1)); // Equation 13
        _cn = max(_cn, _cmin); 
    }
    _n++;
    _stepInterval = _cn;
    _speed = 1000000.0 / _cn;

    if (_speed > _maxSpeed)
        _speed = _maxSpeed;
    else if (_speed < _maxSpeed)
        _n = (long)((_maxSpeed * _maxSpeed) / (2.0 * _acceleration));
    //if (_speed < -_maxSpeed)
    //    _speed = -_maxSpeed;

    //if (_direction == DIRECTION_CCW)
	//   _speed = -_speed;
}

