#include <avr/io.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdlib.h>
#include <avr/interrupt.h>

static void SetFastClock() // set clock to 8 MHz
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        CLKPR = _BV(CLKPCE);
        CLKPR = 0;
    }
}

static void StartPLL()
{
    PLLCSR = 0x86;
    while((PLLCSR & 1) == 0)
        ;
}

static void StartTimer()
{
    StartPLL();
    TCCR1 = 0x71;
    GTCCR = 0x00;
    OCR1A = 0x80;
    OCR1B = 0xFF;
    OCR1C = 0xFF;
    TIMSK = 0x20;
}

static void WriteToTimer(uint8_t value)
{
    while(1)
    {
        if(TIFR & 0x20)
        {
            TIFR = 0x20;
            OCR1A = value;
            return;
        }
    }
}

/*static uint8_t reverse(uint8_t v)
{
    uint8_t retval;
    __asm__ volatile(
        "bst %1, 0" "\n\t"
        "bld %0, 7" "\n\t"
        "bst %1, 1" "\n\t"
        "bld %0, 6" "\n\t"
        "bst %1, 2" "\n\t"
        "bld %0, 5" "\n\t"
        "bst %1, 3" "\n\t"
        "bld %0, 4" "\n\t"
        "bst %1, 4" "\n\t"
        "bld %0, 3" "\n\t"
        "bst %1, 5" "\n\t"
        "bld %0, 2" "\n\t"
        "bst %1, 6" "\n\t"
        "bld %0, 1" "\n\t"
        "bst %1, 7" "\n\t"
        "bld %0, 0"
    : "=&r"(retval)
            : "r"(v)
            : "cc"
        );
    return retval;
}*/

uint8_t stringValues[318];
uint8_t lastStringValue = 0x80;

uint16_t curStringSize = 318;

uint8_t curStringIndex = 0;
uint32_t elapsedTime = 0;

const int MidiNoteOrigin = 48;

uint16_t MidiNoteDelayTimeFracPart[24] =
{
    318,
    300,
    283,
    267,
    252,
    238,
    225,
    212,
    200,
    189,
    178,
    168,
    159,
    150,
    141,
    133,
    126,
    119,
    112,
    106,
    100,
    94,
    89,
    84,
};

struct Note
{
    uint8_t midiNote;
    uint8_t durationIn64thsOfASecond;
};

// C5   60
// C#5  61
// D5   62
// D#5  63
// E5   64
// F5   65
// F#5  66
// G5   67
// G#5  68
// A5   69
// A#5  70
// B5   71
// C6   72

static const Note notes[] =
{
    {60, 32},
    {60, 32},
    {67, 32},
    {67, 32},
    {69, 32},
    {69, 32},
    {67, 64},
    {65, 32},
    {65, 32},
    {64, 32},
    {64, 32},
    {62, 32},
    {62, 32},
    {60, 64},

    {67, 32},
    {67, 32},
    {65, 32},
    {65, 32},
    {64, 32},
    {64, 32},
    {62, 64},

    {67, 32},
    {67, 32},
    {65, 32},
    {65, 32},
    {64, 32},
    {64, 32},
    {62, 64},

    {60, 32},
    {60, 32},
    {67, 32},
    {67, 32},
    {69, 32},
    {69, 32},
    {67, 64},
    {65, 32},
    {65, 32},
    {64, 32},
    {64, 32},
    {62, 32},
    {62, 32},
    {60, 64},
};

unsigned curNote = 0;
uint16_t duration = 1;
bool isMuted = false;

void getCurrentNote()
{
    duration = notes[curNote].durationIn64thsOfASecond;
    isMuted = false;
    if(notes[curNote].midiNote == 0xFF)
        isMuted = true;
    else
    {
        curStringSize = MidiNoteDelayTimeFracPart[notes[curNote].midiNote - MidiNoteOrigin];
        for(uint16_t i = 0; i < curStringSize; i++)
            stringValues[i] = (uint8_t)rand();
        lastStringValue = stringValues[curStringSize - 1];
    }
}

bool isEndOfSong = false;

bool getNextNote()
{
    if(isEndOfSong)
    {
        isEndOfSong = false;
        curNote = 0;
        getCurrentNote();
        return true;
    }
    curNote++;
    if(curNote >= sizeof(notes) / sizeof(notes[0]))
    {
        isEndOfSong = true;
        isMuted = true;
        duration = 512;
        return false;
    }
    getCurrentNote();
    return true;
}

int main(void)
{
    DDRB = _BV(DDB0) | _BV(DDB1) | _BV(DDB2) | _BV(DDB3) | _BV(DDB4) | _BV(DDB5); // all outputs
    PORTB = _BV(PORTB5);
    SetFastClock();
    StartTimer();
    getCurrentNote();

    while(1)
    {
        if(!isMuted)
        {
            uint16_t v = lastStringValue;
            v += stringValues[curStringIndex];
            v += stringValues[curStringIndex >= curStringSize - 1 ? 0 : curStringIndex + 1];
            lastStringValue = stringValues[curStringIndex];
            stringValues[curStringIndex++] = v / 3;
            if(curStringIndex >= curStringSize)
            {
                curStringIndex = 0;
            }
        }
        for(uint8_t i = 0; i < 3; i++)
        {
            WriteToTimer(lastStringValue);
            elapsedTime++;
            if(elapsedTime >= 977)
            {
                elapsedTime = 0;
                duration--;
                if(duration <= 0)
                {
                    curStringIndex = 0;
                    if(!getNextNote())
                    {
                        break;
                    }
                    break;
                }
            }
        }
    }

    return 0;
}
