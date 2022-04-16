/*** Includes ***/
#include <Arduino.h>
#include <FastLED.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>

#include "Constants.h"
#include "NetworkConstants.h"
#include "SpotifyData.h"
#include "Button.h"
#include "Timer.h"
#include "AudioProcessor.h"
#include "MeanCut.h"

/*** Function Prototypes ***/

// LED Functions
void nextPattern();
void rainbow();
void rainbowWithGlitter();
void addGlitter( fract8 chanceOfGlitter);
void confetti();
void sinelon();
void bpm(uint8_t bpm);
void juggle();
void lissajous();
void wu_pixel(uint16_t x, uint16_t y, CRGB* col);

// Spotify Functinos
void get_spotify_token();
void get_spotify_player(SpotifyPlayerData_t *sp_data);
void update_spotify_data(DynamicJsonDocument *json, SpotifyPlayerData *sp_data);
void get_and_update_spotify_features(SpotifyPlayerData *sp_data);
void print_spotify_data(SpotifyPlayerData *sp_data);

// Display Functions
bool display_image(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);   // callback function for JPG decoder
bool copy_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);   // callback function for JPG decoder
int grid_to_idx(int x, int y, bool start_top_left);
void get_spotify_art(SpotifyPlayerData_t *sp_data);
void display_led_demo(SpotifyPlayerData_t *sp_data);
void display_full_art(uint8_t idx);
void decode_art(SpotifyPlayerData_t *sp_data);

// Servo Functions
void move_servo();

// Audio Functions
void run_audio(AudioProcessor *ap);
void set_leds_sym_snake_grid(AudioProcessor *ap);
void set_leds_scrolling(AudioProcessor *ap);
void set_leds_bars(AudioProcessor *ap);
void set_leds_noise(AudioProcessor *ap);
void (*audio_display_func[]) (AudioProcessor *) = {set_leds_sym_snake_grid, set_leds_noise};//, set_leds_bars}; //, set_leds_scrolling};
//void (*audio_display_func[]) (AudioProcessor *) = {set_leds_sym_snake_grid, set_leds_scrolling};


/*** Globals ***/

// Task & Queue Handles
TaskHandle_t task_spotify;
TaskHandle_t task_display_art;
TaskHandle_t task_buttons;
TaskHandle_t task_display_audio;
QueueHandle_t q_sp_data;
QueueHandle_t q_change_mode;
SemaphoreHandle_t mutex_display;
enum Modes { MODE_AUDIO, MODE_ART };
uint8_t curr_mode = MODE_ART;

void task_spotify_code(void *parameter);
void task_display_art_code(void *parameter);
void task_buttons_code(void *parameter);
void task_display_audio_code(void *parameter);

// LED Globals
CRGB leds[NUM_LEDS];                      // array that will hold LED values
typedef void (*SimplePatternList[])();    // List of patterns to cycle through.  Each is defined as a separate function below.
//SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
//SimplePatternList gPatterns = { lissajous, rainbow, confetti, bpm };
SimplePatternList gPatterns = { };
uint8_t gCurrentPatternNumber = 0;        // Index number of which pattern is current
uint8_t gHue = 0;                         // rotating "base color" used by many of the patterns
CRGBPalette16 curr_palette;
CRGBPalette16 target_palette;
double color_index = 0;
TBlendType curr_blending = LINEARBLEND;
CRGB colors_grid_wide[GRID_W * SCROLL_AVG_FACTOR][GRID_H] = {CRGB::Black};
int audio_display_idx = 0;
uint16_t full_art[64][64] = { 0 };

// Globals for palette art
uint16_t palette_art[16][16] = { 0 };
const uint8_t MEAN_CUT_DEPTH = 4;
const uint8_t PALETTE_ENTRIES = 1 << MEAN_CUT_DEPTH;
uint8_t palette_results[PALETTE_ENTRIES][3] = { 0 };
CRGB palette_crgb[PALETTE_ENTRIES];

// The 16 bit version of our coordinates
static uint16_t x;
static uint16_t y;
static uint16_t z;

uint16_t target_x;
uint16_t target_y;

// We're using the x/y dimensions to map to the x/y pixels on the matrix.  We'll
// use the z-axis for "time".  speed determines how fast time moves forward.  Try
// 1 for a very slow moving effect, or 60 for something that ends up looking like
// water.
uint16_t speed = 1; // speed is set dynamically once we've started up

// Scale determines how far apart the pixels in our noise matrix are.  Try
// changing these values around to see how it affects the motion of the display.  The
// higher the value of scale, the more "zoomed out" the noise iwll be.  A value
// of 1 will be so zoomed in, you'll mostly see solid colors.
uint16_t scale = 15; // scale is set dynamically once we've started up
uint16_t target_scale = 15; // scale is set dynamically once we've started up

// This is the array that we keep our computed noise values in
uint8_t noise[GRID_H][GRID_W];

/*** Color Palettes ***/
// Gradient palette "Sunset_Real_gp", originally from
// http://soliton.vm.bytemark.co.uk/pub/cpt-city/nd/atmospheric/tn/Sunset_Real.png.index.html
// converted for FastLED with gammas (2.6, 2.2, 2.5)
// Size: 28 bytes of program space.
DEFINE_GRADIENT_PALETTE( Sunset_Real_gp ) {
  0, 120,  0,  0,
  22, 179, 22,  0,
  51, 255, 104,  0,
  85, 167, 22, 18,
  135, 100,  0, 103,
  198,  16,  0, 130,
  255,   0,  0, 160
};

// Servo Globals
Servo myservo;
int servo_pos = SERVO_START_POS;

