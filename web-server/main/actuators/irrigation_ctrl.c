#include "irrigation_ctrl.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "relay.h"
#include "sensor_cache.h"
#include "task_settings.h"
#include "water_config.h"

// How often the controller evaluates the soil moisture reading.
#define IRRIGATION_CHECK_INTERVAL_MS 10000

static const char *TAG = "irrigation_ctrl";
static bool s_started = false;

static void irrigation_ctrl_task(void *pvParameters)
{
    (void)pvParameters;

    int64_t relay_on_since_us = 0;

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(IRRIGATION_CHECK_INTERVAL_MS));

        // ── Highest priority: manual override ──────────────────────────────
        // While the user is controlling the relay from the web page, skip all
        // automatic decisions and reset duration tracking so the timer starts
        // fresh when auto mode resumes.
        if (relay_is_manual_override())
        {
            relay_on_since_us = 0;
            continue;
        }

        // ── Read sensor data ───────────────────────────────────────────────
        sensor_snapshot_t snapshot;
        if (!sensor_cache_get_snapshot(&snapshot) || !snapshot.valid)
        {
            ESP_LOGW(TAG, "Sensor snapshot not ready, skipping check");
            continue;
        }

        water_config_t cfg = water_config_get();
        float soil = snapshot.soilMoisture;
        bool is_on = relay_get_state();

        // ── Safety: maximum watering duration ─────────────────────────────
        if (is_on)
        {
            if (relay_on_since_us == 0)
            {
                // Relay was turned on by us in a previous cycle; record the time.
                relay_on_since_us = esp_timer_get_time();
            }
            else if (cfg.duration_minutes > 0)
            {
                int64_t elapsed_min = (esp_timer_get_time() - relay_on_since_us) / 60000000LL;
                if (elapsed_min >= (int64_t)cfg.duration_minutes)
                {
                    ESP_LOGW(TAG, "Max duration %u min reached — forcing relay OFF", cfg.duration_minutes);
                    relay_set(false);
                    relay_on_since_us = 0;
                    continue;
                }
            }
        }
        else
        {
            relay_on_since_us = 0;
        }

        // ── Moisture-based switching ───────────────────────────────────────
        // Turn ON when soil is too dry (and a real minimum threshold is set).
        if (!is_on && cfg.min_moisture_level > 0 && soil < (float)cfg.min_moisture_level)
        {
            ESP_LOGI(TAG, "Soil %.1f%% < min %u%% — turning relay ON", soil, cfg.min_moisture_level);
            relay_set(true);
            relay_on_since_us = esp_timer_get_time();
        }
        // Turn OFF once soil is saturated enough (and a real max threshold is set).
        else if (is_on && cfg.max_moisture_level > 0 && soil >= (float)cfg.max_moisture_level)
        {
            ESP_LOGI(TAG, "Soil %.1f%% >= max %u%% — turning relay OFF", soil, cfg.max_moisture_level);
            relay_set(false);
            relay_on_since_us = 0;
        }
    }
}

esp_err_t irrigation_ctrl_start(void)
{
    if (s_started)
    {
        return ESP_OK;
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        irrigation_ctrl_task,
        "irrigation_task",
        IRRIGATION_TASK_STACK_SIZE,
        NULL,
        IRRIGATION_TASK_PRIORITY,
        NULL,
        IRRIGATION_TASK_CORE_ID);

    if (created != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create irrigation task");
        return ESP_FAIL;
    }

    s_started = true;
    ESP_LOGI(TAG, "Irrigation control task started");
    return ESP_OK;
}
