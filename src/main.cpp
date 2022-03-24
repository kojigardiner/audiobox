#include <Arduino.h>
#include <FastLED.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <TJpg_Decoder.h>

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
void getToken();
void getPlayer();

// Display Functions
bool display_image(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);   // callback function for JPG decoder
int grid_to_idx(int x, int y, bool start_top_left);
bool getFile(String url, String filename);

/*** Globals ***/

// LED Globals
CRGB leds[NUM_LEDS];                      // array that will hold LED values
typedef void (*SimplePatternList[])();    // List of patterns to cycle through.  Each is defined as a separate function below.
SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
uint8_t gCurrentPatternNumber = 0;        // Index number of which pattern is current
uint8_t gHue = 0;                         // rotating "base color" used by many of the patterns

// Servo Globals
Servo myservo;
int servo_pos = SERVO_START_POS;

// Wifi Globals
const char* WIFI_HOSTNAME = "ESP32 LED 16x16";
const char* WIFI_SSID = "***REMOVED***";
const char* WIFI_PASS = "***REMOVED***";

// Spotify Globals
String SPOTIFY_CLIENT_ID = "***REMOVED***";
String SPOTIFY_REDIRECT_URI = "http%3A%2F%2Fhttpbin.org%2Fanything";
String SPOTIFY_COOKIE = "***REMOVED***";
long current = 0, duration = 0;
int volume;
String artists = "", title, album, device, type, art_url, curr_uri = "";
int art_width;
bool isActive, playing;

bool isExpired = false;
String token = "";

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
  WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.print("connected\n");
  Serial.print("IP: "); Serial.println(WiFi.localIP());
  Serial.print("RSSI: "); Serial.println(WiFi.RSSI());

  // Spotify Setup
  Serial.print("Setting up Spotify\n");

  Serial.print("Setup complete\n\n");
  //Serial.print("Servo control: f = forward, b = backward\n");

  // JPG decoder setup
  TJpgDec.setJpgScale(4);           // Assuming 64x64 image, will rescale to 16x16
  TJpgDec.setCallback(display_image);  // The decoder must be given the exact name of the rendering function above

  // Filessystem setup
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }
  
  File file = SPIFFS.open("/rainbows.jpg");
  if(!file){
    Serial.println("Failed to open file for reading");
    return;
  }
  file.close();
  
  // Write image
  // Get the width and height in pixels of the jpeg if you wish
  uint16_t w = 0, h = 0;
  TJpgDec.getFsJpgSize(&w, &h, "/rainbows.jpg"); // Note name preceded with "/"
  Serial.print("Width = "); Serial.print(w); Serial.print(", height = "); Serial.println(h);

  // Draw the image, top left at 0,0
  TJpgDec.drawFsJpg(0, 0, "/rainbows.jpg");

  FastLED.show();
}

