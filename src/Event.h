// Events

#include "ModeSequence.h"
#include "Spotify.h"

enum EventType {
    NO_EVENT = 0x0,
    SERVO_POS_CHANGED = 0x1,
    AUDIO_FRAME_DONE = 0x2,
    SPOTIFY_UPDATED = 0x4,
    MODE_CHANGED = 0x8,
    ALL_EVENTS = 0xFF,
};

typedef struct event_t {
    EventType event_type;
    union {
        uint8_t servo_pos;
        Spotify::public_data_t sp_data;
        ModeSequence mode_seq;
    } data;
};
