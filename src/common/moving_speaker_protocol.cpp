#include "moving_speaker_protocol.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {
struct ParsedMotorCommand
{
    double target;
    double speed;
    double acceleration;
    RotaryMode mode;
};
}

MovingSpeakerProtocol::MovingSpeakerProtocol(Stream& serial,
                                             MotorChannel* motors,
                                             uint8_t motorCount,
                                             const char* infoTitle)
    : _serial(serial),
      _motors(motors),
      _motorCount(motorCount),
    _infoTitle(infoTitle)
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

        if (length == sizeof(_buffer) - 1) {
            _serial.find('\n');
            _serial.println("E:Invalid frame: line too long");
            return;
        }

        _buffer[length] = '\0';

        if (length == 1 && _buffer[0] == 'I') {
            sendInfoFrame();
            return;
        }

        if (length == 1 && _buffer[0] == 'T') {
            sendStateFrame();
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

    for (uint8_t index = 0; index < _motorCount; ++index) {
        StepperState state;
        _motors[index].stepper->readState(state);
        _serial.print(state.running);
        _serial.print(",");
        if (_motors[index].modulo)
            _serial.print((double)state.positionModulo * 360.0 / state.stepsPerRev);
        else
            _serial.print((double)state.position * 360.0 / state.stepsPerRev);
        _serial.print(",");
        _serial.print(state.speed * 360.0 / state.stepsPerRev);

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
    constexpr uint8_t maxMotorChannels = 4;
    if (_motorCount > maxMotorChannels) {
        _serial.println("E:Invalid protocol configuration");
        return;
    }

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

    ParsedMotorCommand commands[maxMotorChannels];
    char* token = strtok(_buffer, ",");
    for (uint8_t index = 0; index < _motorCount; ++index) {
        commands[index].mode = ROT_SHORTEST;
        if (!parseDouble(token, commands[index].target)) return;
        token = strtok(NULL, ",");
        if (!parseDouble(token, commands[index].speed)) return;

        if (_motors[index].modulo) {
            token = strtok(NULL, ",");
            if (!parseMode(token, commands[index].mode)) return;
        }

        token = strtok(NULL, ",");
        if (!parseDouble(token, commands[index].acceleration)) return;
        token = strtok(NULL, ",");
    }

    for (uint8_t index = 0; index < _motorCount; ++index) {
        _motors[index].stepper->applyCommandDegrees(
            commands[index].target,
            commands[index].speed,
            commands[index].acceleration,
            commands[index].mode,
            _motors[index].modulo);
    }

}

bool MovingSpeakerProtocol::parseDouble(char*& token, double& value)
{
    if (!token) {
        _serial.println("E:Invalid frame: invalid numeric field");
        return false;
    }

    errno = 0;
    char* end = nullptr;
    value = strtod(token, &end);
    while (end && isspace((unsigned char)*end)) ++end;

    if (end == token || *end != '\0' || errno == ERANGE || !isfinite(value)) {
        _serial.println("E:Invalid frame: invalid numeric field");
        return false;
    }
    return true;
}

bool MovingSpeakerProtocol::parseMode(char*& token, RotaryMode& mode)
{
    if (!token) {
        _serial.println("E:Invalid frame: invalid rotation mode");
        return false;
    }

    errno = 0;
    char* end = nullptr;
    long parsedMode = strtol(token, &end, 10);
    while (end && isspace((unsigned char)*end)) ++end;

    if (end == token || *end != '\0' || errno == ERANGE ||
        parsedMode < ROT_SHORTEST || parsedMode > ROT_CCW) {
        _serial.println("E:Invalid frame: invalid rotation mode");
        return false;
    }

    mode = (RotaryMode)parsedMode;
    return true;
}

void MovingSpeakerProtocol::sendStateFrame()
{
    _serial.print("S: ");
    for (uint8_t index = 0; index < _motorCount; ++index) {
        StepperState state;
        _motors[index].stepper->readState(state);
        _serial.print(state.running);
        _serial.print(",");
        _serial.print((double)state.targetPosition * 360.0 / state.stepsPerRev);
        _serial.print(",");
        _serial.print(state.maxSpeed * 360.0 / state.stepsPerRev);
        _serial.print(",");
        _serial.print(state.acceleration * 360.0 / state.stepsPerRev);

        if (index + 1 < _motorCount) _serial.print(",");
    }
    _serial.println();
}