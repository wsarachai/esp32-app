#include "sensor_cache.h"

#include "dht22.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "task_settings.h"

#define SENSOR_POLL_INTERVAL_MS 2000

static const char *TAG = "sensor_cache";

static StaticSemaphore_t s_snapshot_mutex_buffer;
static SemaphoreHandle_t s_snapshot_mutex = NULL;
static sensor_snapshot_t s_snapshot = {
    .humidity = -1.0f,
    .temperature = -1000.0f,
    .valid = false,
    .timestamp_us = 0,
};
static bool s_started = false;

static void sensor_cache_task(void *pvParameters)
{
    (void)pvParameters;

    while (1)
    {
        float humidity = 0.0f;
        float temperature = 0.0f;
        bool ok = dht22_read(&humidity, &temperature);

        if (ok)
        {
            if (xSemaphoreTake(s_snapshot_mutex, portMAX_DELAY) == pdTRUE)
            {
                s_snapshot.humidity = humidity;
                s_snapshot.temperature = temperature;
                s_snapshot.valid = true;
                s_snapshot.timestamp_us = esp_timer_get_time();
                xSemaphoreGive(s_snapshot_mutex);
            }
        }
        else
        {
            ESP_LOGW(TAG, "DHT22 read failed; keeping previous cached reading");
        }

        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
    }
}

esp_err_t sensor_cache_start(void)
{
    if (s_started)
    {
        return ESP_OK;
    }

    s_snapshot_mutex = xSemaphoreCreateMutexStatic(&s_snapshot_mutex_buffer);
    if (s_snapshot_mutex == NULL)
    {
        return ESP_FAIL;
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        sensor_cache_task,
        "sensor_cache_task",
        SENSOR_TASK_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        NULL,
        SENSOR_TASK_CORE_ID);

    if (created != pdPASS)
    {
        return ESP_FAIL;
    }

    s_started = true;
    ESP_LOGI(TAG, "Sensor cache task started");
    return ESP_OK;
}

bool sensor_cache_get_snapshot(sensor_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_snapshot_mutex == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(s_snapshot_mutex, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return false;
    }

    *snapshot = s_snapshot;
    xSemaphoreGive(s_snapshot_mutex);
    return snapshot->valid;
}
