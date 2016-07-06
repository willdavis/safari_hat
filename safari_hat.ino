#include <FastLED.h>

#define BUZZER_PIN  4
#define LED_PIN     5
#define LED_PIN_2   6
#define MIC_PIN     7 //A7
#define COLOR_ORDER GRB
#define CHIPSET     WS2811
#define NUM_LEDS    40

#define BRIGHTNESS  50
#define FRAMES_PER_SECOND 120

// microphone settings
int micSensorValue = 0;
int maxSensorValue = 450;

CRGB leds[NUM_LEDS];
CRGB leds_2[NUM_LEDS];

// List of patterns to cycle through.  Each is defined as a separate function.
typedef void (*PatternList[])(int);
PatternList patterns = { confetti };
int patterns_size = 1;

uint8_t current_pattern_index = 0;
uint8_t current_hue = 0;

void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_2, COLOR_ORDER>(leds_2, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );

  pinMode(BUZZER_PIN, OUTPUT);
}

void loop()
{
  // reset brightness
  FastLED.setBrightness( BRIGHTNESS );

  // read battery voltage
  if (readVcc() <= 3000) {
    EVERY_N_MILLISECONDS( 33 ){ digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN)); }
  }

  // Read the current analog signal from the mic
  micSensorValue = analogRead(MIC_PIN);

  // Call the current pattern to update the LEDs
  patterns[current_pattern_index](micSensorValue);

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  EVERY_N_MILLISECONDS( 20 ) { current_hue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 10 ) { next_pattern(); }     // change patterns periodically
}

// add one to the current pattern number, and wrap around at the end
void next_pattern()
{
  current_pattern_index = (current_pattern_index + 1) % patterns_size;
}

// random colored speckles that blink in and fade smoothly
void confetti(int mic) 
{
  if (mic >= maxSensorValue){
    FastLED.setBrightness( BRIGHTNESS + 150 );
  }

  fadeToBlackBy( leds, NUM_LEDS, 10);
  fadeToBlackBy( leds_2, NUM_LEDS, 10);

  int pos = random16(NUM_LEDS);
  int pos_2 = random16(NUM_LEDS);

  leds[pos] += CHSV( current_hue + random8(64), 200, 255);
  leds_2[pos_2] += CHSV( current_hue + random8(64), 200, 255);
}

// Shamelessly stolen from:
// http://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/
long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
