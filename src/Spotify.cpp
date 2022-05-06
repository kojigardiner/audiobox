#include "Spotify.h"

// Default constructor
Spotify::Spotify() {
    // don't run _get_token here, because we may not be connected to the network yet
    _reset_variables();
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

    if (track_changed) {
        if (!_get_features()) {
            _reset_variables();
        } else {
            print();
        }
    }
}

// Prints variables related to current playing track
void Spotify::print() {
    Serial.print("\tTitle: ");
    Serial.println(track_title);
    Serial.print("\tArtist: ");
    Serial.println(artists);
    Serial.print("\tAlbum: ");
    Serial.println(album);
    Serial.print("\tAlbum art: ");
    Serial.println(art_url);
    Serial.print("\tAlbum art width: ");
    Serial.println(art_width);
    Serial.print("\tDuration (ms): ");
    Serial.println(duration_ms);
    Serial.print("\tProgress (ms): ");
    Serial.println(progress_ms);
    Serial.print("\tPlaying: ");
    Serial.println(is_playing);
    Serial.print("\tDevice: ");
    Serial.println(device);
    Serial.print("\tType: ");
    Serial.println(device_type);
    Serial.print("\tActive: ");
    Serial.println(is_active);
    Serial.print("\tVolume: ");
    Serial.println(volume);
    Serial.print("\tTempo: ");
    Serial.println(tempo);
    Serial.print("\tEnergy: ");
    Serial.println(energy);
}

