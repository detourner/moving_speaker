#include <Arduino.h>
#include "stepper.h"
#include "digitalWriteFast.h"

static hw_timer_t* groupTimers[2] = { nullptr, nullptr };
static Stepper* groupInstances[2][2] = {{ nullptr, nullptr }, { nullptr, nullptr }};

void IRAM_ATTR timerGroupISR0() {
    if (groupInstances[0][0]) groupInstances[0][0]->RunISR();
    if (groupInstances[0][1]) groupInstances[0][1]->RunISR();
}

void IRAM_ATTR timerGroupISR1() {
    if (groupInstances[1][0]) groupInstances[1][0]->RunISR();
    if (groupInstances[1][1]) groupInstances[1][1]->RunISR();
}

void Stepper::Setup(uint8_t stepPin, uint8_t dirPin,
    uint8_t timerNum,
    double timerPeriodSec,
    long steps_per_rev, long minPos, long maxPos)
{
    _timerNum = timerNum;
    _stepPin = stepPin;
    _dirPin = dirPin;
    _steps_per_rev = steps_per_rev;
    _minPos = minPos;
    _maxPos = maxPos;

    homePosition();

    _timerPeriod = timerPeriodSec;

    // Calculate timer counter value for desired period in microseconds
    _timerSet = (uint64_t)round(_timerPeriod * 1000000.0);
    Serial.print("I: Timer period set to ");
    Serial.println(_timerSet);
    _vmaxMax = 1.0 / _timerPeriod; // one step per timer period
    _accelMax = _vmaxMax / _timerPeriod; // reach max speed in one timer period

    uint8_t group = (timerNum < 2) ? 0 : 1;
    uint8_t indexInGroup = timerNum % 2;

    if (!groupTimers[group]) {
        groupTimers[group] = timerBegin(1000000);
        if (groupTimers[group]) {
            timerAttachInterrupt(groupTimers[group],
                (group == 0) ? timerGroupISR0 : timerGroupISR1);
            timerAlarm(groupTimers[group], _timerSet, true, 0);
            timerStart(groupTimers[group]);
        }
    }

    _timer = groupTimers[group];
    groupInstances[group][indexInGroup] = this;

    pinModeFast(_stepPin, OUTPUT);
    pinModeFast(_dirPin, OUTPUT);
}

void Stepper::RunISR(void)
{
    long dist = _targetPos - _position;
    double d = abs(dist);

    if(_reversing && fabs(_curSpeed) < 1e-6) {
        // if reversing and speed is near zero, apply the new target
        _curSpeed = 0.0;
        _accSteps = 0; 
        _targetPos = _targetDuringReverse;
        _reversing = false;
        return;
    }

    if (dist == 0 && fabs(_curSpeed) < 1e-6) {
        _curSpeed = 0;
        _accSteps = 0;        
        return;
    }

    int dir = (dist >= 0 ? 1 : -1);

    // Maximum achievable velocity based on remaining distance
    double v_peak = sqrt(2.0 * _accel * d);
    double v_target = min(_vmax, v_peak);

    if (_reversing) {
        // if reversing, target speed is zero (for deceleration to stop)
        v_target = 0;
    }

    // Progressive acceleration/deceleration towards v_target
    if (fabs(_curSpeed) < v_target) {
        // accelerating
        _curSpeed += dir * _accel * _timerPeriod;
        if (fabs(_curSpeed) > v_target) _curSpeed = dir * v_target;
    } else if (fabs(_curSpeed) > v_target) {
       // decelerating
        _curSpeed -= dir * _accel * _timerPeriod;
        if (fabs(_curSpeed) < v_target) _curSpeed = dir * v_target;
    }

    // Accumulate fractional steps for smooth motion
    _accSteps += _curSpeed * _timerPeriod;

    // Single step if accumulator reaches ±1
    if (_accSteps >= 1.0 || _accSteps <= -1.0) {
        int stepDir = (_accSteps > 0 ? 1 : -1);
        long nextPos = _position + stepDir;
        bool reachedOrPast = (stepDir > 0 && nextPos >= _targetPos) ||
                              (stepDir < 0 && nextPos <= _targetPos);

        // While reversing, the old target is about to be replaced by
        // _targetDuringReverse: never step towards it, just let speed decay to 0
        if (_reversing && reachedOrPast) {
            _accSteps = 0;
            _curSpeed = 0;
        } else if (!_reversing && (stepDir > 0 ? nextPos > _targetPos : nextPos < _targetPos)) {
            // Would overshoot the target: clamp without stepping
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
            delayMicroseconds(1); // Ensure minimum pulse width
            digitalWriteFast(_stepPin, LOW);

            _position = nextPos;
            _accSteps -= stepDir;

            // Landed exactly on target: stop cleanly
            if (!_reversing && reachedOrPast) {
                _accSteps = 0;
                _curSpeed = 0;
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
    if (isRunning() == false)
    {
        int32_t modulo = _position % _steps_per_rev;
        if (modulo < 0) modulo += _steps_per_rev;

        // Renormalize target position as well
        int32_t tgt = _targetPos % _steps_per_rev;
        if (tgt < 0) tgt += _steps_per_rev;

        // Update only if changed
        if(modulo != _position || tgt != _targetPos)
        {
            noInterrupts();
            _position = modulo;
            _targetPos = tgt;
            interrupts();
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
    
    noInterrupts();
    _position = 0;
    _targetPos = 0;
    _curSpeed = 0.0;
    _accSteps = 0.0;
    interrupts();
    return true;
}

/**
 * Move to target position modulo steps_per_rev
 * Allows rotary movement with various rotation modes
 */
void Stepper::moveToModuloSteps(long targetModulo, RotaryMode mode)
{
    // Normalize target to range [0, steps_per_rev) first: clamping to min/max
    // before wrapping would crush any out-of-range value (e.g. negative degrees)
    // to a bound instead of correctly wrapping it around the circle.
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
    { 
        long currentDir = _targetPos - _position; // acutal direction
        long newDir = absolute - _position; // desired direction

        if (isRunning() && (currentDir * newDir < 0)) // opposite directions and motor is running
        {
            // If moving and new target is in opposite direction, initiate reversing
            // and not apply target yet, just store it
            _reversing = true;
            _targetDuringReverse = absolute;
        } else {
            // no reversing, set target directly
            _targetPos = absolute;
            _reversing = false;
        }
    }
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

    if(_accel != accel && isRunning() == false)   
        _accel = accel;
}



