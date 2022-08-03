#ifndef EVENTHANDLER_H_
#define EVENTHANDLER_H_

#include "ButtonFSM.h"
#include "Mode.h"
#include "Spotify.h"

enum EventType : uint32_t {
    EVENT_NONE = 0x00,
    EVENT_SERVO_POS_CHANGED = 1 << 0,
    EVENT_AUDIO_FRAME_DONE = 1 << 1,
    EVENT_SPOTIFY_UPDATED = 1 << 2,
    EVENT_MODE_CHANGED = 1 << 3,
    EVENT_BUTTON_PRESSED = 1 << 4,
    EVENT_ALL = 0xFFFF,
};

struct event_t {
    EventType event_type;

    union {
        uint8_t servo_pos;
        Spotify::public_data_t sp_data;
        Modes_t modes;
        ButtonFSM::button_fsm_state_t button_state;
    };
};

struct task_associations_t {
    TaskHandle_t t;
    QueueHandle_t q;
    uint32_t subscribed_events;
};

class EventHandler {
   public:
    EventHandler(){};
    EventHandler(QueueHandle_t q_events);
    void emit(event_t e);                                                                    // send an event to the event queue
    void subscribe(TaskHandle_t t, EventType et);                                            // subscribe a task to an event type
    void unsubscribe(TaskHandle_t t, EventType et);                                          // unsubscribe a task from an event type
    bool is_subscribed(TaskHandle_t t, EventType et);                                        // register a task with the event handler
    void register_task(TaskHandle_t t, QueueHandle_t q, uint32_t event_types = EVENT_NONE);  // process an event and pass it to the appropriate tasks
    void process(event_t e);

   private:
    QueueHandle_t _q_events;
    int _num_tasks;
    task_associations_t _task_associations[MAX_EVENTHANDLER_TASKS];
};

#endif  // EVENTHANDLER_H_