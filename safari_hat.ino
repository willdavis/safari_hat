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

#define WARNING_VOLTAGE 350 //cV

// Battery power states
bool low_power_state = false;

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

  Serial.begin(9600);
}

void loop()
{
  // reset brightness
  FastLED.setBrightness( BRIGHTNESS );

  // Read the current analog signal from the mic
  micSensorValue = analogRead(MIC_PIN);

  // Call the current pattern to update the LEDs
  patterns[current_pattern_index](micSensorValue);

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  EVERY_N_MILLISECONDS( 20 ) { current_hue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 10 ) { next_pattern(); }     // change patterns periodically
  EVERY_N_SECONDS( 1 ) { check_voltage(getBandgap()); }    // check if the battery voltage is below the limit

  // beep if a low power state is detected
  if (low_power_state == true) {
    EVERY_N_MILLISECONDS( 50 ){ digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN)); }
  }

}

void check_voltage(int voltage)
{
  // check for low power threshold
  if (voltage <= WARNING_VOLTAGE && low_power_state == false ){ low_power_state = true; }
  if (voltage > WARNING_VOLTAGE && low_power_state == true ){ low_power_state = false; digitalWrite(BUZZER_PIN, LOW); }
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

int getBandgap(void) // Returns actual value of Vcc (x 100)
{
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    // For mega boards
    const long InternalReferenceVoltage = 1080L;  // Adjust this value to your boards specific internal BG voltage x1000
    // REFS1 REFS0          --> 0 1, AVcc internal ref. -Selects AVcc reference
    // MUX4 MUX3 MUX2 MUX1 MUX0  --> 11110 1.1V (VBG)         -Selects channel 30, bandgap voltage, to measure
    ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<ADLAR)| (0<<MUX5) | (1<<MUX4) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);
#else
    // For 168/328 boards
    const long InternalReferenceVoltage = 1250L;  // Adjust this value to your boards specific internal BG voltage x1000
    // REFS1 REFS0          --> 0 1, AVcc internal ref. -Selects AVcc external reference
    // MUX3 MUX2 MUX1 MUX0  --> 1110 1.1V (VBG)         -Selects channel 14, bandgap voltage, to measure
    ADMUX = (0<<REFS1) | (1<<REFS0) | (0<<ADLAR) | (1<<MUX3) | (1<<MUX2) | (1<<MUX1) | (0<<MUX0);
#endif
    delay(2);  // Let mux settle a little to get a more stable A/D conversion
    // Start a conversion
    ADCSRA |= _BV( ADSC );
    // Wait for it to complete
    while( ( (ADCSRA & (1<<ADSC)) != 0 ) );
    // Scale the value
    int results = (((InternalReferenceVoltage * 1023L) / ADC) + 5L) / 10L; // calculates for straight line value
    return results;
}
