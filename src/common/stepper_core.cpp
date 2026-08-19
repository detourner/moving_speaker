#include "stepper_core.h"
#include <digitalWriteFast.h>

void StepperCore::Setup(uint8_t stepPin, uint8_t dirPin,
                        double timerPeriodSec, long steps_per_rev,
                        long minPos, long maxPos)
{
    configurePins(stepPin, dirPin);
    configureMotion(timerPeriodSec, steps_per_rev, minPos, maxPos);

    pinModeFast(_stepPin, OUTPUT);
    pinModeFast(_dirPin, OUTPUT);
}

void StepperCore::configureMotion(double timerPeriodSec, long stepsPerRev,
                                  long minPos, long maxPos)
{
    _steps_per_rev = stepsPerRev;
    _minPos = minPos;
    _maxPos = maxPos;
    homePosition();

    _timerPeriod = timerPeriodSec;
    _vmaxMax = 1.0 / _timerPeriod;
    _accelMax = _vmaxMax / _timerPeriod;
}

void StepperCore::RunISR()
{
    long dist = _targetPos - _position;
    double distance = dist >= 0 ? dist : -dist;

    if (_reversing && fabs(_curSpeed) < 1e-6) {
        _curSpeed = 0.0;
        _accSteps = 0;
        _targetPos = _targetDuringReverse;
        _reversing = false;
        return;
    }

    if (dist == 0 && fabs(_curSpeed) < 1e-6) {
        _curSpeed = 0.0;
        _accSteps = 0.0;
        return;
    }

    int direction = dist >= 0 ? 1 : -1;
    double peakSpeed = sqrt(2.0 * _accel * distance);
    double targetSpeed = _vmax < peakSpeed ? _vmax : peakSpeed;

    if (_reversing) targetSpeed = 0.0;

    if (fabs(_curSpeed) < targetSpeed) {
        _curSpeed += direction * _accel * _timerPeriod;
        if (fabs(_curSpeed) > targetSpeed) _curSpeed = direction * targetSpeed;
    } else if (fabs(_curSpeed) > targetSpeed) {
        _curSpeed -= direction * _accel * _timerPeriod;
        if (fabs(_curSpeed) < targetSpeed) _curSpeed = direction * targetSpeed;
    }

    _accSteps += _curSpeed * _timerPeriod;

    if (_accSteps >= 1.0 || _accSteps <= -1.0) {
        int stepDirection = _accSteps > 0 ? 1 : -1;
        long nextPosition = _position + stepDirection;
        bool reachedOrPast = (stepDirection > 0 && nextPosition >= _targetPos) ||
                             (stepDirection < 0 && nextPosition <= _targetPos);

        if (_reversing && reachedOrPast) {
            _accSteps = 0.0;
            _curSpeed = 0.0;
        } else if (!_reversing &&
                   (stepDirection > 0 ? nextPosition > _targetPos
                                      : nextPosition < _targetPos)) {
            _position = _targetPos;
            _accSteps = 0.0;
            _curSpeed = 0.0;
        } else {
            emitStep(stepDirection);
            _position = nextPosition;
            _accSteps -= stepDirection;

            if (!_reversing && reachedOrPast) {
                _accSteps = 0.0;
                _curSpeed = 0.0;
            }
        }
    }
}

void StepperCore::emitStep(int direction)
{
    digitalWriteFast(_dirPin, direction > 0 ? HIGH : LOW);
    digitalWriteFast(_stepPin, HIGH);
    delayMicroseconds(1);
    digitalWriteFast(_stepPin, LOW);
}

void StepperCore::enterCritical()
{
#if defined(ARDUINO_ARCH_ESP32)
    noInterrupts();
#else
    cli();
#endif
}

void StepperCore::leaveCritical()
{
#if defined(ARDUINO_ARCH_ESP32)
    interrupts();
#else
    sei();
#endif
}

void StepperCore::renormalizePosition()
{
    if (!isRunning()) {
        long positionModulo = _position % _steps_per_rev;
        if (positionModulo < 0) positionModulo += _steps_per_rev;

        long targetModulo = _targetPos % _steps_per_rev;
        if (targetModulo < 0) targetModulo += _steps_per_rev;

        if (positionModulo != _position || targetModulo != _targetPos) {
            enterCritical();
            _position = positionModulo;
            _targetPos = targetModulo;
            leaveCritical();
        }
    }
}

bool StepperCore::homePosition()
{
    if (_curSpeed != 0.0 || _accSteps != 0.0) return false;

    enterCritical();
    _position = 0;
    _targetPos = 0;
    _curSpeed = 0.0;
    _accSteps = 0.0;
    leaveCritical();
    return true;
}

void StepperCore::moveToModuloSteps(long targetModulo, RotaryMode mode)
{
    targetModulo %= _steps_per_rev;
    if (targetModulo < 0) targetModulo += _steps_per_rev;

    long positionModulo = _position % _steps_per_rev;
    if (positionModulo < 0) positionModulo += _steps_per_rev;

    long clockwiseDistance = targetModulo - positionModulo;
    if (clockwiseDistance < 0) clockwiseDistance += _steps_per_rev;

    long counterClockwiseDistance = positionModulo - targetModulo;
    if (counterClockwiseDistance < 0) counterClockwiseDistance += _steps_per_rev;

    long finalTarget = _position;
    switch (mode) {
        case ROT_CW:
            finalTarget += clockwiseDistance;
            break;
        case ROT_CCW:
            finalTarget -= counterClockwiseDistance;
            break;
        case ROT_SHORTEST:
        default:
            if (clockwiseDistance <= counterClockwiseDistance)
                finalTarget += clockwiseDistance;
            else
                finalTarget -= counterClockwiseDistance;
            break;
    }

    moveToSteps(finalTarget);
}

void StepperCore::moveToWithLimitsSteps(long absolute)
{
    if (absolute > _maxPos) absolute = _maxPos;
    if (absolute < _minPos) absolute = _minPos;
    moveToSteps(absolute);
}

void StepperCore::moveToSteps(long absolute)
{
    if (absolute != _targetPos) {
        long currentDirection = _targetPos - _position;
        long newDirection = absolute - _position;

        if (isRunning() && currentDirection * newDirection < 0) {
            _reversing = true;
            _targetDuringReverse = absolute;
        } else {
            _targetPos = absolute;
            _reversing = false;
        }
    }
}

void StepperCore::setMaxSpeed(double vm)
{
    if (vm < 0) vm = -vm;
    if (vm < _vmaxMin) vm = _vmaxMin;
    if (vm > _vmaxMax) vm = _vmaxMax;
    _vmax = vm;
}

void StepperCore::setAcceleration(double accel)
{
    if (accel < 0) accel = -accel;
    if (accel < _accelMin) accel = _accelMin;
    if (accel > _accelMax) accel = _accelMax;
    if (_accel != accel && !isRunning()) _accel = accel;
}