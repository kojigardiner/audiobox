#ifndef TASK_H_
#define TASK_H_

#include <Arduino.h>

#include "Event.h"

struct task_qs_t {
    QueueHandle_t q_to_handler;
    QueueHandle_t q_from_handler;
};

class Task {
   public:
    Task(TaskFunction_t pvTaskCode,
         const char* const pcName,
         const uint32_t usStackDepth,
         void* const pvParameters,
         UBaseType_t uxPriority,
         TaskHandle_t* const pvCreatedTask,
         const BaseType_t xCoreID,
         QueueHandle_t q_to_handler);

    void subscribe(EventType et);      // subscribe to an event type
    bool is_subscribed(EventType et);  // check if an event type has been subscribed
    BaseType_t send(event_t ev);       // send an event

   private:
    TaskHandle_t _task;
    uint32_t _subscribed_events;
    QueueHandle_t _q_from_handler;
};

#endif  // TASK_H