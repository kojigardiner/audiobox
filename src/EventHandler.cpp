#include "EventHandler.h"

#include "Utils.h"

EventHandler::EventHandler(QueueHandle_t q_events) {
    _num_tasks = 0;
    _q_events = q_events;
}

void EventHandler::register_task(TaskHandle_t *t, QueueHandle_t q, uint32_t event_types) {
    if (_num_tasks < MAX_EVENTHANDLER_TASKS) {
        _task_associations[_num_tasks] = {.t = t,
                                          .q = q,
                                          .subscribed_events = event_types};
        _num_tasks++;
        print("Task %d registered with subscription to %d\n", t, event_types);
    } else {
        print("WARNING: register_task failed, too many tasks!\n");
    }
}

void EventHandler::emit(event_t e) {
    if (xQueueSend(_q_events, &e, 0) != pdTRUE) {
        print("WARNING: emit failed to enqueue event!\n");
    }
}

void EventHandler::subscribe(TaskHandle_t *t, EventType et) {
    for (int i = 0; i < _num_tasks; i++) {
        if (_task_associations[i].t == t) {
            _task_associations[i].subscribed_events |= et;
            return;
        }
    }
    print("WARNING: subscribe failed, task not in registry!\n");
}

void EventHandler::unsubscribe(TaskHandle_t *t, EventType et) {
    for (int i = 0; i < _num_tasks; i++) {
        if (_task_associations[i].t == t) {
            _task_associations[i].subscribed_events &= ~et;
            return;
        }
    }
    print("WARNING: unsubscribe failed, task not in registry!\n");
}

bool EventHandler::is_subscribed(TaskHandle_t *t, EventType et) {
    for (int i = 0; i < _num_tasks; i++) {
        if (_task_associations[i].t == t) {
            return (_task_associations[i].subscribed_events & et);
        }
    }
    print("WARNING: is_subscribed failed, task not in registry!\n");
    return false;
}

void EventHandler::process(event_t e) {
    for (int i = 0; i < _num_tasks; i++) {
        if (_task_associations[i].subscribed_events & e.event_type) {
            if (xQueueSend(_task_associations[i].q, &e, 0) != pdTRUE) {
                print("WARNING: process event failed, task queue is full!\n");
            }
        }
    }
}
