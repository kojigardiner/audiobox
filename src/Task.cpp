#include "Task.h"

Task::Task(TaskFunction_t pvTaskCode,
           const char* const pcName,
           const uint32_t usStackDepth,
           void* const pvParameters,
           UBaseType_t uxPriority,
           TaskHandle_t* const pvCreatedTask,
           const BaseType_t xCoreID,
           QueueHandle_t q_to_handler) {
    _task = pvCreatedTask;
    _subscribed_events = EventType::NO_EVENT;
    _q_from_handler = xQueueCreate(1, sizeof(event_t));

    task_qs_t task_qs = {.q_to_handler = q_to_handler,
                         .q_from_handler = _q_from_handler};

    xTaskCreatePinnedToCore(
        pvTaskCode,     // Function to implement the task
        pcName,         // Name of the task
        usStackDepth,   // Stack size in words
        &task_qs,       // Task input parameter
        uxPriority,     // Priority of the task (don't use 0!)
        pvCreatedTask,  // Task handle
        xCoreID         // Pinned core
    );
}

void Task::subscribe(EventType et) {
    _subscribed_events |= et;
}

bool Task::is_subscribed(EventType et) {
    return (_subscribed_events & et);
}

BaseType_t Task::send(event_t ev) {
    return xQueueSend(_q_from_handler, &ev, 0);
}
