#ifndef MOVING_SPEAKER_PROTOCOL_H
#define MOVING_SPEAKER_PROTOCOL_H

#include <Arduino.h>
#include "stepper_core.h"

struct MotorChannel
{
    StepperCore* stepper;
    bool modulo;
};

class MovingSpeakerProtocol
{
    public:
        MovingSpeakerProtocol(Stream& serial,
                              MotorChannel* motors,
                              uint8_t motorCount,
                              const char* infoTitle,
                              bool infoRequestEnabled = true,
                              bool statusPrefixSpace = false);

        void process();
        void sendInfoFrame();

    private:
        void sendPositionFrame();
        void processCommand(uint16_t length);
        bool parseDouble(char*& token, double& value);
        bool parseMode(char*& token, RotaryMode& mode);
        void sendStateFrame();

        Stream& _serial;
        MotorChannel* _motors;
        uint8_t _motorCount;
        const char* _infoTitle;
        bool _infoRequestEnabled;
        bool _statusPrefixSpace;
        unsigned long _lastPositionFrame = 0;
        char _buffer[200];
};

#endif