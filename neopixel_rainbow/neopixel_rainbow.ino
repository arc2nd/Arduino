// Low power NeoPixel goggles example. Makes a nice blinky display
// with just a few LEDs on at any time.
#include <Adafruit_NeoPixel.h>
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
#include <avr/power.h>
#endif

#define PIN 0

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(32, PIN);

uint8_t offset = 0; // Position of spinny eyes
uint32_t prevTime;
uint8_t bright = 85;

void setup() {
#ifdef __AVR_ATtiny85__ // Trinket, Gemma, etc.
  if(F_CPU == 16000000) clock_prescale_set(clock_div_1);
#endif
  pixels.begin();
  pixels.setBrightness(85); // 1/3 brightness
  prevTime = millis();
}

void loop() {
  uint8_t i;
  uint8_t j;
  uint32_t t;
  //                    whi  red  ora  yel  gre  blu  pur  bla
  uint8_t r_array[8] = {255, 255, 255, 255, 0,   0,   128, 0};
  uint8_t g_array[8] = {255, 0,   172, 255, 255, 0,   0,   0};
  uint8_t b_array[8] = {255, 0,   0,   0,   0,   255, 128, 0};
  for(i=0; i<16; i++)
  {
    for(j=0;j<6;j++)
    {
      uint8_t r = 0;
      uint8_t g = 0;
      uint8_t b = 0;
      if(((offset+i) % 16) < 5)
      {
        uint8_t k = ((offset+i) % 5);
        r = r_array[k];
        g = g_array[k];
        b = b_array[k];
      }
      pixels.setPixelColor(i, r, g, b);
      pixels.setPixelColor(31-(i), r, g, b);
    } 
  }
  pixels.show();
  delay(50);
  offset++;    

  t = millis();
  if((t - prevTime) > 10000)
  { 
    for(i=0; i<32; i++) pixels.setPixelColor(i, 0);
    prevTime = t;
  }
}
