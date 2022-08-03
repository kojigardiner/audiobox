#include "Spotify.h"

#include "Utils.h"

Spotify::Spotify(const char *client_id, const char *auth_b64, const char *refresh_token) {
    // don't run _get_token here, because we may not be connected to the network yet
    strncpy(_client_id, client_id, CLI_MAX_CHARS);
    strncpy(_auth_b64, auth_b64, CLI_MAX_CHARS);
    strncpy(_refresh_token, refresh_token, CLI_MAX_CHARS);

    _reset_variables();
}

bool Spotify::get_refresh_token(const char *auth_b64, char *refresh_token) {
    bool ret = false;
    HTTPClient http;

    // get the authorization code from the user
    char auth_code[CLI_MAX_CHARS];
    print("\nCopy and paste the characters after \"code: \" on the webpage. Do not include the quotation (\") marks: ");
    get_input(auth_code);

    http.begin(SPOTIFY_TOKEN_URL);

    char auth_header[HTTP_MAX_CHARS];
    snprintf(auth_header, HTTP_MAX_CHARS, "Basic %s", auth_b64);
    http.addHeader("Authorization", auth_header);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    char request_data[HTTP_MAX_CHARS];
    snprintf(request_data, HTTP_MAX_CHARS, "grant_type=authorization_code&code=%s&redirect_uri=%s", auth_code, SPOTIFY_REDIRECT_URI);

    int httpCode = http.POST(request_data);
    String response = http.getString();
    print("%s\n", response.c_str());
    http.end();

    switch (httpCode) {
        case HTTP_CODE_OK: {
            DynamicJsonDocument json(20000);
            if (deserializeJson(json, response) != DeserializationError::Ok) {
                print("json deserialization error\n");
                ret = false;
            } else {
                strncpy(refresh_token, (json)["refresh_token"].as<const char *>(), CLI_MAX_CHARS);
                ret = true;
            }
            break;
        }
        default: {
            print("%d:%s: Unrecognized error\n", httpCode, __func__);
            ret = false;
            break;
        }
    }
    return ret;
}

// CLI flow for requesting user to grant access to their account
// This should only be run once to link a user's account to the box, and will return a pointer to the refresh token
bool Spotify::request_user_auth(const char *client_id, const char *auth_b64, char *refresh_token) {
    bool ret;
    HTTPClient http;
    char url[HTTP_MAX_CHARS];

    snprintf(url, HTTP_MAX_CHARS,
             "%s"
             "?client_id=%s"
             "&response_type=code"
             "&redirect_uri=%s"
             "&scope=%s",
             SPOTIFY_AUTH_URL, client_id, SPOTIFY_REDIRECT_URI, SPOTIFY_SCOPE);

    http.begin(url);
    int httpCode = http.GET();

    String payload = http.getLocation();  // we are expecting an http redirect, so get the location
    http.end();

    // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
    switch (httpCode) {
        case HTTP_CODE_SEE_OTHER: {
            print("\nLaunch the following URL in your browser to authorize access: ");
            print("%s\n", payload.c_str());
            print("\nAfter granting access, you will be redirected to a webpage.\n");

            ret = get_refresh_token(auth_b64, refresh_token);

            break;
        }
        default:
            print("%d:%s: Unrecognized error\n", httpCode, __func__);
            ret = false;
            break;
    }
    return ret;
}

// Main function to be run in a loop, updates variables regarding state of playback
void Spotify::update() {
    if (_token_expired) {
        while (!_get_token()) {  // keep trying to get a token
            delay(1000);
        }
        _reset_variables();
    }

    if (!_get_player()) {
        _reset_variables();
    }

    if (_track_changed) {
        if (!_get_features()) {
            _reset_variables();
        } else {
            print_info();
        }
    }
}

// Returns a value from 0 to 1.0 indicating how far in the current track playback has progressed
double Spotify::get_track_progress() {
    return double(_progress_ms) / _duration_ms;
}

// Indicates if Spotify is current running on the linked account
bool Spotify::is_active() {
    return _is_active;
}

