#ifndef HTTP_SERVER_MONITOR_H_
#define HTTP_SERVER_MONITOR_H_

#include <stdint.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum
{
    HTTP_SERVER_MONITOR_MSG_CHECK_NOW = 0,
    HTTP_SERVER_MONITOR_MSG_RESTART,
    HTTP_MSG_WIFI_CONNECT_INIT,
} http_server_monitor_msg_id_t;

typedef struct
{
    http_server_monitor_msg_id_t msg_id;
} http_server_queue_message_t;

extern QueueHandle_t http_server_monitor_queue_handle;

esp_err_t http_server_monitor_start(void);

BaseType_t http_server_monitor_send_message(http_server_monitor_msg_id_t eventID);

#endif // HTTP_SERVER_MONITOR_H_
