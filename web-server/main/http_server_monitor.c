#include "http_server_monitor.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "http_server.h"
#include "task_settings.h"

static const char TAG[] = "http_server_mon";
static TaskHandle_t s_http_monitor_task_handle = NULL;
QueueHandle_t http_server_monitor_queue_handle = NULL;

#define HTTP_SERVER_MONITOR_PERIOD_MS 5000
#define HTTP_SERVER_MONITOR_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define HTTP_SERVER_MONITOR_PRIORITY TASK_PRIORITY_DEFAULT
#define HTTP_SERVER_MONITOR_CORE_ID TASK_CORE_ID_DEFAULT

static void http_server_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        http_server_queue_message_t msg;
        BaseType_t has_msg = xQueueReceive(
            http_server_monitor_queue_handle,
            &msg,
            pdMS_TO_TICKS(HTTP_SERVER_MONITOR_PERIOD_MS));

        bool should_restart = false;
        if (has_msg == pdTRUE)
        {
            switch (msg.msg_id)
            {
            case HTTP_SERVER_MONITOR_MSG_RESTART:
                should_restart = true;
                break;
            case HTTP_SERVER_MONITOR_MSG_CHECK_NOW:
            default:
                break;
            }
        }

        if (should_restart || !http_server_is_running())
        {
            ESP_LOGW(TAG, "HTTP server not running, attempting restart");
            esp_err_t restart_err = http_server_start();
            if (restart_err != ESP_OK)
            {
                ESP_LOGE(TAG, "HTTP server restart failed: %s", esp_err_to_name(restart_err));
            }
            else
            {
                ESP_LOGI(TAG, "HTTP server restarted by monitor task");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(HTTP_SERVER_MONITOR_PERIOD_MS));
    }
}

esp_err_t http_server_monitor_start(void)
{
    if (s_http_monitor_task_handle != NULL)
    {
        return ESP_OK;
    }

    if (http_server_monitor_queue_handle == NULL)
    {
        http_server_monitor_queue_handle = xQueueCreate(3, sizeof(http_server_queue_message_t));
        if (http_server_monitor_queue_handle == NULL)
        {
            ESP_LOGE(TAG, "Failed to create HTTP monitor queue");
            return ESP_FAIL;
        }
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        http_server_monitor_task,
        "http_server_mon",
        HTTP_SERVER_MONITOR_STACK_SIZE,
        NULL,
        HTTP_SERVER_MONITOR_PRIORITY,
        &s_http_monitor_task_handle,
        HTTP_SERVER_MONITOR_CORE_ID);

    if (created != pdPASS)
    {
        s_http_monitor_task_handle = NULL;
        ESP_LOGE(TAG, "Failed to create HTTP server monitor task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HTTP server monitor task started");
    return ESP_OK;
}
