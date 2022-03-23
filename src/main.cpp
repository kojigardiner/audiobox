#include <Arduino.h>
#include <FastLED.h>
#include <ESP32Servo.h>
#include <WiFi.h>

/*** Defines ***/
#define PIN_LED_CONTROL     13              // LED strip control GPIO
#define MAX_BRIGHT          50              // sets max brightness for LEDs

#define GRID_H              16
#define GRID_W              16
#define NUM_LEDS            GRID_H * GRID_W
#define FRAMES_PER_SECOND   120

#define PIN_SERVO           18              // Servo pin
#define SERVO_MIN_US        700
#define SERVO_MAX_US        2400
#define SERVO_START_POS     130
#define SERVO_END_POS       0

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))  // helper

/*** Function Prototypes ***/
void nextPattern();
void rainbow();
void rainbowWithGlitter();
void addGlitter( fract8 chanceOfGlitter);
void confetti();
void sinelon();
void bpm();
void juggle();

/*** Globals ***/
Servo myservo;
int servo_pos = SERVO_START_POS;

CRGB leds[NUM_LEDS];                      // array that will hold LED values
typedef void (*SimplePatternList[])();    // List of patterns to cycle through.  Each is defined as a separate function below.
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
uint8_t gCurrentPatternNumber = 0;        // Index number of which pattern is current
uint8_t gHue = 0;                         // rotating "base color" used by many of the patterns

void setup() {
  Serial.begin(115200);         // debug serial terminal

  Serial.print("Initial safety delay\n");
  delay(3000);                  // power-up safety delay for LEDs

  // LED Setup
  Serial.print("Setting up LEDs\n");
  FastLED.addLeds<WS2812, PIN_LED_CONTROL, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHT);

  // Servo Setup
  Serial.print("Setting up servo\n");
  myservo.write(servo_pos);     // Set servo zero position prior to attaching in order to mitigate power-on glitch
  myservo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);

  // WiFi Setup
  Serial.print("Setting up wifi");
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname("ESP32 LED 16x16");
  WiFi.begin("***REMOVED***", "***REMOVED***");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("connected\n");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("RSSI: "); Serial.println(WiFi.RSSI());

  Serial.print("Setup complete\n\n");
  Serial.print("Servo control: f = forward, b = backward\n");
}

void loop() {
  // if (Serial.available() > 0) {
  //   char c = Serial.read();
  //   switch (c) {
  //     case 'f':
  //       servo_pos += 1;
  //       break;
  //     case 'b':
  //       servo_pos -= 1;
  //       break;
  //   }
  // }
  // if (servo_pos > SERVO_START_POS) servo_pos = SERVO_START_POS;
  // if (servo_pos < SERVO_END_POS) servo_pos = SERVO_END_POS;
  // myservo.write(servo_pos);
  
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
}
  
void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 5, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  uint8_t dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}