// Spotify Globals
bool token_expired = true;
String token = "";

// Button Globals
Button button_up = Button(PIN_BUTTON_UP);
Button button_down = Button(PIN_BUTTON_DOWN);
Button button_mode = Button(PIN_BUTTON_MODE);

void setup() {
  // Turn on status LED to indicate program has loaded
  pinMode(PIN_LED_STATUS, OUTPUT);
  digitalWrite(PIN_LED_STATUS, HIGH);

  Serial.begin(115200);         // debug serial terminal

  Serial.print("Initial safety delay\n");
  delay(3000);                  // power-up safety delay for LEDs

  // LED Setup
  Serial.print("Setting up LEDs\n");
  FastLED.addLeds<WS2812, PIN_LED_CONTROL, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(MAX_BRIGHT);
  FastLED.clear();
  FastLED.show();
  curr_palette = Sunset_Real_gp;

  // Servo Setup
  Serial.print("Setting up servo\n");
  myservo.write(servo_pos);     // Set servo zero position prior to attaching in order to mitigate power-on glitch
  myservo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);
  servo_pos = myservo.read();   // Determine where the servo is

  // WiFi Setup
  Serial.print("Setting up wifi");
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("connected\n");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("RSSI: "); Serial.println(WiFi.RSSI());

  // JPG decoder setup
  Serial.print("Setting up JPG decoder\n");
  TJpgDec.setJpgScale(1);           // Assuming 64x64 image, will rescale to 16x16
  //TJpgDec.setCallback(display_image);  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(copy_jpg_data);  // The decoder must be given the exact name of the rendering function above

  // Filessystem setup
  Serial.print("Setting up filesystem\n");
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  // Initialize our coordinates to some random values
  x = random16();
  y = random16();
  z = random16();

  Serial.print("Setup complete\n\n");

  // Task setup
  mutex_display = xSemaphoreCreateMutex();

  xTaskCreate(
    task_spotify_code,      // Function to implement the task
    "task_spotify",         // Name of the task
    10000,                  // Stack size in words
    NULL,                   // Task input parameter
    1,                      // Priority of the task (don't use 0!)
    &task_spotify           // Task handle
  );

  xTaskCreate(
    task_buttons_code,        // Function to implement the task
    "task_buttons",           // Name of the task
    10000,                  // Stack size in words
    NULL,                   // Task input parameter
    1,                      // Priority of the task (don't use 0!)
    &task_buttons             // Task handle
  );

  xTaskCreate(
    task_display_audio_code,        // Function to implement the task
    "task_display_audio",           // Name of the task
    30000,                  // Stack size in words
    NULL,                   // Task input parameter
    1,                      // Priority of the task (don't use 0!)
    &task_display_audio             // Task handle
  );

  xTaskCreate(
    task_display_art_code,      // Function to implement the task
    "task_display_art",         // Name of the task
    10000,                  // Stack size in words
    NULL,                   // Task input parameter
    1,                      // Priority of the task (don't use 0!)
    &task_display_art           // Task handle
  );

  q_sp_data = xQueueCreate(1, sizeof(SpotifyPlayerData_t));
  q_change_mode = xQueueCreate(1, sizeof(uint8_t));

  disableCore0WDT();

  // // Splash screen
  // File file = SPIFFS.open("/rainbows.jpg");
  // if(!file){
  //   Serial.println("Failed to open file for reading");
  //   return;
  // }
  // file.close();
  
  // // Write image
  // // Get the width and height in pixels of the jpeg if you wish
  // uint16_t w = 0, h = 0;
  // TJpgDec.getFsJpgSize(&w, &h, "/rainbows.jpg"); // Note name preceded with "/"
  // Serial.print("Width = "); Serial.print(w); Serial.print(", height = "); Serial.println(h);

  // // Draw the image, top left at 0,0
  // TJpgDec.drawFsJpg(0, 0, "/rainbows.jpg");
}

void loop() {
  vTaskDelete(NULL);
} 

void task_display_audio_code(void *parameter) {
  Serial.print("task_display_audio_code running on core ");
  Serial.println(xPortGetCoreID());

  AudioProcessor ap = AudioProcessor(false, false, true, true);
  uint8_t change_mode = 0;  

  for (;;) {
    // xQueueReceive(q_change_mode, &change_mode, 0);
    // if (change_mode) {
    //   audio_display_idx = (audio_display_idx + 1) % (sizeof(audio_display_func) / (sizeof(audio_display_func[0])));
    //   Serial.println("Mode change, idx =  " + String(audio_display_idx));

    //   change_mode = 0;
    //   xQueueReset(q_change_mode);
    // }
    if (curr_mode == MODE_AUDIO) {
      xSemaphoreTake(mutex_display, portMAX_DELAY);
      move_servo();
      // FastLED.setBrightness(MAX_BRIGHT);
      // EVERY_N_MILLIS(int(1000.0 / FPS)) {
      run_audio(&ap);
      nblendPaletteTowardPalette(curr_palette, target_palette, PALETTE_CHANGE_RATE);
      FastLED.show();
      // }
      xSemaphoreGive(mutex_display);
    }
    //vTaskDelay((1000 / FPS) / portTICK_RATE_MS);
    vTaskDelay(1); // TODO: check this
  }

  vTaskDelete(NULL);
}

void run_audio(AudioProcessor *ap) {  

  ap->get_audio_samples_gapless();
  ap->update_volume();
  ap->run_fft();
  audio_display_func[audio_display_idx](ap);
}

