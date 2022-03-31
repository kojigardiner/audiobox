#include <Arduino.h>
#include <FastLED.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>
#include <NetworkConstants.h>
#include <SpotifyData.h>

/*** Defines ***/
#define PIN_LED_CONTROL     12              // LED strip control GPIO
#define PIN_LED_STATUS      2               // Status LED on HiLetgo board
#define MAX_BRIGHT          100              // sets max brightness for LEDs
#define JPG_GAMMA           2.2
#define LED_GAMMA           2.8

#define GRID_H              16
#define GRID_W              16
#define NUM_LEDS            GRID_H * GRID_W
#define FRAMES_PER_SECOND   120

#define PIN_SERVO           18              // Servo pin
#define SERVO_MIN_US        700
#define SERVO_MAX_US        2400
#define SERVO_START_POS     130
#define SERVO_END_POS       0

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))  // helper macro

/*** Function Prototypes ***/

// LED Functions
void nextPattern();
void rainbow();
void rainbowWithGlitter();
void addGlitter( fract8 chanceOfGlitter);
void confetti();
void sinelon();
void bpm();
void juggle();

// Spotify Functinos
void get_spotify_token();
void get_spotify_player(SpotifyPlayerData_t *sp_data);
void update_spotify_data(DynamicJsonDocument *json, SpotifyPlayerData *sp_data);
void print_spotify_data(SpotifyPlayerData *sp_data);

// Display Functions
bool display_image(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);   // callback function for JPG decoder
int grid_to_idx(int x, int y, bool start_top_left);
void get_spotify_art(SpotifyPlayerData_t *sp_data);

/*** Globals ***/

// Task & Queue Handles
TaskHandle_t task_spotify;
TaskHandle_t task_display;
TaskHandle_t task_servo;
QueueHandle_t q_sp_data;
void task_spotify_code(void *parameter);
void task_display_code(void *parameter);
void task_servo_code(void *parameter);

// LED Globals
CRGB leds[NUM_LEDS];                      // array that will hold LED values
typedef void (*SimplePatternList[])();    // List of patterns to cycle through.  Each is defined as a separate function below.
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
uint8_t gCurrentPatternNumber = 0;        // Index number of which pattern is current
uint8_t gHue = 0;                         // rotating "base color" used by many of the patterns

// Servo Globals
Servo myservo;
int servo_pos = SERVO_START_POS;

// Spotify Globals
bool token_expired = true;
String token = "";

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

  // Servo Setup
  Serial.print("Setting up servo\n");
  myservo.write(servo_pos);     // Set servo zero position prior to attaching in order to mitigate power-on glitch
  myservo.attach(PIN_SERVO, SERVO_MIN_US, SERVO_MAX_US);

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
  TJpgDec.setJpgScale(4);           // Assuming 64x64 image, will rescale to 16x16
  TJpgDec.setCallback(display_image);  // The decoder must be given the exact name of the rendering function above

  // Filessystem setup
  Serial.print("Setting up filesystem\n");
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Serial.print("Setup complete\n\n");

  // Task setup
  xTaskCreate(
    task_spotify_code,      // Function to implement the task
    "task_spotify",         // Name of the task
    10000,                  // Stack size in words
    NULL,                   // Task input parameter
    1,                      // Priority of the task (don't use 0!)
    &task_spotify           // Task handle
  );

  xTaskCreate(
    task_display_code,      // Function to implement the task
    "task_display",         // Name of the task
    10000,                  // Stack size in words
    NULL,                   // Task input parameter
    1,                      // Priority of the task (don't use 0!)
    &task_display           // Task handle
  );

  xTaskCreate(
    task_servo_code,        // Function to implement the task
    "task_servo",           // Name of the task
    10000,                  // Stack size in words
    NULL,                   // Task input parameter
    1,                      // Priority of the task (don't use 0!)
    &task_servo             // Task handle
  );

  q_sp_data = xQueueCreate(1, sizeof(SpotifyPlayerData_t));

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
  // Serial.print("Free heap: ");
  // Serial.println(ESP.getFreeHeap());
  
  // // Call the current pattern function once, updating the 'leds' array
  // gPatterns[gCurrentPatternNumber]();
  
  // // send the 'leds' array out to the actual LED strip
  // FastLED.show();  
  // // insert a delay to keep the framerate modest
  // FastLED.delay(1000/FRAMES_PER_SECOND); 

  // // do some periodic updates
  // EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  // EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
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
      xQueueSend(q_sp_data, &sp_data, 0); // set timeout to zero so loop will continue until display is updated
    } else {
      Serial.println("Error: WiFi not connected! status = " + String(WiFi.status()));
    }
  }
}

