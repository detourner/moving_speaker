#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <Arduino.h>

// Evailable clock frequencies for Timer1
enum ClockFrequency : uint8_t {
    //C16MHz = 0.166667,
    //C2MHz = 0.5,
    C500kHz = 2,
    C250kHz = 4,
    C125kHz = 8,
    C62_500Hz = 16,
    C15_625Hz = 64,
};

class CounterA;
class CounterB;

// Base Counter class
// Provides common interface and functionality for specific counters of Timer1
class Counter {
private:
    
    static uint16_t ticksPeruSec;        
    
public:

    static constexpr uint16_t MAX_VALUE = 65535;    
    
    // Setup Timer1 with specified clock frequency
    static bool Setup(ClockFrequency clock);

    // Get Timer1 ticks per second (depends on clock frequency)
    static uint16_t getTicksPeruSec() {
        return ticksPeruSec;
    }

    // Interface for specific counters
    virtual void Enable();    
    virtual void Disable();
    virtual void Set(uint16_t ticks);    
    virtual void Increment(uint16_t ticks);
};

// Specific CounterA class (used compare to Timer1 COMPA)
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

// Specific CounterA class (used compare to Timer1 COMPB)
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

#endif  // TIMER_H
