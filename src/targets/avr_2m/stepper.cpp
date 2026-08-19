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
    _timerSet = (_timerPeriod * 1000000.0) / _counter->getTicksPeruSec();
    _vmaxMax = 1.0 / _timerPeriod;
    _accelMax = _vmaxMax / _timerPeriod;

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

    if (_reversing && fabs(_curSpeed) < 1e-6) {
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
    double v_peak = sqrt(2.0 * _accel * d);
    double v_target = min(_vmax, v_peak);

    if (_reversing) {
        v_target = 0;
    }

    if (fabs(_curSpeed) < v_target) {
        _curSpeed += dir * _accel * _timerPeriod;
        if (fabs(_curSpeed) > v_target) _curSpeed = dir * v_target;
    } else if (fabs(_curSpeed) > v_target) {
        _curSpeed -= dir * _accel * _timerPeriod;
        if (fabs(_curSpeed) < v_target) _curSpeed = dir * v_target;
    }

    _accSteps += _curSpeed * _timerPeriod;

    if (_accSteps >= 1.0 || _accSteps <= -1.0) {
        int stepDir = (_accSteps > 0 ? 1 : -1);
        long nextPos = _position + stepDir;

        if ((stepDir > 0 && nextPos >= _targetPos) ||
            (stepDir < 0 && nextPos <= _targetPos)) {
            if (_reversing == false)
            {
                _position = _targetPos;
            }
            _accSteps = 0;
            _curSpeed = 0;
        } else {
            if (stepDir > 0)
                digitalWriteFast(_dirPin, HIGH);
            else
                digitalWriteFast(_dirPin, LOW);

            digitalWriteFast(_stepPin, HIGH);
            __asm__("nop\nnop\nnop\nnop\nnop\nnop");
            digitalWriteFast(_stepPin, LOW);

            _position = nextPos;
            _accSteps -= stepDir;
        }
    }
}

void Stepper::renormalizePosition()
{
    if (isRunning() == false)
    {
        int32_t modulo = _position % _steps_per_rev;
        if (modulo < 0) modulo += _steps_per_rev;

        int32_t tgt = _targetPos % _steps_per_rev;
        if (tgt < 0) tgt += _steps_per_rev;

        if (modulo != _position || tgt != _targetPos)
        {
            cli();
            _position = modulo;
            _targetPos = tgt;
            sei();
        }
    }
}

bool Stepper::homePosition()
{
    if (_curSpeed != 0.0 || _accSteps != 0.0)
        return false;

    cli();
    _position = 0;
    _targetPos = 0;
    _curSpeed = 0.0;
    _accSteps = 0.0;
    sei();
    return true;
}

void Stepper::moveToModuloSteps(long targetModulo, RotaryMode mode)
{
    if (targetModulo > _maxPos) targetModulo = _maxPos;
    if (targetModulo < _minPos) targetModulo = _minPos;

    targetModulo %= _steps_per_rev;
    if (targetModulo < 0) targetModulo += _steps_per_rev;

    long posModulo = _position % _steps_per_rev;
    if (posModulo < 0) posModulo += _steps_per_rev;

    long cwDist = (targetModulo - posModulo);
    if (cwDist < 0) cwDist += _steps_per_rev;

    long ccwDist = (posModulo - targetModulo);
    if (ccwDist < 0) ccwDist += _steps_per_rev;

    long finalTarget = _position;

    switch (mode)
    {
        case ROT_CW:
            finalTarget += cwDist;
            break;

        case ROT_CCW:
            finalTarget -= ccwDist;
            break;

        case ROT_SHORTEST:
        default:
            if (cwDist <= ccwDist)
                finalTarget += cwDist;
            else
                finalTarget -= ccwDist;
            break;
    }

    moveToSteps(finalTarget);
}

void Stepper::moveToWithLimitsSteps(long absolute)
{
    if (absolute > _maxPos) absolute = _maxPos;
    if (absolute < _minPos) absolute = _minPos;

    moveToSteps(absolute);
}

void Stepper::moveToSteps(long absolute)
{
    if (absolute != _targetPos)
    {
        long currentDir = _targetPos - _position;
        long newDir = absolute - _position;

        if (isRunning() && (currentDir * newDir < 0))
        {
            _reversing = true;
            _targetDuringReverse = absolute;
        } else {
            _targetPos = absolute;
            _reversing = false;
        }
    }
}

void Stepper::setMaxSpeed(double vm)
{
    if (vm < 0) vm = -vm;

    if (vm < _vmaxMin) vm = _vmaxMin;
    if (vm > _vmaxMax) vm = _vmaxMax;

    if (_vmax != vm)
        _vmax = vm;
}

void Stepper::setAcceleration(double accel)
{
    if (accel < 0) accel = -accel;

    if (accel < _accelMin) accel = _accelMin;
    if (accel > _accelMax) accel = _accelMax;

    if (_accel != accel && isRunning() == false)
        _accel = accel;
}