#ifndef _SPOTIFY_H
#define _SPOTIFY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Constants.h"
#include "NetworkConstants.h"

class Spotify {
   public:
    // Constructor
    Spotify();

    // Struct definition
    struct album_art_t {
        bool loaded;
        bool changed;
        String url;
        uint16_t width;
        uint8_t *data;
        unsigned long num_bytes;
    };

    // Methods

    // Get the latest Spotify data
    void
    update();

    // Print the details of current Spotify playback to Serial
    void print();

    // Return a value from 0 to 1.0 that indicates the progress in the current track
    double get_track_progress();

    // Indicates if Spotify is currently running on the linked account
    bool is_active();

    // Returns a struct with album art details
    album_art_t get_album_art();

   private:
    // Methods

    // Get an authenticated token for use with the Spotify Web API
    bool _get_token();

    // Get details on current Spotify playback via the Web API
    bool _get_player();

    // Get details on current Spotify track via the Web API
    bool _get_features();

    // Retrieve album art for the currently playing track
    bool _get_art();

    // Parse the json response from the Spotify Web API
    void _parse_json(DynamicJsonDocument *json);

    // Reset member variables to default values
    void _reset_variables();

    // Variables
    bool _token_expired = true;
    String _token = "";

    // Variables
    unsigned long _progress_ms = 0;
    unsigned long _duration_ms = 0;
    uint8_t _volume = 0;
    String _track_id = "";
    String _artists = "";
    String _track_title = "";
    String _album = "";
    String _device = "";
    String _device_type = "";
    bool _is_active = false;
    bool _is_playing = false;
    double _tempo = 0.0;
    double _energy = 0.0;
    bool _track_changed = false;

    album_art_t _album_art{
        .loaded = false,
        .changed = false,
        .url = "",
        .width = 0,
        .data = NULL,
        .num_bytes = 0};
};

#endif  // _SPOTIFY_H
