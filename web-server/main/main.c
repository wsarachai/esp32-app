#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_nvs.h"
#include "http_server.h"
#include "irrigation_ctrl.h"
#include "rgb-led.h"
#include "relay.h"
#include "sensor_cache.h"
#include "task_settings.h"
#include "wifi_app.h"
#include "http_server_monitor.h"
#include "main.h"

// Tag used for ESP serial console messages
static const char TAG[] = "main_app";
static bool s_sta_connect_requested_from_http = false;

// Queue handle used to manipulate the main queue of events.
QueueHandle_t app_queue_handle;

BaseType_t app_send_message(app_event_id_t eventID)
{
  app_event_t msg;
  msg.event_id = eventID;
  return xQueueSend(app_queue_handle, &msg, portMAX_DELAY);
}

static void main_task(void *pvParameters)
{
  (void)pvParameters;

  app_queue_handle = xQueueCreate(10, sizeof(app_event_t));
  if (app_queue_handle == NULL)
  {
    printf("Failed to create app queue\n");
    vTaskDelete(NULL);
  }

  esp_err_t led_init_status = rgb_led_init();
  if (led_init_status != ESP_OK)
  {
    ESP_LOGE(TAG, "RGB LED init failed: %s", esp_err_to_name(led_init_status));
  }

  relay_init();
  ESP_LOGI(TAG, "Relay initialized");

  wifi_app_start();
  rgb_led_wifi_app_started();

  while (1)
  {
    app_event_t app_event;
    if (xQueueReceive(app_queue_handle, &app_event, pdMS_TO_TICKS(1000)) == pdPASS)
    {
      printf("main_task received event id: %lu\n", (unsigned long)app_event.event_id);
      switch (app_event.event_id)
      {
      case WIFI_APP_MSG_START_HTTP_SERVER:
        ESP_LOGI(TAG, "WIFI_APP_MSG_START_HTTP_SERVER");

        if (http_server_start() == ESP_OK)
        {
          rgb_led_http_server_started();
        }
        break;

      case WIFI_APP_MSG_STA_CONNECTED_GOT_IP:
        ESP_LOGI(TAG, "WIFI_APP_MSG_STA_CONNECTED_GOT_IP");

        if (s_sta_connect_requested_from_http)
        {
          app_nvs_save_sta_creds();
          s_sta_connect_requested_from_http = false;
        }
        else
        {
          ESP_LOGI(TAG, "Skipping credential save: connection was not initiated from HTTP request");
        }

        break;

      case WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER:
        ESP_LOGI(TAG, "WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER");

        s_sta_connect_requested_from_http = true;
        if (wifi_app_connect_sta() != ESP_OK)
        {
          s_sta_connect_requested_from_http = false;
          ESP_LOGE(TAG, "Failed to start STA connection from HTTP server event");
        }

        http_server_monitor_send_message(HTTP_MSG_WIFI_CONNECT_INIT);
        break;

      default:
        break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void app_main(void)
{
  // Suppress verbose per-pin config logs from the ESP-IDF GPIO driver.
  esp_log_level_set("gpio", ESP_LOG_WARN);

  esp_err_t sensor_cache_status = sensor_cache_start();
  if (sensor_cache_status != ESP_OK)
  {
    ESP_LOGE(TAG, "Sensor cache task start failed: %s", esp_err_to_name(sensor_cache_status));
  }

  esp_err_t irrigation_status = irrigation_ctrl_start();
  if (irrigation_status != ESP_OK)
  {
    ESP_LOGE(TAG, "Irrigation control task start failed: %s", esp_err_to_name(irrigation_status));
  }

  xTaskCreatePinnedToCore(main_task,
                          "main_task",
                          MAIN_TASK_STACK_SIZE,
                          NULL,
                          MAIN_TASK_PRIORITY,
                          NULL,
                          MAIN_TASK_CORE_ID);
}