void task_spotify_code(void *parameter) {
  Serial.print("task_spotify_code running on core ");
  Serial.println(xPortGetCoreID());

  SpotifyPlayerData_t sp_data;

  for (;;) {
    if ((WiFi.status() == WL_CONNECTED)) {
      if (token_expired) {
        get_spotify_token();
      }
      get_spotify_player(&sp_data);
      if (sp_data.track_changed) {
        get_and_update_spotify_features(&sp_data);
        print_spotify_data(&sp_data);

        speed = int(round(2 * sp_data.tempo / FPS));
        if (sp_data.energy >= 0.75) speed *= 2;
        if (speed < 1) speed = 1;
        Serial.println("\tSpeed: " + String(speed));
      }
      xQueueSend(q_sp_data, &sp_data, 0); // set timeout to zero so loop will continue until display is updated
    } else {
      Serial.println("Error: WiFi not connected! status = " + String(WiFi.status()));
    }
    vTaskDelay(1000 / portTICK_RATE_MS);  // Run once a second
  }
  vTaskDelete(NULL);
}

void task_display_art_code(void *parameter) {
  Serial.print("task_display_art_code running on core ");
  Serial.println(xPortGetCoreID());

  uint8_t change_mode = 0;  
  SpotifyPlayerData_t sp_data;
  String curr_track_id = "";
  uint8_t counter = 0;
  BaseType_t q_return;
  double percent_complete = 0.0;

  for (;;) {
    // xQueueReceive(q_change_mode, &change_mode, 0);
    // if (change_mode) {
    //   curr_mode = (curr_mode + 1) % 2;
    //   change_mode = 0;
      
    //   FastLED.clear();

    //   curr_track_id = ""; // reset to force art display on mode change
    //   xQueueReset(q_sp_data); // reset the queue since we haven't been consuming it
    // }
    // if (curr_mode % 2) {
    //   xQueueReceive(q_sp_data, &sp_data, 0);  // only use this for the bpm demo
    //   display_led_demo(&sp_data);
    //   servo_pos = 110;
    //   move_servo();
    // } else {
    //   servo_pos = 30;
    //   move_servo();
    if (curr_mode == MODE_ART) {  // only run when mode is active
      xSemaphoreTake(mutex_display, portMAX_DELAY); // wait until other threads are done acting on the display
      q_return = xQueueReceive(q_sp_data, &sp_data, 0);

      // if we actually have new data from the queue
      // if (q_return == pdTRUE && sp_data.track_id != curr_track_id && sp_data.is_art_loaded) {
      //   Serial.println("Track changed");
      //   curr_track_id = sp_data.track_id;
      //   Serial.println("Decoding art, " + String(sp_data.art_num_bytes) + " bytes");
      //   TJpgDec.drawJpg(0, 0, sp_data.art_data, sp_data.art_num_bytes); // decode and copy jpg data into full_art

      //   mean_cut((uint16_t *)palette_art, 16*16, MEAN_CUT_DEPTH, (uint8_t *)palette_results);
      //   Serial.println("Finished mean cut, printing returned results");
      //   for (int i=0; i < PALETTE_ENTRIES; i++) {
      //     Serial.println(String(palette_results[i][0]) + "," + String(palette_results[i][1]) + "," + String(palette_results[i][2]));
      //     uint8_t r8 = round(pow(float(palette_results[i][0]) / 255, LED_GAMMA / JPG_GAMMA) * 255);
      //     uint8_t g8 = round(pow(float(palette_results[i][1]) / 255, LED_GAMMA / JPG_GAMMA) * 255);
      //     uint8_t b8 = round(pow(float(palette_results[i][2]) / 255, LED_GAMMA / JPG_GAMMA) * 255);
      //     palette_crgb[i] = CRGB(r8, g8, b8);
      //   }
      //   curr_palette = CRGBPalette16(palette_crgb[0], palette_crgb[1], palette_crgb[2], palette_crgb[3],
      //                                palette_crgb[4], palette_crgb[5], palette_crgb[6], palette_crgb[7],
      //                                palette_crgb[8], palette_crgb[9], palette_crgb[10], palette_crgb[11],
      //                                palette_crgb[12], palette_crgb[13], palette_crgb[14], palette_crgb[15]);
      // }
      if (q_return == pdTRUE && sp_data.is_active == true) {
          percent_complete = double(sp_data.progress_ms) / sp_data.duration_ms * 100;
          //Serial.println("Playing..." + String(percent_complete) + "%");
      }
      // Display art and current elapsed regardless of if we have new data from the queue
      if (sp_data.is_art_loaded && sp_data.is_active == true) {
        display_full_art(0);
        counter = (counter + 1) % 4;

        // Update the LED indicator at the bottom of the array
        int grid_pos = int(round(percent_complete / 100 * GRID_W));
        
        for (int i=0; i<GRID_W; i++) {
          if (i < grid_pos) {
            leds[i] = CRGB::Red;
          }
        }
        // for (int i=0; i<PALETTE_ENTRIES; i++) {
        //   leds[i] = palette_crgb[i];
        // }
      } else {
        lissajous();
      }
      //FastLED.setBrightness(int(MAX_BRIGHT * 0.75));
      FastLED.show();
      move_servo();
      xSemaphoreGive(mutex_display);  // release the display mutex
    }
    // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(10 / portTICK_RATE_MS);
  }
  vTaskDelete(NULL);
}

