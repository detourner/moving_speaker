#include "stepper.h"
#include "digitalWriteFast.h"

void Stepper::Setup(uint8_t stepPin, uint8_t dirPin, Counter& counter, double timerPeriodSec)
{
    _stepPin = stepPin;
    _dirPin = dirPin;
    _counter = &counter;

    _timerPeriod = timerPeriodSec;

    // calcul de la valeur à charger dans le compteur pour la période désirée
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

    // vitesse maximale atteignable
    double v_peak = sqrt(2.0 * _accel * d);
    double v_target = min(_vmax, v_peak);

    // ---- changement de sens ----
    if (_curSpeed * dir < 0) {
        // vitesse opposée → décélération uniquement
        double dv = _accel * _timerPeriod;
        if (fabs(_curSpeed) <= dv) {
            _curSpeed = 0;
            _accSteps = 0;  // reset accumulateur pour éviter steps fantômes
        } else {
            _curSpeed += (_curSpeed > 0 ? -dv : dv);  // décélérer vers 0
        }
    }
    else {
        // approche progressive vers v_target
        if (fabs(_curSpeed) < v_target) {
            _curSpeed += dir * _accel * _timerPeriod;
            if (fabs(_curSpeed) > v_target) _curSpeed = dir * v_target;
        } else if (fabs(_curSpeed) > v_target) {
            _curSpeed -= dir * _accel * _timerPeriod;
            if (fabs(_curSpeed) < v_target) _curSpeed = dir * v_target;
        }

        // accumulation fractionnaire seulement après vitesse compatible
        _accSteps += _curSpeed * _timerPeriod;

        // step unique si accumulateur >= 1 ou <= -1
        if (_accSteps >= 1.0 || _accSteps <= -1.0) {
            int stepDir = (_accSteps > 0 ? 1 : -1);
            long nextPos = _position + stepDir;

            if ((stepDir > 0 && nextPos >= _targetPos) ||
                (stepDir < 0 && nextPos <= _targetPos)) {
                _position = _targetPos;
                _accSteps = 0;
                _curSpeed = 0;
            } else {

                if (stepDir > 0) 
                    digitalWriteFast(_dirPin, HIGH);
                else         
                    digitalWriteFast(_dirPin, LOW);

                digitalWriteFast(_stepPin, HIGH);
                // très court délai (1–2 cycles)
                digitalWriteFast(_stepPin, LOW);


                _position = nextPos;
                _accSteps -= stepDir;
            }
        }
    }
}

void Stepper::moveTo(long absolute)
{
    if( absolute != _targetPos) 
        _targetPos = absolute;
}

void Stepper::setMaxSpeed(double vm)
{
    if(vm < 0) vm = -vm;

    if(_vmax != vm)
        _vmax = vm;
}

void Stepper::setAcceleration(double accel)
{
    if(accel < 0) accel = -accel;
    if(_accel != accel)   
    _accel = accel;
}