// Returns struct containing public data
Spotify::public_data_t Spotify::get_data() {
    _public_data.is_active = _is_active;
    _public_data.track_progress = get_track_progress();
    _public_data.art_changed = _album_art.changed;
    _public_data.art_data = _album_art.data;
    _public_data.art_loaded = _album_art.loaded;
    _public_data.art_num_bytes = _album_art.num_bytes;
    _public_data.art_width = _album_art.width;

    return _public_data;
}

// Prints variables related to current playing track
void Spotify::print_info() {
    print("\tTitle: %s\n", _track_title);
    print("\tArtist: %s\n", _artists);
    print("\tAlbum: %s\n", _album);
    print("\tAlbum art: %s\n", _album_art.url);
    print("\tAlbum art width: %d\n", _album_art.width);
    print("\tDuration (ms): %d\n", _duration_ms);
    print("\tProgress (ms): %d\n", _progress_ms);
    print("\tPlaying: ");
    _is_playing ? print("true\n") : print("false\n");
    print("\tDevice: %s\n", _device);
    print("\tType: %s\n", _device_type);
    print("\tActive: ");
    _is_active ? print("true\n") : print("false\n");
    print("\tVolume: %d\n", _volume);
    print("\tTempo: %f\n", _tempo);
    print("\tEnergy: %f\n", _energy);
}

// Gets token for use with Spotify Web API
bool Spotify::_get_token() {
    bool ret;

    print("Getting Spotify token...\n");

    HTTPClient http;
    http.begin(SPOTIFY_TOKEN_URL);
    char auth_header[HTTP_MAX_CHARS];
    snprintf(auth_header, HTTP_MAX_CHARS, "Basic %s", _auth_b64);
    http.addHeader("Authorization", auth_header);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    char request_data[HTTP_MAX_CHARS];
    snprintf(request_data, HTTP_MAX_CHARS, "grant_type=refresh_token&refresh_token=%s", _refresh_token);

    int httpCode = http.POST(request_data);

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        print("%s\n", payload.c_str());

        DynamicJsonDocument json(20000);
        if (deserializeJson(json, payload) != DeserializationError::Ok) {
            print("json deserialization error\n");
            ret = false;
        } else {
            strncpy(_token, (json)["access_token"].as<const char *>(), CLI_MAX_CHARS);
            ret = true;
        }
    } else {
        print("%d: Error retrieving token\n", httpCode);
        ret = false;
    }

    http.end();

    return ret;
}

