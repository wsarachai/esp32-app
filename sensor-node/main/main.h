#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum
{
    APP_MSG_BT_CONNECTED = 0,
    APP_MSG_BT_DISCONNECTED,
} app_event_id_t;

typedef struct
{
    app_event_id_t event_id;
} app_event_t;

// Queue handle used to post events to the main task.
extern QueueHandle_t app_queue_handle;

BaseType_t app_send_message(app_event_id_t event_id);

#endif // MAIN_H
