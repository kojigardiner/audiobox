#include <Arduino.h>

// Wifi Globals
const char* WIFI_HOSTNAME = "ESP32 LED 16x16";
const char* WIFI_SSID = "***REMOVED***";
const char* WIFI_PASS = "***REMOVED***";
// const char* WIFI_SSID = "***REMOVED***";
// const char* WIFI_PASS = "***REMOVED***";

// Spotify Globals
const String SPOTIFY_CLIENT_ID = "***REMOVED***";
const String SPOTIFY_REDIRECT_URI = "http%3A%2F%2Fhttpbin.org%2Fanything";
const String SPOTIFY_COOKIE = "***REMOVED***";
const String SPOTIFY_GET_TOKEN_URL = "https://accounts.spotify.com/authorize?response_type=token&redirect_uri=" + SPOTIFY_REDIRECT_URI + "&client_id=" + SPOTIFY_CLIENT_ID + "&scope=user-read-playback-state+user-read-playback-position+user-modify-playback-state&state=cryq3";
const String SPOTIFY_PLAYER_URL = "https://api.spotify.com/v1/me/player";
const String SPOTIFY_FEATURES_URL = "https://api.spotify.com/v1/audio-features";