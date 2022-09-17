#ifndef _SPOTIFY_H
#define _SPOTIFY_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Constants.h"

// The Spotify class is intended to be instantiated once, and encapsulates all interactions with
// the Spotify Web API. The main purpose of the Spotify object is to regularly query the API for
// information on the linked user's playback state/position. 
//
// The Spotify Web API provides query responses in json format. ArduinoJson is used to deserialize
// these responses and parse their contents for the information we need.
//
// The class also exposes a few static methods required for generating the authorization code and 
// refresh token needed to authenticate with the web API. Full documentation for the Spotify Web API
// can be found here: https://developer.spotify.com/documentation/web-api/.

// URLs and other constants
const char SPOTIFY_AUTH_URL[] = "https://accounts.spotify.com/authorize";
const char SPOTIFY_TOKEN_URL[] = "https://accounts.spotify.com/api/token";
const char SPOTIFY_REDIRECT_URI[] = "http%3A%2F%2F192.168.3.147%2Fspotify-auth";        // TODO: replace hardcoded IP
const char SPOTIFY_SCOPE[] = "user-read-playback-state+user-read-playback-position";
const char SPOTIFY_PLAYER_URL[] = "https://api.spotify.com/v1/me/player";
const char SPOTIFY_USER_URL[] = "https://api.spotify.com/v1/me";
const char SPOTIFY_FEATURES_URL[] = "https://api.spotify.com/v1/audio-features";

// Size (in bytes) to allocate to the DynamicJsonDocument for deserializing the Spotify response json.
// Commented values were empirically found to be the average document size for each response type, however,
// these are set to the same value to reduce heap fragmentation.
// See here for more details on ArduinoJson memory usage: https://arduinojson.org/v6/how-to/reduce-memory-usage/
#define SPOTIFY_TOKEN_JSON_SIZE 2000          // ~300 bytes
#define SPOTIFY_PLAYER_JSON_SIZE 2000         // 5000~6000 bytes ==> 2000 bytes of json memory after filtering
#define SPOTIFY_FEATURES_JSON_SIZE 2000       // ~600 bytes
#define SPOTIFY_REFRESH_TOKEN_JSON_SIZE 2000  // ~500 bytes
#define SPOTIFY_USER_JSON_SIZE 2000

class Spotify {
   public:
    // Constructor, takes a Spotify client (developer) ID, authorization string, and refresh token.
    // These parameters are typically stored in the ESP32's non-volatile Preferences and set via the
    // command-line or web setup of the Spotify account. See set_spotify_client_id() and set_spotify_account()
    // in CLI.cpp for the full workflow.
    Spotify(const char *client_id, const char *auth_b64, const char *refresh_token);

    // Struct for album art details
    struct album_art_t {
        bool loaded = false;
        bool changed = false;
        char url[CLI_MAX_CHARS] = {0};
        uint16_t width = 0;
        uint8_t *data = NULL;
        unsigned long num_bytes = 0;
    };

    // Struct for Spotify data shared with other tasks. This struct is designed to be
    // relatively small (32 bytes) as it is sent via the eventhandler to various tasks.
    struct public_data_t {
        bool is_active = false;
        double track_progress = 0.0;

        bool art_loaded = false;
        bool art_changed = false;
        uint16_t art_width = 0;
        uint8_t *art_data = NULL;
        unsigned long art_num_bytes = 0;
    };
    
    // Static methods for Spotify account setup
    
    // Given a client (developer) ID and an authorization string, generates a URL where a Spotify user can
    // provide permission to view their playback details. The auth_url string should be able to hold
    // at least HTTP_MAX_CHARS bytes.
    static bool request_user_auth(const char *client_id, const char *auth_b64, char *auth_url);

    // Given an authorization string and a user authorization code, generates a refresh token to be used
    // when querying the Spotify Web API. The refresh_token string should be able to hold at least CLI_MAX_CHARS
    // bytes.
    static bool get_refresh_token(const char *auth_b64, const char *auth_code, char *refresh_token);

    // Gets the latest Spotify playback data and populates both the private and public_data_t variables.
    void update();

    // Prints the details of current Spotify playback to serial.
    void print_info();

    // Returns a value from 0 to 1.0 that indicates the progress in the current track.
    double get_track_progress();

    // Indicates if Spotify is currently running on the linked account.
    bool is_active();

    // Gets the user name for the currently linked account. user_name should hold at least CLI_MAX_CHARS bytes.
    void get_user_name(char *user_name);

    // Gets the url for the current album art. url should hold at least CLI_MAX_CHARS bytes.
    void get_art_url(char *url);

    // Gets the current artist name. artist_name should hold at least CLI_MAX_CHARS bytes.
    void get_artist_name(char *artist_name);

    // Gets the current album name. Album name should hold at least CLI_MAX_CHARS bytes.
    void get_album_name(char *album_name);

    // Returns a struct with Spotify playback data.
    public_data_t get_data();

   private:
    // Gets an authenticated token for use with the Spotify Web API
    // and updates the token variable. Returns true on
    // success and false otherwise.
    bool _get_token();

    // Gets the current user's profile via the Web API
    // and updates the user name variable. Returns true on
    // success and false otherwise.
    bool _get_user_profile();

    // Gets details on current Spotify playback via the Web API
    // and updates the assoicated private variables. Returns true on
    // success and false otherwise.
    bool _get_player();

    // Gets detailed Spotify track features via the Web API
    // and updates the assoicated private variables. Returns true on
    // success and false otherwise.
    bool _get_features();

    // Retrieves album art for the currently playing track.
    // and updates the assoicated private variables. Returns true on
    // success and false otherwise.
    bool _get_art();

    // Parses the json response from the Spotify Web API and updates
    // the associated member variables.
    void _parse_json(JsonDocument *json);

    // Resets member variables to default values.
    void _reset_variables();

    // Helper function to replace https with http when downloading album art from
    // Spotify servers. This is a workaround that prevents a crash in WiFiSecure libraries.
    void _replace_https_with_http(char *url);

    // Writes the http response to a specified file. Not currently used.
    static int _write_http_response_to_file(HTTPClient *http, const char *filename);

    // Populates the json document with contents of the specified file. Not currently used.
    static DeserializationError _deserialize_json_from_file(JsonDocument *json, const char *filename);

    // Populates the json document with contents of the specified string. Set use_filter to true
    // when using this method to deserialize the _get_player() response. This dramatically reduces
    // dynamic memory usage by only deserializing specific json fields we care about.
    static DeserializationError _deserialize_json_from_string(JsonDocument *json, String *response, bool use_filter = false);

    // Private variables
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