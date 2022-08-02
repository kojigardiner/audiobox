#ifndef _SPOTIFY_H
#define _SPOTIFY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Constants.h"

const char SPOTIFY_AUTH_URL[] = "https://accounts.spotify.com/authorize";
const char SPOTIFY_TOKEN_URL[] = "https://accounts.spotify.com/api/token";
const char SPOTIFY_REDIRECT_URI[] = "http%3A%2F%2Fhttpbin.org%2Fanything";
const char SPOTIFY_SCOPE[] = "user-read-playback-state+user-read-playback-position";  //+user-modify-playback-state"
const char SPOTIFY_PLAYER_URL[] = "https://api.spotify.com/v1/me/player";
const char SPOTIFY_FEATURES_URL[] = "https://api.spotify.com/v1/audio-features";

class Spotify {
   public:
    // Constructor
    Spotify() = default;
    Spotify(const char *client_id, const char *auth_b64, const char *refresh_token);

    // Struct definition
    struct album_art_t {
        bool loaded;
        bool changed;
        char url[CLI_MAX_CHARS];
        uint16_t width;
        uint8_t *data;
        unsigned long num_bytes;
    };

    struct public_data_t {
        bool is_active;
        double track_progress;
        album_art_t album_art;
    };

    // Static methods for initial account setup

    // CLI flow for requesting user authorization to access their account
    static bool
    request_user_auth(const char *client_id, const char *auth_b64, char *refresh_token);
    static bool get_refresh_token(const char *auth_b64, char *refresh_token);

    // Methods

    // Get the latest Spotify data
    void update();

    // Print the details of current Spotify playback to Serial
    void print_info();

    // Return a value from 0 to 1.0 that indicates the progress in the current track
    double get_track_progress();

    // Indicates if Spotify is currently running on the linked account
    bool is_active();

    // Returns a struct with public data
    public_data_t get_data();

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

    // Helper function to workaround https memory issue
    void _replace_https_with_http(char *url);

    // Variables
    bool _token_expired;
    char _token[CLI_MAX_CHARS];
    char _refresh_token[CLI_MAX_CHARS];
    char _client_id[CLI_MAX_CHARS];
    char _auth_b64[CLI_MAX_CHARS];

    unsigned long _progress_ms;
    unsigned long _duration_ms;
    uint8_t _volume;
    char _track_id[CLI_MAX_CHARS];
    char _artists[CLI_MAX_CHARS];
    char _track_title[CLI_MAX_CHARS];
    char _album[CLI_MAX_CHARS];
    char _device[CLI_MAX_CHARS];
    char _device_type[CLI_MAX_CHARS];
    bool _is_active;
    bool _is_playing;
    double _tempo;
    double _energy;
    bool _track_changed;

    public_data_t _public_data;
};

#endif  // _SPOTIFY_H
