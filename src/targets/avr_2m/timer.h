#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <Arduino.h>

enum ClockFrequency : uint8_t {
    C500kHz = 2,
    C250kHz = 4,
    C125kHz = 8,
    C62_500Hz = 16,
    C15_625Hz = 64,
};

class CounterA;
class CounterB;

class Counter {
private:
    static uint16_t ticksPerUsec;

public:
    static constexpr uint16_t MAX_VALUE = 65535;

    static bool Setup(ClockFrequency clock);

    static uint16_t getTicksPeruSec() {
        return ticksPerUsec;
    }

    virtual void Enable();
    virtual void Disable();
    virtual void Set(uint16_t ticks);
    virtual void Increment(uint16_t ticks);
};

class CounterA : public Counter {
public:
    void Enable() {
        TIMSK1 |= (1 << OCIE1A);
    }

    void Disable() {
        TIMSK1 &= ~(1 << OCIE1A);
    }

    void Set(uint16_t ticks) {
        OCR1A = TCNT1 + ticks;
    }

    void Increment(uint16_t ticks) {
        OCR1A += ticks;
    }
};

class CounterB : public Counter {
public:
    void Enable() {
        TIMSK1 |= (1 << OCIE1B);
    }

    void Disable() {
        TIMSK1 &= ~(1 << OCIE1B);
    }

    void Set(uint16_t ticks) {
        OCR1B = TCNT1 + ticks;
    }

    void Increment(uint16_t ticks) {
        OCR1B += ticks;
    }
};

#endif