void task_buttons_code(void *parameter) {
  Serial.print("task_buttons_code running on core ");
  Serial.println(xPortGetCoreID());

  uint8_t change_mode = 1;
  for (;;) {
    if (button_up.read_button() == LOW) {
      servo_pos += 1;
      move_servo();
    }
    if (button_down.read_button() == LOW) {
      servo_pos -= 1;
      move_servo();
    }
    if (button_mode.read_button() == LOW) {
      Serial.println("Mode change");
      //xQueueSend(q_change_mode, &change_mode, 0);
      FastLED.clear(true); // clear before mode change in case task switches

      curr_mode = (curr_mode + 1) % 2;

      if (curr_mode == MODE_AUDIO) {
        audio_display_idx = (audio_display_idx + 1) % 2;
        servo_pos = 115;
      }
      if (curr_mode == MODE_ART) servo_pos = 70;
      Serial.println("curr_mode = " + String(curr_mode));

    }
    //Serial.println(uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(SERVO_BUTTON_HOLD_DELAY_MS / portTICK_RATE_MS); // added to avoid starving other tasks
  }
  vTaskDelete(NULL);
}

// Fill the x/y array of 8-bit noise values using the inoise8 function.
void fillnoise8() {
  // If we're runing at a low "speed", some 8-bit artifacts become visible
  // from frame-to-frame.  In order to reduce this, we can do some fast data-smoothing.
  // The amount of data smoothing we're doing depends on "speed".
  uint8_t dataSmoothing = 0;

  scale = int(round((target_scale * 0.3) + (scale * 0.7))); // slowly move toward the new target_scale
  x = int(round((target_x * 0.3) + (x * 0.7))); // slowly move toward the new target_x
  y = int(round((target_y * 0.3) + (y * 0.7))); // slowly move toward the new target_y

  if( speed < 50) {
    dataSmoothing = 200 - (speed * 4);
  }
  
  for(int i = 0; i < GRID_W; i++) {
    int ioffset = scale * (i - int(GRID_W / 2));    // center the scale shift
    for(int j = 0; j < GRID_H; j++) {
      int joffset = scale * (j - int(GRID_H / 2));  // center the scale shift
      
      uint8_t data = inoise8(x + ioffset,y + joffset,z);

      // The range of the inoise8 function is roughly 16-238.
      // These two operations expand those values out to roughly 0..255
      // You can comment them out if you want the raw noise data.
      data = qsub8(data,16);
      data = qadd8(data,scale8(data,39));

      if( dataSmoothing ) {
        uint8_t olddata = noise[i][j];
        uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
        data = newdata;
      }
      
      noise[i][j] = data;
    }
  }
  
  z += speed;
  
  // apply slow drift to X and Y, just for visual variation.
  // x += speed / 8;
  // y -= speed / 16;
}

void mapNoiseToLEDsUsingPalette()
{
  static uint8_t ihue=0;
  
  for(int i = 0; i < GRID_W; i++) {
    for(int j = 0; j < GRID_H; j++) {
      // We use the value at the (i,j) coordinate in the noise
      // array for our brightness, and the flipped value from (j,i)
      // for our pixel's index into the color palette.

      uint8_t index = noise[j][i];
      uint8_t bri =   noise[i][j];

      // // if this palette is a 'loop', add a slowly-changing base value
      // if( colorLoop) { 
      //   index += ihue;
      // }

      // brighten up, as the color palette itself often contains the 
      // light/dark dynamic range desired
      if( bri > 127 ) {
        bri = 255;
      } else {
        bri = dim8_raw( bri * 2);
      }

      CRGB color = ColorFromPalette( curr_palette, index, bri);
      leds[grid_to_idx(i,j, false)] = color;
    }
  }
  
  ihue+=1;
}

void set_leds_noise(AudioProcessor *ap) {
  if (ap->beat_detected) {
    uint8_t change_type = random8(6);   // select a random behavior

    switch (change_type) {              // either change the scale or scan in XY
      case 0:
        target_scale = constrain(scale + random8(30, 40), 10, 50);
        break;
      case 1:
        target_scale = constrain(scale - random8(30, 40), 10, 50);
        break;
      case 2:
        target_x = constrain(x + 16 * (random8(3, 7)) * scale / 10, 0, 65535);  // scale by "scale" so that the moves look similar
        break;
      case 3:
        target_y = constrain(y + 16 * (random8(3, 7)) * scale / 10, 0, 65535);
        break;
      case 4:
        target_x = constrain(x - 16 * (random8(3, 7)) * scale / 10, 0, 65535);
        break;
      case 5:
        target_y = constrain(y - 16 * (random8(3, 7)) * scale / 10, 0, 65535);
        break;        
    }
  }

    // generate noise data
  fillnoise8();
  
  // convert the noise data to colors in the LED array
  // using the current palette
  mapNoiseToLEDsUsingPalette();
}

void set_leds_bars(AudioProcessor *ap) {
  ap->calc_intensity_simple(GRID_W);

  for (int bar_x = 0; bar_x < GRID_W; bar_x++) {
    int max_y = int(round((float(ap->intensity[bar_x]) / 256) * GRID_H)); // scale the intensity by the grid height

    for (int bar_y = 0; bar_y < GRID_H; bar_y++) {
      if (bar_y <= max_y) {
        leds[grid_to_idx(bar_x, bar_y, false)] = CRGB::Red;   // turn leds red up to the max value
      } else {
        if (ap->beat_detected) {
          leds[grid_to_idx(bar_x, bar_y, false)] = CRGB::DimGray;
        } else {
          leds[grid_to_idx(bar_x, bar_y, false)] = CRGB::Black;
        }
      }
    }
  }
}

