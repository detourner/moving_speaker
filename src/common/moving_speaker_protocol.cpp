#include "moving_speaker_protocol.h"

#include <stdlib.h>
#include <string.h>

MovingSpeakerProtocol::MovingSpeakerProtocol(Stream& serial,
                                             MotorChannel* motors,
                                             uint8_t motorCount,
                                             const char* infoTitle,
                                             bool infoRequestEnabled,
                                             bool statusPrefixSpace)
    : _serial(serial),
      _motors(motors),
      _motorCount(motorCount),
      _infoTitle(infoTitle),
      _infoRequestEnabled(infoRequestEnabled),
      _statusPrefixSpace(statusPrefixSpace)
{
}

void MovingSpeakerProtocol::process()
{
    if (millis() - _lastPositionFrame > 100) {
        _lastPositionFrame = millis();
        sendPositionFrame();
    }

    if (_serial.available()) {
        uint16_t length = _serial.readBytesUntil('\n', _buffer, sizeof(_buffer) - 1);
        _buffer[length] = '\0';

        if (_infoRequestEnabled && length == 1 && _buffer[0] == 'I') {
            sendInfoFrame();
            return;
        }

        processCommand(length);
    }
}

void MovingSpeakerProtocol::sendInfoFrame()
{
    _serial.println(_infoTitle);
    _serial.print("I:");

    for (uint8_t index = 0; index < _motorCount; ++index) {
        StepperCore& motor = *_motors[index].stepper;
        _serial.print(motor.getMinPositionDeg());
        _serial.print(",");
        _serial.print(motor.getMaxPositionDeg());
        _serial.print(",");
        _serial.print(motor.getMaxSpeedDegMin());
        _serial.print(",");
        _serial.print(motor.getMaxSpeedDegMax());
        _serial.print(",");
        _serial.print(motor.getAccelDegMin());
        _serial.print(",");
        _serial.print(motor.getAccelDegMax());

        if (index + 1 < _motorCount) _serial.print(",");
    }

    _serial.println();
    _serial.println("I: Ready");
}

void MovingSpeakerProtocol::sendPositionFrame()
{
    _serial.print("P:");
    if (_statusPrefixSpace) _serial.print(" ");

    for (uint8_t index = 0; index < _motorCount; ++index) {
        StepperCore& motor = *_motors[index].stepper;
        _serial.print(motor.isRunning());
        _serial.print(",");
        if (_motors[index].modulo)
            _serial.print(motor.getPositionModuloDeg());
        else
            _serial.print(motor.getPositionDeg());
        _serial.print(",");
        _serial.print(motor.getSpeedDeg());

        if (index + 1 < _motorCount) _serial.print(",");
    }
    _serial.println();

    for (uint8_t index = 0; index < _motorCount; ++index) {
        if (_motors[index].modulo)
            _motors[index].stepper->renormalizePosition();
    }
}

void MovingSpeakerProtocol::processCommand(uint16_t length)
{
    uint16_t commaCount = 0;
    for (uint16_t index = 0; index < length; ++index) {
        if (_buffer[index] == ',') ++commaCount;
    }

    uint16_t expectedFields = 0;
    for (uint8_t index = 0; index < _motorCount; ++index)
        expectedFields += _motors[index].modulo ? 4 : 3;

    if (commaCount != expectedFields - 1) {
        _serial.println("E:Invalid frame: wrong number of fields");
        return;
    }

    char* token = strtok(_buffer, ",");
    for (uint8_t index = 0; index < _motorCount; ++index) {
        double target;
        double speed;
        double acceleration;
        RotaryMode mode = ROT_SHORTEST;

        if (!parseDouble(token, target)) return;
        token = strtok(NULL, ",");
        if (!parseDouble(token, speed)) return;

        if (_motors[index].modulo) {
            token = strtok(NULL, ",");
            if (!parseMode(token, mode)) return;
            token = strtok(NULL, ",");
            if (!parseDouble(token, acceleration)) return;
        } else {
            token = strtok(NULL, ",");
            if (!parseDouble(token, acceleration)) return;
        }

        StepperCore& motor = *_motors[index].stepper;
        motor.setAccelerationDeg(acceleration);
        motor.setMaxSpeedDeg(speed);
        if (_motors[index].modulo)
            motor.moveToModuloDeg(target, mode);
        else
            motor.moveToWithLimitsDeg(target);
    }

    sendStateFrame();
}

bool MovingSpeakerProtocol::parseDouble(char*& token, double& value)
{
    if (!token) return false;
    value = atof(token);
    return true;
}

bool MovingSpeakerProtocol::parseMode(char*& token, RotaryMode& mode)
{
    if (!token) return false;
    mode = (RotaryMode)atoi(token);
    return true;
}

void MovingSpeakerProtocol::sendStateFrame()
{
    _serial.print("S: ");
    for (uint8_t index = 0; index < _motorCount; ++index) {
        StepperCore& motor = *_motors[index].stepper;
        _serial.print(motor.isRunning());
        _serial.print(",");
        _serial.print(motor.getTargetPositionDeg());
        _serial.print(",");
        _serial.print(motor.getMaxSpeedDeg());
        _serial.print(",");
        _serial.print(motor.getAccelDeg());

        if (index + 1 < _motorCount) _serial.print(",");
    }
    _serial.println();
}