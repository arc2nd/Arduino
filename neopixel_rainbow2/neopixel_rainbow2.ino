#include <Adafruit_NeoPixel.h>
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
#include <avr/power.h>
#endif

#define PIN 0

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(32, PIN);
 
// Pattern types supported:
enum  pattern { NONE, RAINBOW_CYCLE, THEATER_CHASE, COLOR_WIPE, SCANNER, FADE };
// Patern directions supported:
enum  direction { FORWARD, REVERSE };
 
// NeoPattern Class - derived from the Adafruit_NeoPixel class
class NeoPatterns : public Adafruit_NeoPixel
{
    public:
 
    // Member Variables:  
    pattern  ActivePattern;  // which pattern is running
    direction Direction;     // direction to run the pattern
    
    unsigned long Interval;   // milliseconds between updates
    unsigned long lastUpdate; // last update of position
    
    uint32_t Color1, Color2;  // What colors are in use
    uint16_t TotalSteps;  // total number of steps in the pattern
    uint16_t Index;  // current step within the pattern
    
    void (*OnComplete)();  // Callback on completion of pattern
};

// Constructor - calls base-class constructor to initialize strip
NeoPatterns(uint16_t pixels, uint8_t pin, uint8_t type, void (*callback)())
:Adafruit_NeoPixel(pixels, pin, type)
{
    OnComplete = callback;
}

// Update the pattern
void Update()
{
    if((millis() - lastUpdate) > Interval) // time to update
    {
        lastUpdate = millis();
        switch(ActivePattern)
        {
            case RAINBOW_CYCLE:
                RainbowCycleUpdate();
                break;
            case THEATER_CHASE:
                TheaterChaseUpdate();
                break;
            case COLOR_WIPE:
                ColorWipeUpdate();
                break;
            case SCANNER:
                ScannerUpdate();
                break;
            case FADE:
                FadeUpdate();
                break;
            default:
                break;
        }
    }
}

// Increment the Index and reset at the end
void Increment()
{
    if (Direction == FORWARD)
    {
       Index++;
       if (Index >= TotalSteps)
        {
            Index = 0;
            if (OnComplete != NULL)
            {
                OnComplete(); // call the comlpetion callback
            }
        }
    }
    else // Direction == REVERSE
    {
        --Index;
        if (Index <= 0)
        {
            Index = TotalSteps-1;
            if (OnComplete != NULL)
            {
                OnComplete(); // call the comlpetion callback
            }
        }
    }
}


// Initialize for a RainbowCycle
void RainbowCycle(uint8_t interval, direction dir = FORWARD)
{
    ActivePattern = RAINBOW_CYCLE;
    Interval = interval;
    TotalSteps = 255;
    Index = 0;
    Direction = dir;
}

// Update the Rainbow Cycle Pattern
void RainbowCycleUpdate()
{
    for(int i=0; i< numPixels(); i++)
    {
        setPixelColor(i, Wheel(((i * 256 / numPixels()) + Index) & 255));
    }
    show();
    Increment();
}

void setup() {
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
  if(F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  RainbowCycle(8000);
}

void loop() {
  // put your main code here, to run repeatedly:
  RainbowCycleUpdate();
}
