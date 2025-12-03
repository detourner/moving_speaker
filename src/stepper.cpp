#include "stepper.h"
#include "digitalWriteFast.h"

void Stepper::Setup(uint8_t stepPin, uint8_t dirPin, 
    Counter& counter, double timerPeriodSec,
    long steps_per_rev, long minPos, long maxPos)
{
    _stepPin = stepPin;
    _dirPin = dirPin;
    _counter = &counter;
    _steps_per_rev = steps_per_rev;
    _minPos = minPos;
    _maxPos = maxPos;

    homePosition();

    _timerPeriod = timerPeriodSec;

    // Calculate timer counter value for desired period
    _timerSet =  (_timerPeriod * 1000000.0) / _counter->getTicksPeruSec();

    Serial.print("Timer set to ");
    Serial.println(_timerSet);

    _counter->Set(_timerSet);
    _counter->Enable();

    pinModeFast(_stepPin, OUTPUT);
    pinModeFast(_dirPin, OUTPUT);
}

void Stepper::RunISR(void)
{
    _counter->Set(_timerSet);
    long dist = _targetPos - _position;
    double d = abs(dist);

    if (dist == 0 && fabs(_curSpeed) < 1e-6) {
        _curSpeed = 0;
        _accSteps = 0;
        return;
    }

    int dir = (dist >= 0 ? 1 : -1);

    // Maximum achievable velocity based on remaining distance
    double v_peak = sqrt(2.0 * _accel * d);
    double v_target = min(_vmax, v_peak);

    // Handle direction changes
    if (_curSpeed * dir < 0) {
        // Opposite velocity direction - decelerate only
        double dv = _accel * _timerPeriod;
        if (fabs(_curSpeed) <= dv) {
            _curSpeed = 0;
            _accSteps = 0;  // reset accumulator to avoid phantom steps
        } else {
            _curSpeed += (_curSpeed > 0 ? -dv : dv);  // decelerate towards zero
        }
    }
    else {
        // Progressive acceleration/deceleration towards v_target
        if (fabs(_curSpeed) < v_target) {
            _curSpeed += dir * _accel * _timerPeriod;
            if (fabs(_curSpeed) > v_target) _curSpeed = dir * v_target;
        } else if (fabs(_curSpeed) > v_target) {
            _curSpeed -= dir * _accel * _timerPeriod;
            if (fabs(_curSpeed) < v_target) _curSpeed = dir * v_target;
        }

        // Accumulate fractional steps for smooth motion
        _accSteps += _curSpeed * _timerPeriod;

        // Single step if accumulator reaches ±1
        if (_accSteps >= 1.0 || _accSteps <= -1.0) {
            int stepDir = (_accSteps > 0 ? 1 : -1);
            long nextPos = _position + stepDir;

            if ((stepDir > 0 && nextPos >= _targetPos) ||
                (stepDir < 0 && nextPos <= _targetPos)) {
                _position = _targetPos;
                _accSteps = 0;
                _curSpeed = 0;
            } else {

                // Set direction pin
                if (stepDir > 0) 
                    digitalWriteFast(_dirPin, HIGH);
                else         
                    digitalWriteFast(_dirPin, LOW);

                // Generate step pulse
                digitalWriteFast(_stepPin, HIGH);
                __asm__("nop\nnop\nnop\nnop\nnop\nnop");  // Small CPU delay for step pulse width
                digitalWriteFast(_stepPin, LOW);


                _position = nextPos;
                _accSteps -= stepDir;
            }
        }
    }
}

/**
 * Renormalize position to modulo range [0, _steps_per_rev)
 * Only performs renormalization when motor is at rest
 */
void Stepper::renormalizePosition()
{
    // Only if motor is at rest
    if (_curSpeed == 0.0 && _accSteps == 0) 
    {
        int32_t modulo = _position % _steps_per_rev;
        if (modulo < 0) modulo += _steps_per_rev;

        // Renormalize target position as well
        int32_t tgt = _targetPos % _steps_per_rev;
        if (tgt < 0) tgt += _steps_per_rev;

        // Update only if changed
        if(modulo != _position || tgt != _targetPos)
        {
            cli();
            _position = modulo;
            _targetPos = tgt;
            sei();
        }
    }
}

/**
 * Set home position (reset position to zero)
 * Cannot home while motor is moving
 */
bool Stepper::homePosition()
{
    if (_curSpeed != 0.0 || _accSteps != 0.0) 
        return false; // cannot home while moving
    
    cli();
    _position = 0;
    _targetPos = 0;
    _curSpeed = 0.0;
    _accSteps = 0.0;
    sei();
    return true;
}

/**
 * Move to target position modulo steps_per_rev
 * Allows rotary movement with various rotation modes
 */
void Stepper::moveToModuloSteps(long targetModulo, RotaryMode mode)
{
    if( targetModulo > _maxPos) targetModulo = _maxPos;
    if( targetModulo < _minPos) targetModulo = _minPos;
    
    // Normalize target to range [0, steps_per_rev)
    targetModulo %= _steps_per_rev;
    if (targetModulo < 0) targetModulo += _steps_per_rev;

    // Get current position modulo steps_per_rev
    long posModulo = _position % _steps_per_rev;
    if (posModulo < 0) posModulo += _steps_per_rev;

    // Calculate distances in both directions
    long cwDist  = (targetModulo - posModulo);  // clockwise distance
    if (cwDist < 0) cwDist += _steps_per_rev;

    long ccwDist = (posModulo - targetModulo);  // counter-clockwise distance
    if (ccwDist < 0) ccwDist += _steps_per_rev;

    long finalTarget = _position; // will be modified based on mode

    switch(mode)
    {
        case ROT_CW:
            finalTarget += cwDist;   // rotate clockwise only
            break;

        case ROT_CCW:
            finalTarget -= ccwDist;  // rotate counter-clockwise only
            break;

        case ROT_SHORTEST:
        default:
            if (cwDist <= ccwDist)
                finalTarget += cwDist;   // rotate clockwise (shorter path)
            else
                finalTarget -= ccwDist;  // rotate counter-clockwise (shorter path)
            break;
    }

    // Call internal moveTo function
    moveToSteps(finalTarget);
}

/**
 * Move to absolute position with min/max limits applied
 */
void Stepper::moveToWithLimitsSteps(long absolute)
{
    if( absolute > _maxPos) absolute = _maxPos;
    if( absolute < _minPos) absolute = _minPos;

    moveToSteps(absolute);
}

/**
 * Set target position in steps
 * The motor will accelerate/decelerate to reach this position
 */
void Stepper::moveToSteps(long absolute)
{
    if( absolute != _targetPos) 
        _targetPos = absolute;
}

/**
 * Set maximum velocity
 * Takes absolute value to ensure positive speed
 */
void Stepper::setMaxSpeed(double vm)
{
    if(vm < 0) vm = -vm;

    if(vm < _vmaxMin) vm = _vmaxMin;
    if(vm > _vmaxMax) vm = _vmaxMax;

    if(_vmax != vm)
        _vmax = vm;
}

/**
 * Set acceleration
 * Takes absolute value to ensure positive acceleration
 */
void Stepper::setAcceleration(double accel)
{
    if(accel < 0) accel = -accel;

    if(accel < _accelMin) accel = _accelMin;
    if(accel > _accelMax) accel = _accelMax;

    if(_accel != accel)   
    _accel = accel;
}



