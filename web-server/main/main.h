#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum
{
  WIFI_APP_MSG_START_HTTP_SERVER = 0,
  WIFI_APP_MSG_STA_CONNECTED_GOT_IP,
  WIFI_APP_MSG_STA_CONNECTED,
  WIFI_APP_MSG_STA_DISCONNECTED,
  WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER,
  APP_MSG_SENSOR_DATA_RECEIVED,
} app_event_id_t;

typedef struct
{
  app_event_id_t event_id;
} app_event_t;

// Queue handle used to manipulate the main queue of events.
extern QueueHandle_t app_queue_handle;

BaseType_t app_send_message(app_event_id_t eventID);

#endif // MAIN_H