void set_leds_sym_snake_grid(AudioProcessor *ap) {
  ap->calc_intensity(NUM_LEDS / 2); // for a symmetric pattern, we only calculate intensities for half the LEDs

  // Left half of array
  /*
   * 0: 3 -> 0     (width * i + width/2 - 1)::-1
   * 1: 15 -> 12   (width * (i + 1) - 1)::-1
   * 2: 19 -> 16   (width * i + width/2 - 1)::-1
   * 3: 31 -> 28   (width * (i + 1) - 1)::-1
   * 4: 35 -> 32   (width * i + width/2 - 1)::-1
   * 5: 47 -> 44   (width * (i + 1) - 1)::-1
   * 6: 51 -> 48   (width * i + width/2 - 1)::-1
   * 7: 63 -> 60   (width * (i + 1) - 1)::-1
   */

  int curr_index = 0;
  color_index = 0;
  for (int i = 0; i < GRID_H; i++) {
    if (i % 2 == 0) {
      for (int j = 0; j < GRID_W/2; j++) {
        leds[GRID_W * i + GRID_W/2 - 1 - j] = ColorFromPalette(curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(ap->intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), curr_blending);
        color_index += 255.0 / (NUM_LEDS/2);
        curr_index += 1;
      }
    } else {
      for (int j = 0; j < GRID_W/2; j++) {
        leds[GRID_W * (i + 1) - 1 - j] = ColorFromPalette(curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(ap->intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), curr_blending);
        color_index += 255.0 / (NUM_LEDS/2);
        curr_index += 1;
      }      
    }
  }

  // Right half of array
  /*
   * 0: 4 -> 7     (width * i + width/2)::+1
   * 1: 8 -> 11    (width * i)::+1
   * 2: 20 -> 23   (width * i + width/2)::+1
   * 3: 24 -> 27   (width * i)::+1
   * 4: 36 -> 39   (width * i + width/2)::+1
   * 5: 40 -> 43   (width * i)::+1
   * 6: 52 -> 55   (width * i + width/2)::+1
   * 7: 56 -> 59   (width * i)::+1
   */
  curr_index = 0;
  color_index = 0;
  CRGB curr_color;
  for (int i = 0; i < GRID_H; i++) {
    if (i % 2 == 0) {
      for (int j = 0; j < GRID_W/2; j++) {
        leds[GRID_W * i + GRID_W/2 + j] = ColorFromPalette(curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(ap->intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), curr_blending);
        // TODO: Pull color from palette first at intensity[curr_index]; _then_ apply gamma to adjust. 
        // TODO: Use same gammas for RGB that are used on RaspPi
        color_index += 255.0 / (NUM_LEDS/2);
        curr_index += 1;
      }
    } else {
      for (int j = 0; j < GRID_W/2; j++) {
        leds[GRID_W * i + j] = ColorFromPalette(curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(ap->intensity[curr_index] * 255.0 / double(BRIGHT_LEVELS))]), curr_blending);
        color_index += 255.0 / (NUM_LEDS/2);
        curr_index += 1;
      }      
    }
  }
}

void set_leds_scrolling(AudioProcessor *ap) {
  ap->calc_intensity(GRID_H);

  // Generate an extra wide representation of the colors
  int effective_w = GRID_W * SCROLL_AVG_FACTOR;
  int avg_r, avg_g, avg_b;
  
  color_index = 0;
  
  // For leftmost columns, grab data from the next column over
  // For the last column, actually take the audio data
  for (int x = 0; x < effective_w; x++) {  
    for (int y = 0; y < GRID_H; y++) {
      if (x == effective_w - 1) {
        colors_grid_wide[x][y] = ColorFromPalette(curr_palette, int(color_index), pgm_read_byte(&GAMMA8[int(ap->intensity[y] * 255.0 / double(BRIGHT_LEVELS))]), curr_blending);
        color_index += 255.0 / (GRID_H);
      } else {
        colors_grid_wide[x][y] = colors_grid_wide[x+1][y]; 
        //colors_grid_wide[x][y].fadeToBlackBy(32); // note this is an exponential decay by XX/255. 32 makes this look like a standard spectrogram
      }
    }
  }
  for (int x = 0; x < GRID_W; x++) {
    for (int y = 0; y < GRID_H; y++) {
      avg_r = 0; 
      avg_g = 0;
      avg_b = 0;
      for (int i = 0; i < SCROLL_AVG_FACTOR; i++) {
        avg_r += colors_grid_wide[x * SCROLL_AVG_FACTOR + i][y].red;
        avg_g += colors_grid_wide[x * SCROLL_AVG_FACTOR + i][y].green;
        avg_b += colors_grid_wide[x * SCROLL_AVG_FACTOR + i][y].blue;
      }

      leds[grid_to_idx(x, y, false)] = CRGB(avg_r / SCROLL_AVG_FACTOR, avg_g / SCROLL_AVG_FACTOR, avg_b / SCROLL_AVG_FACTOR);
    }
  }
}

void display_led_demo(SpotifyPlayerData_t *sp_data) {
  // Call the current pattern function once, updating the 'leds' array
  //gPatterns[gCurrentPatternNumber]();
  bpm(int(sp_data->tempo / 2));

  // send the 'leds' array out to the actual LED strip
  FastLED.show();  
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FPS); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  //EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
}

void move_servo() {
  if (servo_pos > SERVO_START_POS) servo_pos = SERVO_START_POS;
  if (servo_pos < SERVO_END_POS) servo_pos = SERVO_END_POS;
  if (myservo.read() != servo_pos) {    // only try to move if we are going to a new position
    Serial.println("Servo pos: " + String(servo_pos));
    myservo.write(servo_pos);
  }
}