// Gets token for use with Spotify Web API
bool Spotify::_get_token() {
    bool ret;

    Serial.println("Getting Spotify token...");

    HTTPClient http;
    http.begin(SPOTIFY_GET_TOKEN_URL);
    http.addHeader("Cookie", SPOTIFY_COOKIE);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_FOUND) {
        String payload = http.getLocation();
        _token = payload.substring(payload.indexOf("access_token=") + 13, payload.indexOf("&token_type"));
        Serial.print("Token: ");
        Serial.println(_token);
        ret = true;
    } else {
        Serial.println(String(httpCode) + ": Error retrieving token");
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
    http.addHeader("Authorization", "Bearer " + _token);

    int httpCode = http.GET();
    String payload = http.getString();
    http.end();

    switch (httpCode) {  // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
        case HTTP_CODE_OK: {
            DynamicJsonDocument json(20000);
            if (deserializeJson(json, payload) != DeserializationError::Ok) {
                Serial.println("json deserialization error");
                ret = false;
            } else {
                _parse_json(&json);
                ret = true;
            }

            _token_expired = false;
            break;
        }
        case HTTP_CODE_NO_CONTENT:
            Serial.println(String(httpCode) + ": Playback not available/active");
            _token_expired = false;
            ret = false;
            break;
        case HTTP_CODE_BAD_REQUEST:
        case HTTP_CODE_UNAUTHORIZED:
        case HTTP_CODE_FORBIDDEN:
            Serial.println(String(httpCode) + ": Bad/expired token or OAuth request");
            _token_expired = true;
            ret = false;
            break;
        case HTTP_CODE_TOO_MANY_REQUESTS:
            Serial.println(String(httpCode) + ": Exceeded rate limits");
            _token_expired = false;
            ret = false;
            break;
        default:
            Serial.println(String(httpCode) + ": Unrecognized error");
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

    http.begin(SPOTIFY_FEATURES_URL + "/" + track_id);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json");
    http.addHeader("Authorization", "Bearer " + _token);

    int httpCode = http.GET();
    String payload = http.getString();
    http.end();

    switch (httpCode) {  // see here: https://developer.spotify.com/documentation/web-api/reference/#/operations/get-information-about-the-users-current-playback
        case HTTP_CODE_OK: {
            DynamicJsonDocument json(20000);
            if (deserializeJson(json, payload) != DeserializationError::Ok) {
                Serial.println("json deserialization error");
                ret = false;
            } else {
                tempo = json["tempo"].as<float>();
                energy = json["energy"].as<float>();
                ret = true;
            }

            _token_expired = false;
            break;
        }
        case HTTP_CODE_NO_CONTENT:
            Serial.println(String(httpCode) + ": Playback not available/active");
            is_active = false;
            is_playing = false;
            _token_expired = false;
            ret = false;
            break;
        case HTTP_CODE_BAD_REQUEST:
        case HTTP_CODE_UNAUTHORIZED:
        case HTTP_CODE_FORBIDDEN:
            Serial.println(String(httpCode) + ": Bad/expired token or OAuth request");
            _token_expired = true;
            ret = false;
            break;
        case HTTP_CODE_TOO_MANY_REQUESTS:
            Serial.println(String(httpCode) + ": Exceeded rate limits");
            _token_expired = false;
            ret = false;
            break;
        default:
            Serial.println(String(httpCode) + ": Unrecognized error");
            _token_expired = true;
            ret = false;
            break;
    }

    return ret;
}

// Parses json response from the Spotify Web API and stores results in member variables
void Spotify::_parse_json(DynamicJsonDocument *json) {
    String parsed_track_id = (*json)["item"]["id"].as<String>();

    if (track_id != parsed_track_id) {  // if the track has changed
        track_changed = true;
        track_id = parsed_track_id;
        track_title = (*json)["item"]["name"].as<String>();
        album = (*json)["item"]["album"]["name"].as<String>();

        JsonArray arr = (*json)["item"]["artists"].as<JsonArray>();
        artists = "";
        for (JsonVariant value : arr) {
            artists += value["name"].as<String>() + " ";
        }
        is_active = (*json)["device"]["is_active"].as<bool>();
        device_type = (*json)["device"]["type"].as<String>();
        progress_ms = (*json)["progress_ms"].as<int>();
        duration_ms = (*json)["item"]["duration_ms"].as<int>();
        is_playing = (*json)["is_playing"].as<bool>();
        device = (*json)["device"]["name"].as<String>();
        volume = (*json)["device"]["volume_percent"].as<int>();

        int art_url_count = (*json)["item"]["album"]["images"].size();                                      // count number of album art URLs
        String parsed_art_url = (*json)["item"]["album"]["images"][art_url_count - 1]["url"].as<String>();  // the last one is the smallest, typically 64x64
        parsed_art_url.replace("https", "http");                                                            // needed to eliminate SSL handshake errors

        if (art_url != parsed_art_url) {  // if the art url has changed
            art_url = parsed_art_url;
            art_changed = true;

            art_width = (*json)["item"]["album"]["images"][art_url_count - 1]["width"].as<int>();
            is_art_loaded = false;

            if (art_url == NULL) {
                track_id = "";  // force next loop to check json again
            } else {
                _get_art();
            }
        } else {
            art_changed = false;
        }
    } else {  // if it's the same track, just show how far we've advanced
        track_changed = false;
        art_changed = false;
        progress_ms = (*json)["progress_ms"].as<int>();
        duration_ms = (*json)["item"]["duration_ms"].as<int>();
    }
}

// Downloads album cover art
bool Spotify::_get_art() {
    bool ret;
    int start_ms = millis();
    Serial.println("Downloading " + art_url);

    HTTPClient http;
    http.begin(art_url);

    // Start connection and send HTTP header
    int httpCode = http.GET();

    // File found at server
    if (httpCode == HTTP_CODE_OK) {
        // Get length of document (is -1 when Server sends no Content-Length header)
        int total = http.getSize();

        art_num_bytes = total;

        // Allocate memory in heap for the downloaded data
        if (art_data != NULL) {
            Serial.println("Free heap prior to free: " + String(ESP.getFreeHeap()));
            free(art_data);  // if not NULL, we have previously allocated memory and should free it
        }
        Serial.println("Free heap prior to malloc: " + String(ESP.getFreeHeap()));
        art_data = (uint8_t *)malloc(art_num_bytes * sizeof(*(art_data)));  // dereference the art_data pointer to get the size of each element (uint8_t)

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
                memcpy(&(art_data)[art_num_bytes - len], buff, c);

                // Calculate remaining bytes
                if (len > 0) {
                    len -= c;
                }
            }
            yield();
        }
        // Serial.println();
        // Serial.print("[HTTP] connection closed or file end.\n");
        is_art_loaded = true;
        Serial.println(String(millis() - start_ms) + "ms to download art");
        ret = true;
    }
    // f.close();
    else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        is_art_loaded = false;
        ret = false;
    }
    http.end();

    return ret;
}

void Spotify::_reset_variables() {
    progress_ms = 0;
    duration_ms = 0;
    volume = 0;
    track_id = "";
    artists = "";
    track_title = "";
    album = "";
    device = "";
    device_type = "";
    art_url = "";
    art_width = 0;
    is_active = false;
    is_playing = false;
    if (art_data != NULL) {
        free(art_data);
    }
    art_data = NULL;
    art_num_bytes = 0;
    is_art_loaded = false;
    tempo = 0.0;
    energy = 0.0;
    track_changed = false;
    art_changed = false;
}