void task_display_code(void *parameter) {
  Serial.print("task_display_code running on core ");
  Serial.println(xPortGetCoreID());

  SpotifyPlayerData_t sp_data;
  String curr_track_id = "";
  CRGB leds_last_row[GRID_W];   // save last row of album art so we can deal with progress bar moving backward

  for (;;) {
    xQueueReceive(q_sp_data, &sp_data, portMAX_DELAY);

    if (sp_data.track_id != curr_track_id && sp_data.is_art_loaded) {
      Serial.println("Track changed");
      curr_track_id = sp_data.track_id;
      Serial.println("Drawing art, " + String(sp_data.art_num_bytes) + " bytes");
      TJpgDec.drawJpg(0, 0, sp_data.art_data, sp_data.art_num_bytes);

      for (int i=0; i<GRID_W; i++) {
        leds_last_row[i] = leds[i];
      }
    }

    // Update the LED indicator at the bottom of the array
    double percent_complete = double(sp_data.progress_ms) / sp_data.duration_ms * 100;
    Serial.println("Playing..." + String(percent_complete) + "%");

    int grid_pos = int(round(percent_complete / 100 * GRID_W));

    for (int i=0; i<GRID_W; i++) {
      if (i < grid_pos) {
        leds[i] = CRGB::Red;
      } else {
        leds[i] = leds_last_row[i];
      }
    }

    FastLED.show();
  }
}

void task_servo_code(void *parameter) {
  Serial.print("task_servo_code running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    if (Serial.available() > 0) {
      char c = Serial.read();
      switch (c) {
        case 'f':
          servo_pos -= 1;
          break;
        case 'b':
          servo_pos += 1;
          break;
      }
      Serial.println("Servo pos: " + String(servo_pos));
      myservo.write(servo_pos);
    }
    // if (servo_pos > SERVO_START_POS) servo_pos = SERVO_START_POS;
    // if (servo_pos < SERVO_END_POS) servo_pos = SERVO_END_POS;
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

bool display_image(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  // Note bitmap is in RGB565 format!!!

  // Stop further decoding as image is running off bottom of screen
  if (y >= GRID_H) return false;

  for (int row = 0; row < h; row++) {
    for (int col = 0; col < w; col++) {
      uint16_t rgb565 = bitmap[row * w + col];
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
  // Serial.println(payload);

  Serial.print(String(httpCode) + ": ");
  switch (httpCode) {   // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
    case HTTP_CODE_OK:
      {
        Serial.println("Information about playback");

        DynamicJsonDocument json(20000);
        deserializeJson(json, payload);
        update_spotify_data(&json, sp_data);

        token_expired = false;
        break;
      }
    case HTTP_CODE_NO_CONTENT:
      Serial.println("Playback not available/active");
      token_expired = false;
      break;
    case HTTP_CODE_BAD_REQUEST:
    case HTTP_CODE_UNAUTHORIZED:
    case HTTP_CODE_FORBIDDEN:
      Serial.println("Bad/expired token or OAuth request");
      token_expired = true;
      break;
    case HTTP_CODE_TOO_MANY_REQUESTS:
      Serial.println("Exceeded rate limits");
      token_expired = false;
      break;
    default:
      Serial.println("Unrecognized error");
      token_expired = true;
      break;
  }

  http.end();
}

void update_spotify_data(DynamicJsonDocument *json, SpotifyPlayerData_t *sp_data) {
  if (sp_data->track_id != (*json)["item"]["id"].as<String>()) {
    sp_data->track_id = (*json)["item"]["id"].as<String>();
    sp_data->track_title = (*json)["item"]["name"].as<String>();
    sp_data->album = (*json)["item"]["album"]["name"].as<String>();
    
    JsonArray arr = (*json)["item"]["artists"].as<JsonArray>();
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
    sp_data->art_width = (*json)["item"]["album"]["images"][art_url_count - 1]["width"].as<int>();
    sp_data->is_art_loaded = false;

    if (sp_data->art_url == NULL) {
      sp_data->track_id = "";  // force next loop to check json again
    } else {
      get_spotify_art(sp_data);
    }

    print_spotify_data(sp_data);
  } else {  // if it's the same track, just show how far we've advanced
    sp_data->progress_ms = (*json)["progress_ms"].as<int>();
    sp_data->duration_ms = (*json)["item"]["duration_ms"].as<int>();
  }
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
  if (httpCode > 0) {
    // fs::File f = SPIFFS.open(filename, "w+");
    // if (!f) {
    //   Serial.println("file open failed");
    //   return 0;
    // }
    // HTTP header has been send and Server response header has been handled
    //Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // File found at server
    if (httpCode == HTTP_CODE_OK) {

      // Get length of document (is -1 when Server sends no Content-Length header)
      int total = http.getSize();

      sp_data->art_num_bytes = total;

      // Allocate memory in heap for the downloaded data
      if (sp_data->art_data != NULL) {
        free (sp_data->art_data);   // if not NULL, we have previously allocated memory and should free it
      }
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
    }
    // f.close();
  }
  else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();

  sp_data->is_art_loaded = true;
  Serial.println(String(millis() - start_ms) + "ms to download art");
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

