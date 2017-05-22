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
#define CRITICAL_VOLTAGE   340 //cV

// ----------------------
// Global variables
// ----------------------

// Battery power states
bool low_power_state = false;
bool critical_power_state = false;

// Microphone variables
char im[128], data[128];
char data_avgs[14];
int audio_bins[5];
int audioVal;

// LED variables for FastLED library
CRGB leds[NUM_LEDS];
CRGB leds_2[NUM_LEDS];

// Counters
uint8_t current_pattern = 0;
uint8_t current_hue = 0;
int i = 0;

// ----------------------
// LED patterns
// ----------------------

// Clear all LEDS (used in critical power state)
void blank_pattern()
{
  for( int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
    leds_2[i] = CRGB::Black;
  }
}

// random colored speckles that blink in and fade smoothly
// example pattern from: {FastLED example URL}
void confetti()
{
  fadeToBlackBy( leds, NUM_LEDS, 10);
  fadeToBlackBy( leds_2, NUM_LEDS, 10);

  int pos = random16(NUM_LEDS);
  int pos_2 = random16(NUM_LEDS);

  leds[pos] += CHSV( current_hue + random8(64), 200, 255);
  leds_2[pos_2] += CHSV( current_hue + random8(64), 200, 255);
}

// sound reactive pattern that displays 5 audio bins.
// low, low-mid, mid, mid-high, and high frequencies
// louder frequency ranges will display on the highest LED row
// quieter frequency ranges will display on the lowest LED row, or not at all.
// segment the LEDs into 8 groups with 5 LEDs per group.
// update each group with the same audio data, and render each LED accordingly
void spectrum()
{
  for(int i = 0; i < 8; i++){
    // low
    if (audio_bins[0] > 64) { leds[i*5] = CHSV(current_hue, 200, 255); }
    else { leds[i*5] = CRGB::Black; }
    if (audio_bins[0] > 80) { leds_2[i*5] = CHSV(current_hue + 128, 200, 255); }
    else { leds_2[i*5] = CRGB::Black; }

    // low-mid
    if (audio_bins[1] > 64) { leds[i*5+1] = CHSV(current_hue, 200, 255); }
    else { leds[i*5+1] = CRGB::Black; }
    if (audio_bins[1] > 80) { leds_2[i*5+1] = CHSV(current_hue + 128, 200, 255); }
    else { leds_2[i*5+1] = CRGB::Black; }

    // mid
    if (audio_bins[2] > 64) { leds[i*5+2] = CHSV(current_hue, 200, 255); }
    else { leds[i*5+2] = CRGB::Black; }
    if (audio_bins[2] > 80) { leds_2[i*5+2] = CHSV(current_hue + 128, 200, 255); }
    else { leds_2[i*5+2] = CRGB::Black; }

    // mid-high
    if (audio_bins[3] > 64) { leds[i*5+3] = CHSV(current_hue, 200, 255); }
    else { leds[i*5+3] = CRGB::Black; }
    if (audio_bins[3] > 80) { leds_2[i*5+3] = CHSV(current_hue + 128, 200, 255); }
    else { leds_2[i*5+3] = CRGB::Black; }

    // high
    if (audio_bins[4] > 64) { leds[i*5+4] = CHSV(current_hue, 200, 255); }
    else { leds[i*5+4] = CRGB::Black; }
    if(audio_bins[4] > 80) { leds_2[i*5+4] = CHSV(current_hue + 128, 200, 255); }
    else { leds_2[i*5+4] = CRGB::Black; }
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

// List of patterns to cycle through.  Each is defined as a separate function.
typedef void (*PatternList[])();
PatternList patterns = { spectrum, spectrum, confetti, spectrum, juggle, spectrum };
uint8_t patterns_size = 6;

// ----------------------
// Helper methods
// ----------------------

// check for low power
void check_voltage(int voltage)
{
  if (voltage <= WARNING_VOLTAGE && low_power_state == false ){ low_power_state = true; }
  if (voltage > WARNING_VOLTAGE && low_power_state == true ){ low_power_state = false; digitalWrite(BUZZER_PIN, LOW); }
  if (voltage <= CRITICAL_VOLTAGE ){ critical_power_state = true; }
}

// go to the next LED pattern number, and wrap around at the end
void next_pattern()
{
  current_pattern = (current_pattern + 1) % patterns_size;
}

// FFT magic
void get_audio_levels()
{
  // sample audio data 128 times
  for (i = 0; i < 128; i++)
  {
    audioVal = analogRead(MIC_PIN);
    data[i] = audioVal;
    im[i] = 0;
  };

  // cast spell of FFT magic!
  fix_fft(data, im, 7, 0);

  // process sound data doing some wizardry from: {URL}
  // DO NOT CHANGE THIS UNLESS YOU KNOW WHAT YOUR DOING!
  for (i = 0; i < 64; i++)
  {
    data[i] = sqrt(data[i] * data[i] + im[i] * im[i]);
  };
  for (i = 0; i < 14; i++)
  {
    data_avgs[i] = data[i * 4] + data[i * 4 + 1] + data[i * 4 + 2] + data[i * 4 + 3];
    data_avgs[i] = map(data_avgs[i], 0, 30, 0, 9);
  }

  // set the value for each audio bin
  audio_bins[0] = data_avgs[0] + data_avgs[1] + data_avgs[3];
  audio_bins[1] = data_avgs[4] + data_avgs[5] + data_avgs[2];
  audio_bins[2] = data_avgs[6] + data_avgs[7] + data_avgs[8] + data_avgs[9];
  audio_bins[3] = data_avgs[8] + data_avgs[9] + data_avgs[10];
  audio_bins[4] = data_avgs[11] + data_avgs[12] + data_avgs[13];

  // sanitize the audio data
  //audio_bins[0] = map(audio_bins[0], 0, 50, 0, 128);
  //audio_bins[1] = map(audio_bins[1], 0, 50, 0, 128);
  //audio_bins[2] = map(audio_bins[2], 0, 50, 0, 128);
  //audio_bins[3] = map(audio_bins[3], 0, 30, 0, 128);
  //audio_bins[4] = map(audio_bins[4], 0, 30, 0, 128);

  Serial.println("low: " + String(audio_bins[0]) + " low-mid: " + String(audio_bins[1]) + " mid: " + String(audio_bins[2]) + " mid-high: " + String(audio_bins[3]) + " high: " + String(audio_bins[4]));
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
  // check if the battery voltage is critically low, if not,
  // call the current pattern and update the LEDs.
  if (critical_power_state == true) {
    blank_pattern();
  } else {
    patterns[current_pattern]();
  }

  // display this frame and wait for the next one.
  FastLED.show();
  FastLED.delay(1000 / FRAMES_PER_SECOND);

  // background timers
  EVERY_N_MILLISECONDS( 20 ) { current_hue++; }         // slowly cycle the "base color" through the rainbow
  EVERY_N_MILLISECONDS( 50 ){ get_audio_levels(); }     // run the FFT library and update the audio bins
  EVERY_N_SECONDS( 1 ) { check_voltage(getBandgap()); } // check if the battery voltage is low
  EVERY_N_SECONDS( 30 ) { next_pattern(); }             // change patterns periodically

  // beep if a low power state is detected
  if (low_power_state == true) {
    EVERY_N_MILLISECONDS( 100 ){ digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN)); }
  }
}