// Gets the Spotify player data from the web API
bool Spotify::_get_player() {
    bool ret;
    HTTPClient http;

    http.begin(SPOTIFY_PLAYER_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    char bearer_header[HTTP_MAX_CHARS];
    snprintf(bearer_header, HTTP_MAX_CHARS, "Bearer %s", _token);
    http.addHeader("Authorization", bearer_header);

    int httpCode = http.GET();
    String payload = http.getString();
    // Serial.println(payload);

    http.end();

    // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
    switch (httpCode) {
        case HTTP_CODE_OK: {
            DynamicJsonDocument json(20000);
            if (deserializeJson(json, payload) != DeserializationError::Ok) {
                print("json deserialization error\n");
                ret = false;
            } else {
                _parse_json(&json);
                ret = true;
            }

            _token_expired = false;
            break;
        }
        case HTTP_CODE_NO_CONTENT:
            print("%d:%s: Playback not available/active\n", httpCode, __func__);
            _token_expired = false;
            _is_active = false;
            _is_playing = false;
            ret = false;
            break;
        case HTTP_CODE_BAD_REQUEST:
        case HTTP_CODE_UNAUTHORIZED:
        case HTTP_CODE_FORBIDDEN:
            print("%d:%s: Bad/expired token or OAuth request\n", httpCode, __func__);
            _token_expired = true;
            ret = false;
            break;
        case HTTP_CODE_TOO_MANY_REQUESTS:
            print("%d:%s: Exceeded rate limits\n", httpCode, __func__);
            _token_expired = false;
            ret = false;
            break;
        default:
            print("%d:%s: Unrecognized error\n", httpCode, __func__);
            _token_expired = true;
            ret = false;
            break;
    }

    return ret;
}

// Gets more detailed features of the currently playing track via the Spotify Web API
bool Spotify::_get_features() {
    bool ret;
    HTTPClient http;
    char features[HTTP_MAX_CHARS];

    snprintf(features, HTTP_MAX_CHARS, "%s/%s", SPOTIFY_FEATURES_URL, _track_id);
    http.begin(features);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    char bearer_header[HTTP_MAX_CHARS];
    snprintf(bearer_header, HTTP_MAX_CHARS, "Bearer %s", _token);
    http.addHeader("Authorization", bearer_header);

    int httpCode = http.GET();
    String payload = http.getString();
    http.end();

    switch (httpCode) {  // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
        case HTTP_CODE_OK: {
            DynamicJsonDocument json(20000);
            if (deserializeJson(json, payload) != DeserializationError::Ok) {
                print("json deserialization error\n");
                ret = false;
            } else {
                _tempo = json["tempo"].as<float>();
                _energy = json["energy"].as<float>();
                ret = true;
            }

            _token_expired = false;
            break;
        }
        case HTTP_CODE_NO_CONTENT:
            print("%d:%s: Playback not available/active\n", httpCode, __func__);
            _is_active = false;
            _is_playing = false;
            _token_expired = false;
            ret = false;
            break;
        case HTTP_CODE_BAD_REQUEST:
        case HTTP_CODE_UNAUTHORIZED:
        case HTTP_CODE_FORBIDDEN:
            print("%d:%s: Bad/expired token or OAuth request\n", httpCode, __func__);
            _token_expired = true;
            ret = false;
            break;
        case HTTP_CODE_TOO_MANY_REQUESTS:
            print("%d:%s: Exceeded rate limits\n", httpCode, __func__);
            _token_expired = false;
            ret = false;
            break;
        default:
            print("%d:%s: Unrecognized error\n", httpCode, __func__);
            _token_expired = true;
            ret = false;
            break;
    }

    return ret;
}

// Parses json response from the Spotify Web API and stores results in member variables
void Spotify::_parse_json(DynamicJsonDocument *json) {
    if ((*json)["item"]["id"].as<const char *>() == 0) {
        print("json track id is null!\n");
        return;  // in rare cases (e.g. on a playlist, Spotify will
                 // report is_playing but the track id will be null)
    }

    char parsed_track_id[CLI_MAX_CHARS];
    strncpy(parsed_track_id, (*json)["item"]["id"].as<const char *>(), CLI_MAX_CHARS);

    if (strncmp(_track_id, parsed_track_id, CLI_MAX_CHARS) != 0) {  // if the track has changed
        _track_changed = true;
        strncpy(_track_id, parsed_track_id, CLI_MAX_CHARS);
        strncpy(_track_title, (*json)["item"]["name"].as<const char *>(), CLI_MAX_CHARS);
        strncpy(_track_title, (*json)["item"]["name"].as<const char *>(), CLI_MAX_CHARS);
        strncpy(_album, (*json)["item"]["album"]["name"].as<const char *>(), CLI_MAX_CHARS);

        JsonArray arr = (*json)["item"]["artists"].as<JsonArray>();
        strncpy(_artists, "", CLI_MAX_CHARS);
        for (JsonVariant value : arr) {
            snprintf(_artists, CLI_MAX_CHARS, "%s%s ", _artists, value["name"].as<const char *>());
        }
        _is_active = (*json)["device"]["is_active"].as<bool>();
        strncpy(_device_type, (*json)["device"]["type"].as<const char *>(), CLI_MAX_CHARS);
        _progress_ms = (*json)["progress_ms"].as<int>();
        _duration_ms = (*json)["item"]["duration_ms"].as<int>();
        _is_playing = (*json)["is_playing"].as<bool>();
        strncpy(_device, (*json)["device"]["name"].as<const char *>(), CLI_MAX_CHARS);
        _volume = (*json)["device"]["volume_percent"].as<int>();

        int art_url_count = (*json)["item"]["album"]["images"].size();  // count number of album art URLs
        char parsed_art_url[CLI_MAX_CHARS];
        strncpy(parsed_art_url, (*json)["item"]["album"]["images"][art_url_count - 1]["url"].as<const char *>(), CLI_MAX_CHARS);  // the last one is the smallest, typically 64x64

        // replace https with http, needed to eliminate SSL handshake errors
        _replace_https_with_http(parsed_art_url);

        if (strncmp(_album_art.url, parsed_art_url, CLI_MAX_CHARS) != 0) {  // if the art url has changed
            strncpy(_album_art.url, parsed_art_url, CLI_MAX_CHARS);
            _album_art.changed = true;

            _album_art.width = (*json)["item"]["album"]["images"][art_url_count - 1]["width"].as<int>();
            _album_art.loaded = false;

            if (_album_art.url == NULL) {
                strncpy(_track_id, "", CLI_MAX_CHARS);  // force next loop to check json again
            } else {
                _get_art();
            }
        } else {
            _album_art.changed = false;
        }
    } else {  // if it's the same track, just show how far we've advanced
        _track_changed = false;
        _album_art.changed = false;
        _progress_ms = (*json)["progress_ms"].as<int>();
        _duration_ms = (*json)["item"]["duration_ms"].as<int>();
    }
}

// Downloads album cover art
bool Spotify::_get_art() {
    bool ret;
    int start_ms = millis();
    print("Downloading %s\n", _album_art.url);

    HTTPClient http;
    http.begin(_album_art.url);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        // Get length of document (is -1 when Server sends no Content-Length header)
        int total = http.getSize();

        _album_art.num_bytes = total;

        // Allocate memory in heap for the downloaded data
        if (_album_art.data != NULL) {
            print("Free heap prior to free: %d\n", ESP.getFreeHeap());
            free(_album_art.data);  // if not NULL, we have previously allocated memory and should free it
        }
        print("Free heap prior to malloc: %d\n", ESP.getFreeHeap());
        _album_art.data = (uint8_t *)malloc(_album_art.num_bytes * sizeof(*(_album_art.data)));  // dereference the art_data pointer to get the size of each element (uint8_t)

        if (_album_art.data == NULL) {
            print("Not enough heap for malloc\n");
            _album_art.loaded = false;
            ret = false;
        } else {
            int len = total;

            // Create buffer for read
            uint8_t buff[128] = {0};

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
                    memcpy(&(_album_art.data)[_album_art.num_bytes - len], buff, c);

                    // Calculate remaining bytes
                    if (len > 0) {
                        len -= c;
                    }
                }
                yield();
            }
            // Serial.println();
            // Serial.print("[HTTP] connection closed or file end.\n");
            _album_art.loaded = true;
            print("%dms to download art\n", millis() - start_ms);
            ret = true;
        }
    }
    // f.close();
    else {
        print("%d:%s: Unrecognized error\n", httpCode, __func__);
        _album_art.loaded = false;
        ret = false;
    }
    http.end();

    return ret;
}

void Spotify::_reset_variables() {
    _progress_ms = 0;
    _duration_ms = 0;
    _volume = 0;
    strncpy(_track_id, "", CLI_MAX_CHARS);
    strncpy(_artists, "", CLI_MAX_CHARS);
    strncpy(_track_title, "", CLI_MAX_CHARS);
    strncpy(_album, "", CLI_MAX_CHARS);
    strncpy(_device, "", CLI_MAX_CHARS);
    strncpy(_device_type, "", CLI_MAX_CHARS);
    _is_active = false;
    _is_playing = false;
    _tempo = 0.0;
    _energy = 0.0;
    _track_changed = false;

    strncpy(_album_art.url, "", CLI_MAX_CHARS);
    _album_art.width = 0;
    if (_album_art.data != NULL) {
        free(_album_art.data);
    }
    _album_art.data = NULL;
    _album_art.num_bytes = 0;
    _album_art.loaded = false;
    _album_art.changed = false;
}

void Spotify::_replace_https_with_http(char *url) {
    char *https = strstr(url, "https");  // get pointer to https
    if (https != NULL) {
        for (char *c = https + strlen("http"); *(c) != '\0'; c++) {
            *(c) = *(c + 1);
        }
    }
}