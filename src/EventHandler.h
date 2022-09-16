#ifndef _EVENTHANDLER_H_
#define _EVENTHANDLER_H_

#include "ButtonFSM.h"
#include "Mode.h"
#include "ModeSequence.h"
#include "Spotify.h"

// The EventHandler class is intended to be instantiated once as a global object that handles
// message passing between FreeRTOS tasks. Tasks can register themselves with the EventHandler
// object, providing it with a queue handle as well as a bitfield defining the types of
// events for which that task wishes to be notified. Tasks can emit() events to the EventHandler
// object, which loops through all tasks and pass the event on to those which have
// subscribed to that event type.
//
// An event_t struct encapsulates the actual "event", and consists of an enum defining the type
// of event as well as a union field containing relevant data associated with that event.
// 
// All tasks that intend to be run should register for the EVENT_START event. After the 
// EventHandler is initialized, it sends the EVENT_START message to all registered tasks,
// which will wait to receive the event before starting their task loops. This ensures
// a task does not start before the EventHandler is initialized.

// Events that tasks may register for.
enum EventType : uint32_t {
    EVENT_NONE = 0x00,
    EVENT_START = 1 << 0,               // All tasks should register to this event and block on its receipt.
    EVENT_SERVO_POS_CHANGED = 1 << 1,   // Indicates that a task is requesting a new servo position
    EVENT_AUDIO_FRAME_DONE = 1 << 2,    // Indicates that an audio frame has finished FFT processing
    EVENT_SPOTIFY_UPDATED = 1 << 3,     // Indicates that the Spotify now-playing information has been updated
    EVENT_MODE_CHANGED = 1 << 4,        // Indicates that the current display mode (album art, audio vis, etc) has changed
    EVENT_BUTTON_PRESSED = 1 << 5,      // Indicates that the a button on the audiobox was pressed
    EVENT_POWER_OFF = 1 << 6,           // Indicates that the power switch was depressed to the off position
    EVENT_REBOOT = 1 << 7,              // Indicates that a reboot was requested
    EVENT_ALL = 0xFFFF,
};

// Encapsulates a button event
struct button_event_t {
    int id;                                 // pin number for the button
    ButtonFSM::button_fsm_state_t state;    // the current state of the button (MOMENTARY or HOLD)
};

// Encapsulates a generic event
struct event_t {
    EventType event_type;       // the type of event

    union {
        uint8_t servo_pos;              // the requested servo position
        Spotify::public_data_t sp_data; // the updated Spotify now-playing info
        curr_mode_t mode;               // the current mode
        button_event_t button_info;     // the button state info
    };
};

// Structure for keeping track of tasks that register with the EventHandler.
struct task_associations_t {
    TaskHandle_t *t;                // pointer to task
    QueueHandle_t q;                // queue to send messages to the task
    uint32_t subscribed_events;     // bitfield of subscribed events
};

class EventHandler {
   public:
    // Default constructor, used to declare global instance
    EventHandler(){};

    // Constructor, pass in a handle to a queue that will serve as the "receiver" for task-emitted events.
    EventHandler(QueueHandle_t q_events);

    // Sends an event to the event queue to be broadcast to all relevant tasks.
    void emit(event_t e);

    // Sends an event to the event queue to be broadcast to all relevant tasks. Use when sending from an ISR.
    void emit_from_isr(event_t e);

    // Subscribes a given task to a given event type.
    void subscribe(TaskHandle_t *t, EventType et);

    // Unsubscribes a given task to a given event type.
    void unsubscribe(TaskHandle_t *t, EventType et);

    // Checks if a given task is subscribed to a given event type.
    bool is_subscribed(TaskHandle_t *t, EventType et);

    // Registers a given task and queue with the event handler. Optionally pass a bitfield with the event types to subscribe to.
    void register_task(TaskHandle_t *t, QueueHandle_t q, uint32_t event_types = EVENT_NONE);

    // Process a given event by iterating through all registered tasks and sending the event to those registered
    // for the event type.
    void process(event_t e);

   private:
    QueueHandle_t _q_events;    // queue for receiving events from tasks
    int _num_tasks;             // total number of tasks registered
    task_associations_t _task_associations[MAX_EVENTHANDLER_TASKS];     // array of task associations
};

#endif  // _EVENTHANDLER_H_