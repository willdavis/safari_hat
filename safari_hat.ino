#include <FastLED.h>
#include "fix_fft.h"

// IO pins
#define BUZZER_PIN  4
#define LED_PIN     5
#define LED_PIN_2   6
#define MIC_PIN     7 //A7

// LED constants
#define COLOR_ORDER       GRB
#define CHIPSET           WS2811
#define NUM_LEDS          40
#define MIN_BRIGHTNESS    50
#define MAX_BRIGHTNESS    200
#define FRAMES_PER_SECOND 120

// Battery constants
#define WARNING_VOLTAGE   350 //cV

// ----------------------
// Global variables
// ----------------------

// Battery power states
bool low_power_state = false;

// microphone variables
char im[128], data[128];
char data_avgs[14];
int value[3];
int i = 0, audioVal;

CRGB leds[NUM_LEDS];
CRGB leds_2[NUM_LEDS];

// List of patterns to cycle through.  Each is defined as a separate function.
typedef void (*PatternList[])();
PatternList patterns = { confetti, bpm, juggle };
uint8_t patterns_size = 3;
uint8_t current_pattern_index = 0;
uint8_t current_hue = 0;

// ----------------------
// LED patterns
// ----------------------

// random colored speckles that blink in and fade smoothly
void confetti()
{
  fadeToBlackBy( leds, NUM_LEDS, 10);
  fadeToBlackBy( leds_2, NUM_LEDS, 10);

  int pos = random16(NUM_LEDS);
  int pos_2 = random16(NUM_LEDS);

  leds[pos] += CHSV( current_hue + random8(64), 200, 255);
  leds_2[pos_2] += CHSV( current_hue + random8(64), 200, 255);
}

// colored stripes pulsing at a defined Beats-Per-Minute (BPM)
void bpm()
{
  uint8_t BeatsPerMinute = 120;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, current_hue+(i*2), beat-current_hue+(i*10));
    leds_2[i] = ColorFromPalette(palette, current_hue+(i*2), beat-current_hue+(i*10));
  }
}

// eight colored dots, weaving in and out of sync with each other
void juggle() {
  fadeToBlackBy( leds, NUM_LEDS, 20);
  fadeToBlackBy( leds_2, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    leds_2[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

// ----------------------
// Helper methods
// ----------------------

// check for low power
void check_voltage(int voltage)
{
  if (voltage <= WARNING_VOLTAGE && low_power_state == false ){ low_power_state = true; }
  if (voltage > WARNING_VOLTAGE && low_power_state == true ){ low_power_state = false; digitalWrite(BUZZER_PIN, LOW); }
}

// go to the next LED pattern number, and wrap around at the end
void next_pattern()
{
  current_pattern_index = (current_pattern_index + 1) % patterns_size;
}

// FFT magic
void get_audio_levels()
{
  for (i = 0; i < 128; i++)
  {
    audioVal = analogRead(MIC_PIN);
    data[i] = audioVal;
    im[i] = 0;
  };
  fix_fft(data, im, 7, 0);
  for (i = 0; i < 64; i++)
  {
    data[i] = sqrt(data[i] * data[i] + im[i] * im[i]);
  };
  for (i = 0; i < 14; i++)
  {
    data_avgs[i] = data[i * 4] + data[i * 4 + 1] + data[i * 4 + 2] + data[i * 4 + 3];
    data_avgs[i] = map(data_avgs[i], 0, 30, 0, 9);
  }
  value[0] = 0.75 * data_avgs[0] + data_avgs[1] + 0.75 * data_avgs[2];
  value[1] = data_avgs[3] + 2 * data_avgs[4] + 3 * data_avgs[5] + 3 * data_avgs[6] + 3 * data_avgs[7] + 2 * data_avgs[8] + data_avgs[9];
  value[2] = data_avgs[8] + 2 * data_avgs[9] + 3 * data_avgs[10] + 3 * data_avgs[11] + 2 * data_avgs[12] + data_avgs[13];

  String msg = "Audio levels: ";
  Serial.println(msg + "low: " + value[0] + " mid: " + value[1] + " high: " + value[2]);
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


// ----------------------
// Main program
// ----------------------

void setup() {
  delay(3000); // sanity delay
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.addLeds<CHIPSET, LED_PIN_2, COLOR_ORDER>(leds_2, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( MIN_BRIGHTNESS );

  pinMode(BUZZER_PIN, OUTPUT);

  Serial.begin(9600);
}

void loop()
{
  // Call the current pattern and update the LEDs
  patterns[current_pattern_index]();

  FastLED.show(); // display this frame
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  EVERY_N_MILLISECONDS( 20 ) { current_hue++; }         // slowly cycle the "base color" through the rainbow
  EVERY_N_MILLISECONDS( 50 ){ get_audio_levels(); }     // run the FFT library and update the audio bins
  EVERY_N_SECONDS( 1 ) { check_voltage(getBandgap()); } // check if the battery voltage is below the limit
  EVERY_N_SECONDS( 20 ) { next_pattern(); }             // change patterns periodically

  // beep if a low power state is detected
  if (low_power_state == true) {
    EVERY_N_MILLISECONDS( 100 ){ digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN)); }
  }
}
