#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "main.h"
#include "sensor_task.h"
#include "task_settings.h"
#include "wifi_sta.h"

static const char TAG[] = "main";

QueueHandle_t app_queue_handle;

BaseType_t app_send_message(app_event_id_t event_id)
{
    app_event_t msg = {.event_id = event_id};
    return xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
}

static void main_task(void *pvParameters)
{
    (void)pvParameters;

    app_queue_handle = xQueueCreate(10, sizeof(app_event_t));
    if (app_queue_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to create app queue");
        vTaskDelete(NULL);
        return;
    }

    wifi_sta_start();

    while (1)
    {
        app_event_t event;
        if (xQueueReceive(app_queue_handle, &event, pdMS_TO_TICKS(1000)) == pdPASS)
        {
            switch (event.event_id)
            {
            case APP_MSG_WIFI_CONNECTED_GOT_IP:
                ESP_LOGI(TAG, "APP_MSG_WIFI_CONNECTED_GOT_IP");
                sensor_task_start();
                break;

            case APP_MSG_WIFI_DISCONNECTED:
                ESP_LOGW(TAG, "APP_MSG_WIFI_DISCONNECTED");
                break;

            default:
                break;
            }
        }
    }
}

void app_main(void)
{
    // Suppress verbose per-pin config logs from the ESP-IDF GPIO driver.
    esp_log_level_set("gpio", ESP_LOG_WARN);

    xTaskCreatePinnedToCore(
        main_task,
        "main_task",
        MAIN_TASK_STACK_SIZE,
        NULL,
        MAIN_TASK_PRIORITY,
        NULL,
        MAIN_TASK_CORE_ID);
}