/*** Get Index of LED on a serpentine grid ***/
int grid_to_idx(int x, int y, bool start_top_left) {
  /** 
   * Coordinate system starts with (0, 0) at the bottom corner of grid.
   * Assumes a serpentine grid with the first pixel at the bottom left corner.
   * Even rows count left->right, odd rows count right->left.
   **/

  int idx = 0;

  if ((x >= GRID_W) || (y >= GRID_H)) {   // we are outside the grid
    return -1;
  }

  if (start_top_left) {
    y = GRID_H - 1 - y;
  }

  if (y % 2 == 0) {
    idx = GRID_W * y + x;
  } else {
    idx = GRID_W * y + (GRID_W - x - 1);
  }

  return idx;
}

void display_full_art(uint8_t idx) {
  for (int row = 0; row < GRID_H; row++) {
    for (int col = 0; col < GRID_W; col++) {
      uint8_t full_row = row*4 + idx;
      uint8_t full_col = col*4 + idx;

      // Select the last row/col so artwork with borders looks cleaner
      if (row == GRID_H-1) full_row = GRID_H * 4 - 1;
      if (col == GRID_W-1) full_col = GRID_W * 4 - 1;

      uint16_t rgb565 = full_art[full_row][full_col];
      
      uint8_t r5 = (rgb565 >> 11) & 0x1F;
      uint8_t g6 = (rgb565 >> 5) & 0x3F;
      uint8_t b5 = (rgb565) & 0x1F;

      uint8_t r8 = round(pow(float(r5) / 31, LED_GAMMA / JPG_GAMMA) * 255);
      uint8_t g8 = round(pow(float(g6) / 63, LED_GAMMA / JPG_GAMMA) * 255);
      uint8_t b8 = round(pow(float(b5) / 31, LED_GAMMA / JPG_GAMMA) * 255);

      // Serial.println(r5);
      // Serial.println(g6);
      // Serial.println(b5);

      // Serial.println(r8);
      // Serial.println(g8);
      // Serial.println(b8);

      // Serial.println();

      int idx = grid_to_idx(col, row, true);
      if (idx >= 0) {
        leds[idx] = CRGB(r8, g8, b8);
      }
    }
  }
}

bool copy_jpg_data(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint8_t full_row = y + row;
      uint8_t full_col = x + col;
      uint8_t bitmap_idx = row * w + col;
      full_art[full_row][full_col] = bitmap[bitmap_idx];
      if ((full_row % 4 == 0) && (full_col % 4 == 0)) {
        palette_art[int(full_row / 4)][int(full_col / 4)] = bitmap[bitmap_idx];
      }
    }
  }

  return true;
}

bool display_image(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Note bitmap is in RGB565 format!!!

  // Stop further decoding as image is running off bottom of screen
  x /= 4;
  y /= 4;
  
  if (y >= GRID_H) return false;

  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint16_t rgb565 = bitmap[row * w * 4 + col * 4];
      uint8_t r5 = (rgb565 >> 11) & 0x1F;
      uint8_t g6 = (rgb565 >> 5) & 0x3F;
      uint8_t b5 = (rgb565) & 0x1F;

      uint8_t r8 = round(pow(float(r5) / 31, LED_GAMMA / JPG_GAMMA) * 255);
      uint8_t g8 = round(pow(float(g6) / 63, LED_GAMMA / JPG_GAMMA) * 255);
      uint8_t b8 = round(pow(float(b5) / 31, LED_GAMMA / JPG_GAMMA) * 255);

      // Serial.println(r5);
      // Serial.println(g6);
      // Serial.println(b5);

      // Serial.println(r8);
      // Serial.println(g8);
      // Serial.println(b8);

      // Serial.println();

      int idx = grid_to_idx(x + col, y + row, true);
      if (idx >= 0) {
        leds[idx] = CRGB(r8, g8, b8);
      }
    }
  }

  return true;
}

void get_spotify_token() {
  Serial.println("Getting Spotify token...");

  HTTPClient http;
  http.begin(SPOTIFY_GET_TOKEN_URL);
  http.addHeader("Cookie", SPOTIFY_COOKIE);

  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_FOUND) { 
    String payload = http.getLocation();
    token = payload.substring(payload.indexOf("access_token=") + 13, payload.indexOf("&token_type"));
    Serial.print("Token: ");
    Serial.println(token);
  } else {
    Serial.println(String(httpCode) + ": Error retrieving token");
  }

  http.end();
}

void get_spotify_player(SpotifyPlayerData_t *sp_data) {
  HTTPClient http;
  
  http.begin(SPOTIFY_PLAYER_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + token);

  int httpCode = http.GET();
  String payload = http.getString();
  http.end();
  // Serial.println(payload);

  switch (httpCode) {   // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
    case HTTP_CODE_OK:
      {
        DynamicJsonDocument json(20000);
        deserializeJson(json, payload);
        update_spotify_data(&json, sp_data);

        token_expired = false;
        break;
      }
    case HTTP_CODE_NO_CONTENT:
      Serial.println(String(httpCode) + ": Playback not available/active");
      token_expired = false;
      break;
    case HTTP_CODE_BAD_REQUEST:
    case HTTP_CODE_UNAUTHORIZED:
    case HTTP_CODE_FORBIDDEN:
      Serial.println(String(httpCode) + ": Bad/expired token or OAuth request");
      token_expired = true;
      break;
    case HTTP_CODE_TOO_MANY_REQUESTS:
      Serial.println(String(httpCode) + ": Exceeded rate limits");
      token_expired = false;
      break;
    default:
      Serial.println(String(httpCode) + ": Unrecognized error");
      token_expired = true;
      break;
  }
}

