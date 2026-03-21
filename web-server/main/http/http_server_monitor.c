#include "http_server_monitor.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "http_server.h"
#include "sensor_cache.h"
#include "task_settings.h"

static const char TAG[] = "http_server_mon";
static TaskHandle_t s_http_monitor_task_handle = NULL;
QueueHandle_t http_server_monitor_queue_handle = NULL;

#define HTTP_SERVER_MONITOR_PERIOD_MS 5000
#define OTA_REBOOT_DELAY_MS 10000
#define SENSOR_NODE_OFFLINE_TIMEOUT_MS 15000
#define HTTP_SERVER_MONITOR_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define HTTP_SERVER_MONITOR_PRIORITY TASK_PRIORITY_DEFAULT
#define HTTP_SERVER_MONITOR_CORE_ID TASK_CORE_ID_DEFAULT

static volatile bool s_sensor_data_available = false;
static volatile uint8_t s_online_node_count = 0;
static volatile uint8_t s_registered_node_count = 0;

static void http_server_monitor_task(void *pvParameters)
{
    (void)pvParameters;

    bool ota_reboot_pending = false;
    TickType_t ota_reboot_deadline = 0;

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
            case HTTP_MSG_WIFI_CONNECT_INIT:
                // When WiFi connection is initiated, proactively check if the server is running and restart if not.
                should_restart = true;
                break;
            case HTTP_MSG_OTA_UPDATE_SUCCESSFUL:
                ota_reboot_pending = true;
                ota_reboot_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(OTA_REBOOT_DELAY_MS);
                ESP_LOGI(TAG, "OTA successful, reboot scheduled in %d ms", OTA_REBOOT_DELAY_MS);
                break;
            case HTTP_MSG_OTA_UPDATE_FAILED:
                ota_reboot_pending = false;
                ota_reboot_deadline = 0;
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

        sensor_cache_stats_t sensor_stats = {0};
        if (sensor_cache_get_stats(&sensor_stats, SENSOR_NODE_OFFLINE_TIMEOUT_MS))
        {
            s_registered_node_count = sensor_stats.registered_devices;
            s_online_node_count = sensor_stats.online_devices;

            bool has_available_data = (sensor_stats.registered_devices > 0) &&
                                      (sensor_stats.online_devices > 0);
            if (has_available_data != s_sensor_data_available)
            {
                s_sensor_data_available = has_available_data;
                if (s_sensor_data_available)
                {
                    ESP_LOGI(TAG, "Sensor data available again (%u/%u nodes online)",
                             (unsigned int)s_online_node_count,
                             (unsigned int)s_registered_node_count);
                }
                else
                {
                    ESP_LOGW(TAG, "All sensor nodes offline (%u tracked)",
                             (unsigned int)s_registered_node_count);
                }
            }
        }

        if (ota_reboot_pending && xTaskGetTickCount() >= ota_reboot_deadline)
        {
            ESP_LOGI(TAG, "Rebooting into updated firmware");
            vTaskDelay(pdMS_TO_TICKS(200));
            esp_restart();
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

BaseType_t http_server_monitor_send_message(http_server_monitor_msg_id_t eventID)
{
    if (http_server_monitor_queue_handle == NULL)
    {
        return pdFAIL;
    }
    http_server_queue_message_t msg = {.msg_id = eventID};
    return xQueueSend(http_server_monitor_queue_handle, &msg, pdMS_TO_TICKS(100));
}

bool http_server_monitor_is_sensor_data_available(void)
{
    return s_sensor_data_available;
}

uint8_t http_server_monitor_online_node_count(void)
{
    return s_online_node_count;
}

uint8_t http_server_monitor_registered_node_count(void)
{
    return s_registered_node_count;
}
