#include "sensor_cache.h"

#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "task_settings.h"

#define SENSOR_POLL_INTERVAL_MS      2000
#define SENSOR_DEFAULT_HUMIDITY      0.0f
#define SENSOR_DEFAULT_TEMPERATURE   0.0f
#define SENSOR_DEFAULT_SOIL_MOISTURE 50.0f
#define SENSOR_DEVICE_ID_MAX_LEN     32

static const char *TAG = "sensor_cache";

typedef struct
{
    char    device_id[SENSOR_DEVICE_ID_MAX_LEN];
    float   temperature;
    float   humidity;
    float   soilMoisture;
    bool    valid;
    int64_t timestamp_us;
} device_entry_t;

static StaticSemaphore_t s_mutex_buffer;
static SemaphoreHandle_t s_mutex       = NULL;
static device_entry_t    s_device_table[SENSOR_CACHE_MAX_DEVICES];
static int               s_device_count = 0;
static bool              s_started      = false;

/* ------------------------------------------------------------------ */

static void sensor_cache_task(void *pvParameters)
{
    (void)pvParameters;
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
    }
}

/* ------------------------------------------------------------------ */

esp_err_t sensor_cache_start(void)
{
    if (s_started)
    {
        return ESP_OK;
    }

    memset(s_device_table, 0, sizeof(s_device_table));
    s_device_count = 0;

    s_mutex = xSemaphoreCreateMutexStatic(&s_mutex_buffer);
    if (s_mutex == NULL)
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
    ESP_LOGI(TAG, "Sensor cache started (max %d devices)", SENSOR_CACHE_MAX_DEVICES);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */

bool sensor_cache_get_snapshot(sensor_snapshot_t *snapshot)
{
    if (snapshot == NULL || s_mutex == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return false;
    }

    /* No devices registered yet – return safe defaults */
    if (s_device_count == 0)
    {
        snapshot->temperature  = SENSOR_DEFAULT_TEMPERATURE;
        snapshot->humidity     = SENSOR_DEFAULT_HUMIDITY;
        snapshot->soilMoisture = SENSOR_DEFAULT_SOIL_MOISTURE;
        snapshot->valid        = true;
        snapshot->timestamp_us = esp_timer_get_time();
        xSemaphoreGive(s_mutex);
        return true;
    }

    /* Average all valid device entries */
    float   sum_temp  = 0.0f;
    float   sum_hum   = 0.0f;
    float   sum_soil  = 0.0f;
    int     count     = 0;
    int64_t latest_ts = 0;

    for (int i = 0; i < SENSOR_CACHE_MAX_DEVICES; i++)
    {
        if (!s_device_table[i].valid)
        {
            continue;
        }
        sum_temp += s_device_table[i].temperature;
        sum_hum  += s_device_table[i].humidity;
        sum_soil += s_device_table[i].soilMoisture;
        count++;
        if (s_device_table[i].timestamp_us > latest_ts)
        {
            latest_ts = s_device_table[i].timestamp_us;
        }
    }

    if (count == 0)
    {
        snapshot->valid = false;
        xSemaphoreGive(s_mutex);
        return false;
    }

    snapshot->temperature  = sum_temp / (float)count;
    snapshot->humidity     = sum_hum  / (float)count;
    snapshot->soilMoisture = sum_soil / (float)count;
    snapshot->valid        = true;
    snapshot->timestamp_us = latest_ts;

    xSemaphoreGive(s_mutex);
    return true;
}

bool sensor_cache_get_stats(sensor_cache_stats_t *stats, uint32_t offline_timeout_ms)
{
    if (stats == NULL || s_mutex == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        return false;
    }

    int64_t now_us = esp_timer_get_time();
    int64_t offline_timeout_us = ((int64_t)offline_timeout_ms) * 1000;
    uint8_t registered = 0;
    uint8_t online = 0;
    int64_t newest_ts = 0;

    for (int i = 0; i < SENSOR_CACHE_MAX_DEVICES; i++)
    {
        if (!s_device_table[i].valid)
        {
            continue;
        }

        registered++;
        if (s_device_table[i].timestamp_us > newest_ts)
        {
            newest_ts = s_device_table[i].timestamp_us;
        }

        if (offline_timeout_us <= 0 || (now_us - s_device_table[i].timestamp_us) <= offline_timeout_us)
        {
            online++;
        }
    }

    stats->registered_devices = registered;
    stats->online_devices = online;
    stats->newest_timestamp_us = newest_ts;

    xSemaphoreGive(s_mutex);
    return true;
}

/* ------------------------------------------------------------------ */

esp_err_t sensor_cache_update_snapshot(const char *device_id,
                                       float temperature,
                                       float humidity,
                                       float soil_moisture)
{
    if (s_mutex == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }
    if (device_id == NULL || device_id[0] == '\0')
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    /* 1. Look for an existing entry for this device_id */
    int slot = -1;
    for (int i = 0; i < SENSOR_CACHE_MAX_DEVICES; i++)
    {
        if (s_device_table[i].valid &&
            strncmp(s_device_table[i].device_id, device_id, SENSOR_DEVICE_ID_MAX_LEN - 1) == 0)
        {
            slot = i;
            break;
        }
    }

    /* 2. No existing entry – find a free slot */
    if (slot == -1)
    {
        for (int i = 0; i < SENSOR_CACHE_MAX_DEVICES; i++)
        {
            if (!s_device_table[i].valid)
            {
                slot = i;
                break;
            }
        }
    }

    /* 3. Table full – evict the oldest entry */
    if (slot == -1)
    {
        int     oldest    = 0;
        int64_t oldest_ts = s_device_table[0].timestamp_us;
        for (int i = 1; i < SENSOR_CACHE_MAX_DEVICES; i++)
        {
            if (s_device_table[i].timestamp_us < oldest_ts)
            {
                oldest_ts = s_device_table[i].timestamp_us;
                oldest    = i;
            }
        }
        ESP_LOGW(TAG, "Device table full – evicting '%s' for '%s'",
                 s_device_table[oldest].device_id, device_id);
        slot = oldest;
        /* s_device_count stays at SENSOR_CACHE_MAX_DEVICES */
    }

    bool is_new = !s_device_table[slot].valid;

    strncpy(s_device_table[slot].device_id, device_id, SENSOR_DEVICE_ID_MAX_LEN - 1);
    s_device_table[slot].device_id[SENSOR_DEVICE_ID_MAX_LEN - 1] = '\0';
    s_device_table[slot].temperature  = temperature;
    s_device_table[slot].humidity     = humidity;
    s_device_table[slot].soilMoisture = soil_moisture;
    s_device_table[slot].valid        = true;
    s_device_table[slot].timestamp_us = esp_timer_get_time();

    if (is_new)
    {
        s_device_count++;
        ESP_LOGI(TAG, "New device registered: '%s' (total %d/%d)",
                 device_id, s_device_count, SENSOR_CACHE_MAX_DEVICES);
    }

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}
