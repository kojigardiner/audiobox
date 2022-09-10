#ifndef _SPOTIFY_H
#define _SPOTIFY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Constants.h"

const char SPOTIFY_AUTH_URL[] = "https://accounts.spotify.com/authorize";
const char SPOTIFY_TOKEN_URL[] = "https://accounts.spotify.com/api/token";
// const char SPOTIFY_REDIRECT_URI[] = "http%3A%2F%2Fhttpbin.org%2Fanything";
const char SPOTIFY_REDIRECT_URI[] = "http%3A%2F%2F192.168.3.147%2Fspotify-auth";
const char SPOTIFY_SCOPE[] = "user-read-playback-state+user-read-playback-position";  //+user-modify-playback-state"
const char SPOTIFY_PLAYER_URL[] = "https://api.spotify.com/v1/me/player";
const char SPOTIFY_USER_URL[] = "https://api.spotify.com/v1/me";
const char SPOTIFY_FEATURES_URL[] = "https://api.spotify.com/v1/audio-features";
const char SPOTIFY_JSON_FILENAME[] = "/spotify.json";

// Size (in bytes) to allocate in the DynamicJsonDocument to deserialize the json
// Commented values were empirically found to be ~averages
#define SPOTIFY_TOKEN_JSON_SIZE 2000          // ~300 bytes
#define SPOTIFY_PLAYER_JSON_SIZE 2000         // 5000~6000 bytes ==> 2000 bytes of json memory after filtering
#define SPOTIFY_FEATURES_JSON_SIZE 2000       // ~600 bytes
#define SPOTIFY_REFRESH_TOKEN_JSON_SIZE 2000  // ~500 bytes
#define SPOTIFY_USER_JSON_SIZE 2000

class Spotify {
   public:
    // Constructor
    Spotify() = default;
    Spotify(const char *client_id, const char *auth_b64, const char *refresh_token);

    // Struct definition
    struct album_art_t {
        bool loaded = false;
        bool changed = false;
        char url[CLI_MAX_CHARS] = {0};
        uint16_t width = 0;
        uint8_t *data = NULL;
        unsigned long num_bytes = 0;
    };

    struct public_data_t {
        bool is_active = false;
        double track_progress = 0.0;

        bool art_loaded = false;
        bool art_changed = false;
        uint16_t art_width = 0;
        uint8_t *art_data = NULL;
        unsigned long art_num_bytes = 0;
    };

    // Static methods for initial account setup

    // CLI flow for requesting user authorization to access their account
    static bool request_user_auth(const char *client_id, const char *auth_b64, char *auth_url);
    static bool get_refresh_token(const char *auth_b64, const char *auth_code, char *refresh_token);

    // Methods

    // Get the latest Spotify data
    void update();

    // Print the details of current Spotify playback to Serial
    void print_info();

    // Return a value from 0 to 1.0 that indicates the progress in the current track
    double get_track_progress();

    // Indicates if Spotify is currently running on the linked account
    bool is_active();

    // Get the user name for the currently linked account
    void get_user_name(char *user_name);

    // Get the url for the current album art
    void get_art_url(char *url);

    // Get the current artist name
    void get_artist_name(char *artist_name);

    // Get the current album name
    void get_album_name(char *album_name);

    // Returns a struct with public data
    public_data_t get_data();

   private:
    // Methods

    // Get an authenticated token for use with the Spotify Web API
    bool _get_token();

    // Get the current user's profile via the Web API
    bool _get_user_profile();

    // Get details on current Spotify playback via the Web API
    bool _get_player();

    // Get details on current Spotify track via the Web API
    bool _get_features();

    // Retrieve album art for the currently playing track
    bool _get_art();

    // Parse the json response from the Spotify Web API
    void _parse_json(JsonDocument *json);

    // Reset member variables to default values
    void _reset_variables();

    // Helper function to workaround https memory issue
    void _replace_https_with_http(char *url);

    // Write the http response to a specified file
    static int _write_http_response_to_file(HTTPClient *http, const char *filename);

    // Populate the json document with contents of the specified file
    static DeserializationError _deserialize_json_from_file(JsonDocument *json, const char *filename);
    static DeserializationError _deserialize_json_from_string(JsonDocument *json, String *response, bool use_filter = false);

    // Variables
    bool _token_expired;
    char _token[CLI_MAX_CHARS];
    char _refresh_token[CLI_MAX_CHARS];
    char _client_id[CLI_MAX_CHARS];
    char _auth_b64[CLI_MAX_CHARS];
    char _user_name[CLI_MAX_CHARS];

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

    album_art_t _album_art;
    public_data_t _public_data;
};

#endif  // _SPOTIFY_H