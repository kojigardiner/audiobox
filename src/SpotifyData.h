// Struct for Spotify data
#include <Arduino.h>

typedef struct SpotifyPlayerData {
    unsigned long progress_ms = 0;
    unsigned long duration_ms = 0;
    uint8_t volume = 0;
    String track_id = "";
    String artists = "";
    String track_title = "";
    String album = "";
    String device = "";
    String device_type = "";
    String art_url = "";
    uint16_t art_width = 0;
    bool is_active = false;
    bool is_playing = false;
    uint8_t *art_data = NULL;
    unsigned long art_num_bytes = 0;
    bool is_art_loaded = false;
    double tempo = 0.0;
    double energy = 0.0;
    bool track_changed = false;
} SpotifyPlayerData_t;
