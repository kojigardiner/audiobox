#ifndef NETWORKCONSTANTS_H
#define NETWORKCONSTANTS_H

#include <Arduino.h>

// Spotify Globals
const String SPOTIFY_CLIENT_ID = "***REMOVED***";
const String SPOTIFY_REDIRECT_URI = "http%3A%2F%2Fhttpbin.org%2Fanything";
const String SPOTIFY_COOKIE = "***REMOVED***";
const String SPOTIFY_GET_TOKEN_URL = "https://accounts.spotify.com/authorize?response_type=token&redirect_uri=" + SPOTIFY_REDIRECT_URI + "&client_id=" + SPOTIFY_CLIENT_ID + "&scope=user-read-playback-state+user-read-playback-position+user-modify-playback-state&state=cryq3";
const String SPOTIFY_PLAYER_URL = "https://api.spotify.com/v1/me/player";
const String SPOTIFY_FEATURES_URL = "https://api.spotify.com/v1/audio-features";

#endif  // NETWORKCONSTANTS_H