#include "timer.h"

uint16_t Counter::ticksPeruSec = 0;

bool Counter::Setup(ClockFrequency clock)
{
    switch (clock)
    {
        /*case C16MHz: 
            cli();
            TCCR1A = 0;
            TCCR1B = (1 << CS10);
            sei();
            break;
        case C2MHz:
            cli();
            TCCR1A = 0;
            TCCR1B = (1 << CS11);
            sei();
            break;
        */
        case C250kHz :
            cli();
            TCCR1A = 0;
            TCCR1B = (1 << CS11) | (1 << CS10);
            sei();
            break;
        case C62_500Hz :
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
        break;
    }
    ticksPeruSec = clock;
    return true;
}