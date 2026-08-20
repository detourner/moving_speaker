#include "timer.h"

uint16_t Counter::ticksPerUsec = 0;

bool Counter::Setup(ClockFrequency clock)
{
    switch (clock)
    {
        case C250kHz:
            cli();
            TCCR1A = 0;
            TCCR1B = (1 << CS11) | (1 << CS10);
            sei();
            break;
        case C62_500Hz:
            cli();
            TCCR1A = 0;
            TCCR1B = (1 << CS12);
            sei();
            break;
        case C15_625Hz:
            cli();
            TCCR1A = 0;
            TCCR1B = (1 << CS12) | (1 << CS10);
            sei();
            break;
        default:
            return false;
    }
    ticksPerUsec = clock;
    return true;
}

void Counter::Enable() {}
void Counter::Disable() {}
void Counter::Set(uint16_t ticks) { (void)ticks; }
void Counter::Increment(uint16_t ticks) { (void)ticks; }