void get_and_update_spotify_features(SpotifyPlayerData_t *sp_data) {
  HTTPClient http;
  
  http.begin(SPOTIFY_FEATURES_URL + "/" + sp_data->track_id);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + token);

  int httpCode = http.GET();
  String payload = http.getString();
  http.end();
  // Serial.println(payload);

  switch (httpCode) {   // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
    case HTTP_CODE_OK:
      {
        DynamicJsonDocument json(20000);
        deserializeJson(json, payload);

        sp_data->tempo = json["tempo"].as<float>();
        sp_data->energy = json["energy"].as<float>();

        token_expired = false;
        break;
      }
    case HTTP_CODE_NO_CONTENT:
      Serial.println(String(httpCode) + ": Playback not available/active");
      token_expired = false;
      break;
    case HTTP_CODE_BAD_REQUEST:
    case HTTP_CODE_UNAUTHORIZED:
    case HTTP_CODE_FORBIDDEN:
      Serial.println(String(httpCode) + ": Bad/expired token or OAuth request");
      token_expired = true;
      break;
    case HTTP_CODE_TOO_MANY_REQUESTS:
      Serial.println(String(httpCode) + ": Exceeded rate limits");
      token_expired = false;
      break;
    default:
      Serial.println(String(httpCode) + ": Unrecognized error");
      token_expired = true;
      break;
  }
}


void update_spotify_data(DynamicJsonDocument *json, SpotifyPlayerData_t *sp_data) {
  if (sp_data->track_id != (*json)["item"]["id"].as<String>()) {    // if the track has changed
    sp_data->track_changed = true;
    sp_data->track_id = (*json)["item"]["id"].as<String>();
    sp_data->track_title = (*json)["item"]["name"].as<String>();
    sp_data->album = (*json)["item"]["album"]["name"].as<String>();
    
    JsonArray arr = (*json)["item"]["artists"].as<JsonArray>();
    sp_data->artists = "";
    for (JsonVariant value : arr) {
      sp_data->artists += value["name"].as<String>() + " ";
    }
    sp_data->is_active = (*json)["device"]["is_active"].as<bool>();
    sp_data->device_type = (*json)["device"]["type"].as<String>();
    sp_data->progress_ms = (*json)["progress_ms"].as<int>();
    sp_data->duration_ms = (*json)["item"]["duration_ms"].as<int>();
    sp_data->is_playing = (*json)["is_playing"].as<bool>();
    sp_data->device = (*json)["device"]["name"].as<String>();
    sp_data->volume = (*json)["device"]["volume_percent"].as<int>();

    int art_url_count = (*json)["item"]["album"]["images"].size(); // count number of album art URLs
    sp_data->art_url = (*json)["item"]["album"]["images"][art_url_count - 1]["url"].as<String>(); // the last one is the smallest, typically 64x64
    sp_data->art_url.replace("https", "http");  // needed to eliminate SSL handshake errors
    sp_data->art_width = (*json)["item"]["album"]["images"][art_url_count - 1]["width"].as<int>();
    sp_data->is_art_loaded = false;

    if (sp_data->art_url == NULL) {
      sp_data->track_id = "";  // force next loop to check json again
    } else {
      get_spotify_art(sp_data);
      decode_art(sp_data);
    }
  } else {  // if it's the same track, just show how far we've advanced
    sp_data->track_changed = false;
    sp_data->progress_ms = (*json)["progress_ms"].as<int>();
    sp_data->duration_ms = (*json)["item"]["duration_ms"].as<int>();
  }
}

// Decode art from jpg into full_art, and calculate palette
void decode_art(SpotifyPlayerData_t *sp_data) {
  Serial.println("Decoding art, " + String(sp_data->art_num_bytes) + " bytes");
  TJpgDec.drawJpg(0, 0, sp_data->art_data, sp_data->art_num_bytes); // decode and copy jpg data into full_art

  // Calculate color palette
  mean_cut((uint16_t *)palette_art, 16*16, MEAN_CUT_DEPTH, (uint8_t *)palette_results);
  Serial.println("Finished mean cut, printing returned results");
  for (int i=0; i < PALETTE_ENTRIES; i++) {
    Serial.println(String(palette_results[i][0]) + "," + String(palette_results[i][1]) + "," + String(palette_results[i][2]));
    uint8_t r8 = round(pow(float(palette_results[i][0]) / 255, LED_GAMMA / JPG_GAMMA) * 255);
    uint8_t g8 = round(pow(float(palette_results[i][1]) / 255, LED_GAMMA / JPG_GAMMA) * 255);
    uint8_t b8 = round(pow(float(palette_results[i][2]) / 255, LED_GAMMA / JPG_GAMMA) * 255);
    palette_crgb[i] = CRGB(r8, g8, b8);
  }

  target_palette = CRGBPalette16(palette_crgb[0], palette_crgb[1], palette_crgb[2], palette_crgb[3],
                                palette_crgb[4], palette_crgb[5], palette_crgb[6], palette_crgb[7],
                                palette_crgb[8], palette_crgb[9], palette_crgb[10], palette_crgb[11],
                                palette_crgb[12], palette_crgb[13], palette_crgb[14], palette_crgb[15]);
}