void loop() {
  // Serial.print("Free heap: ");
  // Serial.println(ESP.getFreeHeap());
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
  
  if (isExpired) {
    getToken();
  }

  getPlayer();

  FastLED.show();
  
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

      uint8_t r8 = round((float(r5) / 31) * 255);
      uint8_t g8 = round((float(g6) / 63) * 255);
      uint8_t b8 = round((float(b5) / 31) * 255);

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

void getToken() {
  HTTPClient http;
  String url = "https://accounts.spotify.com/authorize?response_type=token&redirect_uri=" + SPOTIFY_REDIRECT_URI + "&client_id=" + SPOTIFY_CLIENT_ID + "&scope=user-read-playback-state+user-read-playback-position+user-modify-playback-state&state=cryq3";
  Serial.println(url);
  http.begin(url);
  http.addHeader("Cookie", SPOTIFY_COOKIE);
  int httpCode = http.GET();

  String payload = http.getLocation();
  token = payload.substring(payload.indexOf("access_token=") + 13, payload.indexOf("&token_type"));
  Serial.print("Token: ");
  Serial.println(token);

  http.end();
}

void getPlayer() {
  HTTPClient http;
  http.begin("https://api.spotify.com/v1/me/player");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Accept", "application/json");
  http.addHeader("Authorization", "Bearer " + token);
  int httpCode = http.GET();
  String payload = http.getString();
  // Serial.println(payload);
  if (httpCode == HTTP_CODE_OK) {
    DynamicJsonDocument json(20000);
    deserializeJson(json, payload);

    if (curr_uri != json["item"]["uri"].as<String>()) {
      curr_uri = json["item"]["uri"].as<String>();
      title = json["item"]["name"].as<String>();
      album = json["item"]["album"]["name"].as<String>();
      int art_url_count = json["item"]["album"]["images"].size();
      art_url = json["item"]["album"]["images"][art_url_count - 1]["url"].as<String>();
      art_width = json["item"]["album"]["images"][art_url_count - 1]["width"].as<int>();
      artists = "";
      JsonArray arr = json["item"]["artists"].as<JsonArray>();
      for (JsonVariant value : arr) {
        artists += value["name"].as<String>() + " ";
      }

      isActive = json["device"]["is_active"].as<bool>();
      type = json["device"]["type"].as<String>();

      current = json["progress_ms"].as<long>();
      duration = json["item"]["duration_ms"].as<long>();
      playing = json["is_playing"].as<bool>();
      device = json["device"]["name"].as<String>();

      volume = json["device"]["volume_percent"].as<int>();

      getFile(art_url, "/album_art.jpg");
      TJpgDec.drawFsJpg(0, 0, "/album_art.jpg");

      Serial.print("Title: ");
      Serial.println(title);
      Serial.print("Artist: ");
      Serial.println(artists);
      Serial.print("Album: ");
      Serial.println(album);
      Serial.print("Album art: ");
      Serial.println(art_url);
      Serial.print("Album art width: ");
      Serial.println(art_width);
      Serial.print("Duration: ");
      Serial.println(duration);
      Serial.print("Current: ");
      Serial.println(current);
      Serial.print("Playing: ");
      Serial.println(playing);
      Serial.print("Device: ");
      Serial.println(device);
      Serial.print("Type: ");
      Serial.println(type);
      Serial.print("Active: ");
      Serial.println(isActive);
      Serial.print("Volume: ");
      Serial.println(volume);
    }

    isExpired = false;

  } else {
    current = 0, duration = 10;
    volume = 0;
    artists = "", title = "ERROR", album = "Token expired", device = "", type = "";
    isActive = false, playing = false;
    isExpired = true;
  }

  http.end();
}

// Fetch a file from the URL given and save it in SPIFFS
// Return 1 if a web fetch was needed or 0 if file already exists
bool getFile(String url, String filename) {
  // If it exists then no need to fetch it
  // if (SPIFFS.exists(filename) == true) {
  //   Serial.println("Found " + filename);
  //   return 0;
  // }

  if (SPIFFS.exists(filename) == true) {
      SPIFFS.remove(filename);  // since we will always use the same filename
  }

  Serial.println("Downloading "  + filename + " from " + url);

  // Check WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {

    Serial.print("[HTTP] begin...\n");

    HTTPClient http;
    // Configure server and url
    http.begin(url);

    Serial.print("[HTTP] GET...\n");
    // Start connection and send HTTP header
    int httpCode = http.GET();
    if (httpCode > 0) {
      fs::File f = SPIFFS.open(filename, "w+");
      if (!f) {
        Serial.println("file open failed");
        return 0;
      }
      // HTTP header has been send and Server response header has been handled
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // File found at server
      if (httpCode == HTTP_CODE_OK) {

        // Get length of document (is -1 when Server sends no Content-Length header)
        int total = http.getSize();
        int len = total;

        // Create buffer for read
        uint8_t buff[128] = { 0 };

        // Get tcp stream
        WiFiClient * stream = http.getStreamPtr();

        // Read all data from server
        while (http.connected() && (len > 0 || len == -1)) {
          // Get available data size
          size_t size = stream->available();

          if (size) {
            // Read up to 128 bytes
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            // Write it to file
            f.write(buff, c);

            // Calculate remaining bytes
            if (len > 0) {
              len -= c;
            }
          }
          yield();
        }
        Serial.println();
        Serial.print("[HTTP] connection closed or file end.\n");
      }
      f.close();
    }
    else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
  return 1; // File was fetched from web
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

