#include "sensor_task.h"

#include "bt_sensor_client.h"
#include "dht22.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soil_moisture_adc.h"
#include "task_settings.h"

#define SENSOR_POLL_INTERVAL_MS 2000

static const char *TAG = "sensor_task";

static bool s_started = false;

static void sensor_task_fn(void *pvParameters)
{
  (void)pvParameters;

  esp_err_t soil_err = soil_moisture_adc_init();
  if (soil_err != ESP_OK)
  {
    ESP_LOGE(TAG, "Soil moisture ADC init failed: %s", esp_err_to_name(soil_err));
  }

  int reading = 0;

  while (1)
  {
    reading++;

    float humidity = 0.0f;
    float temperature = 0.0f;
    float soil_moisture = 0.0f;

    bool dht_ok = dht22_read(&humidity, &temperature);
    esp_err_t soil_ok = soil_moisture_adc_read_percent(&soil_moisture);

    if (!dht_ok)
    {
      ESP_LOGW(TAG, "[%d] DHT22 read failed - skipping send", reading);
      vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
      continue;
    }

    if (soil_ok != ESP_OK)
    {
      ESP_LOGW(TAG, "[%d] Soil moisture read failed (%s) - using 0%%",
               reading, esp_err_to_name(soil_ok));
      soil_moisture = 0.0f;
    }

    ESP_LOGI(TAG, "[%d] temp=%.1f°C  humidity=%.1f%%  soil=%.1f%%",
             reading, temperature, humidity, soil_moisture);

    if (!bt_sensor_client_is_connected())
    {
      bt_sensor_client_maintain();
      ESP_LOGW(TAG, "[%d] No Bluetooth link - skipping send", reading);
    }
    else
    {
      esp_err_t send_err = bt_sensor_client_send(temperature, humidity, soil_moisture);
      if (send_err == ESP_OK)
      {
        ESP_LOGI(TAG, "[%d] Sensor data queued for Bluetooth send", reading);
      }
      else
      {
        ESP_LOGW(TAG, "[%d] Bluetooth send failed: %s",
                 reading, esp_err_to_name(send_err));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(SENSOR_POLL_INTERVAL_MS));
  }
}

esp_err_t sensor_task_start(void)
{
  if (s_started)
  {
    return ESP_OK;
  }

  BaseType_t created = xTaskCreatePinnedToCore(
      sensor_task_fn,
      "sensor_task",
      SENSOR_TASK_STACK_SIZE,
      NULL,
      SENSOR_TASK_PRIORITY,
      NULL,
      SENSOR_TASK_CORE_ID);

  if (created != pdPASS)
  {
    ESP_LOGE(TAG, "Failed to create sensor task");
    return ESP_FAIL;
  }

  s_started = true;
  ESP_LOGI(TAG, "Sensor task started");
  return ESP_OK;
}