// Fetch a file from the URL given and save it to memory
void get_spotify_art(SpotifyPlayerData_t *sp_data) {
  // If it exists then no need to fetch it
  // if (SPIFFS.exists(filename) == true) {
  //   Serial.println("Found " + filename);
  //   return 0;
  // }

  // if (SPIFFS.exists(filename) == true) {
  //     SPIFFS.remove(filename);  // since we will always use the same filename
  // }

  int start_ms = millis();
  Serial.println("Downloading " + sp_data->art_url);
  //Serial.print("[HTTP] begin...\n");

  HTTPClient http;
  // Configure server and url
  http.begin(sp_data->art_url);
  //Serial.print("[HTTP] GET...\n");

  // Start connection and send HTTP header
  int httpCode = http.GET();
  // if (httpCode > 0) {
  //   // fs::File f = SPIFFS.open(filename, "w+");
  //   // if (!f) {
  //   //   Serial.println("file open failed");
  //   //   return 0;
  //   // }
  //   // HTTP header has been send and Server response header has been handled
  //   //Serial.printf("[HTTP] GET... code: %d\n", httpCode);

  //   // File found at server
  if (httpCode == HTTP_CODE_OK) {

    // Get length of document (is -1 when Server sends no Content-Length header)
    int total = http.getSize();

    sp_data->art_num_bytes = total;

    // Allocate memory in heap for the downloaded data
    if (sp_data->art_data != NULL) {
      Serial.println("Free heap prior to free: " + String(ESP.getFreeHeap()));
      free(sp_data->art_data);   // if not NULL, we have previously allocated memory and should free it
    }
    Serial.println("Free heap prior to malloc: " + String(ESP.getFreeHeap()));
    sp_data->art_data = (uint8_t *)malloc(sp_data->art_num_bytes * sizeof(*(sp_data->art_data))); // dereference the art_data pointer to get the size of each element (uint8_t)

    int len = total;

    // Create buffer for read
    uint8_t buff[128] = { 0 };

    // Get tcp stream
    WiFiClient *stream = http.getStreamPtr();

    // Read all data from server
    while (http.connected() && (len > 0 || len == -1)) {
      // Get available data size
      size_t size = stream->available();

      if (size) {
        // Read up to 128 bytes
        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

        // Write it to file
        // f.write(buff, c);

        // Write it to memory, (curr_art_size - len) is the number of bytes already written
        memcpy(&(sp_data->art_data)[sp_data->art_num_bytes - len], buff, c);

        // Calculate remaining bytes
        if (len > 0) {
          len -= c;
        }
      }
      yield();
    }
    //Serial.println();
    //Serial.print("[HTTP] connection closed or file end.\n");
    sp_data->is_art_loaded = true;
    Serial.println(String(millis() - start_ms) + "ms to download art");
  }
    // f.close();
  else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    sp_data->is_art_loaded = false;
  }
  http.end();
}

void print_spotify_data(SpotifyPlayerData_t *sp_data) {
  Serial.print("\tTitle: "); Serial.println(sp_data->track_title);
  Serial.print("\tArtist: "); Serial.println(sp_data->artists);
  Serial.print("\tAlbum: "); Serial.println(sp_data->album);
  Serial.print("\tAlbum art: "); Serial.println(sp_data->art_url);
  Serial.print("\tAlbum art width: "); Serial.println(sp_data->art_width);
  Serial.print("\tDuration (ms): "); Serial.println(sp_data->duration_ms);
  Serial.print("\tProgress (ms): "); Serial.println(sp_data->progress_ms);
  Serial.print("\tPlaying: "); Serial.println(sp_data->is_playing);
  Serial.print("\tDevice: "); Serial.println(sp_data->device);
  Serial.print("\tType: "); Serial.println(sp_data->device_type);
  Serial.print("\tActive: "); Serial.println(sp_data->is_active);
  Serial.print("\tVolume: "); Serial.println(sp_data->volume);
  Serial.print("\tTempo: "); Serial.println(sp_data->tempo);
  Serial.print("\tEnergy: "); Serial.println(sp_data->energy);
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

void bpm(uint8_t bpm = 62)
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = bpm;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 0, 255);
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

void lissajous() {
  FastLED.clear();

  static uint16_t startHue = 0;
  static uint16_t xPhase = 0;
  static uint16_t yPhase = 64 * 256;
  uint8_t pixelHue = startHue;
  for (uint16_t i = 0; i < 128; i++) {
    uint16_t x = 32767 + cos16(xPhase + i * 2 * 256);
    uint16_t y = 32767 + sin16(yPhase + i * 2 * 256);
    x /= 256 / (GRID_W - 1);
    y /= 256 / (GRID_H - 1);
    CRGB col = ColorFromPalette(RainbowColors_p, pixelHue, 255, LINEARBLEND);
    wu_pixel(x, y, & col);
    pixelHue += 2;
  }
  // fadeToBlackBy(leds, NUM_LEDS, 32);
  startHue += 3;
  xPhase += 512;  
}

void wu_pixel(uint16_t x, uint16_t y, CRGB* col) {
  // extract the fractional parts and derive their inverses
  uint8_t xx = x & 0xff, yy = y & 0xff, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  #define WU_WEIGHT(a, b) ((uint8_t)(((a) * (b) + (a) + (b)) >> 8))
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)
                  };
  #undef WU_WEIGHT
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (uint8_t i = 0; i < 4; i++) {
    uint8_t local_x = (x >> 8) + (i & 1);
    uint8_t local_y = (y >> 8) + ((i >> 1) & 1);
    uint16_t xy = grid_to_idx(local_x, local_y, false);
    if (xy > NUM_LEDS) continue;
    leds[xy].r = qadd8(leds[xy].r, col->r * wu[i] >> 8);
    leds[xy].g = qadd8(leds[xy].g, col->g * wu[i] >> 8);
    leds[xy].b = qadd8(leds[xy].b, col->b * wu[i] >> 8);
  }
}
