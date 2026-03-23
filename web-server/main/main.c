#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "app_nvs.h"
#include "http_server.h"
#include "irrigation_ctrl.h"
#include "rgb-led.h"
#include "relay.h"
#include "sensor_cache.h"
#include "task_settings.h"
#include "time_sync.h"
#include "wifi_app.h"
#include "http_server_monitor.h"
#include "main.h"

// Tag used for ESP serial console messages
static const char TAG[] = "main_app";
static bool s_sta_connect_requested_from_http = false;

#define SENSOR_DATA_FLASH_DURATION_MS 150

static rgb_led_color_id_t s_current_led_color = RGB_LED_COLOR_BLUE;
static rgb_led_color_id_t s_status_led_color  = RGB_LED_COLOR_BLUE;
static bool               s_last_relay_state   = false;
static esp_timer_handle_t s_led_restore_timer  = NULL;

static void led_restore_timer_cb(void *arg)
{
  rgb_led_set_color_by_id(s_current_led_color);
}

static void set_led_status(rgb_led_color_id_t color)
{
  s_status_led_color  = color;
  s_current_led_color = color;
  if (s_led_restore_timer != NULL)
  {
    esp_timer_stop(s_led_restore_timer);
  }
  esp_err_t err = rgb_led_set_color_by_id(color);
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to set LED status color %d: %s", (int)color, esp_err_to_name(err));
  }
}

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

  const esp_timer_create_args_t led_restore_timer_args = {
      .callback = led_restore_timer_cb,
      .name = "led_restore",
  };
  esp_timer_create(&led_restore_timer_args, &s_led_restore_timer);

  relay_init();
  ESP_LOGI(TAG, "Relay initialized");

  esp_err_t time_init_status = time_sync_init();
  if (time_init_status != ESP_OK)
  {
    ESP_LOGW(TAG, "Offline RTC time unavailable during boot: %s", esp_err_to_name(time_init_status));
  }

  wifi_app_start();
  set_led_status(RGB_LED_COLOR_BLUE);

  while (1)
  {
    app_event_t app_event;
    if (xQueueReceive(app_queue_handle, &app_event, pdMS_TO_TICKS(1000)) == pdPASS)
    {
      // printf("main_task received event id: %lu\n", (unsigned long)app_event.event_id);
      switch (app_event.event_id)
      {
      case WIFI_APP_MSG_START_HTTP_SERVER:
        ESP_LOGI(TAG, "WIFI_APP_MSG_START_HTTP_SERVER");

        if (http_server_start() == ESP_OK)
        {
          set_led_status(RGB_LED_COLOR_CYAN);
        }
        break;

      case WIFI_APP_MSG_STA_CONNECTED:
        ESP_LOGI(TAG, "WIFI_APP_MSG_STA_CONNECTED");
        set_led_status(RGB_LED_COLOR_GREEN);
        break;

      case WIFI_APP_MSG_STA_DISCONNECTED:
        ESP_LOGI(TAG, "WIFI_APP_MSG_STA_DISCONNECTED");
        set_led_status(RGB_LED_COLOR_RED);
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

        // Start SNTP now that we have internet access.
        time_sync_start();
        break;

      case APP_MSG_SENSOR_DATA_RECEIVED:
        rgb_led_set_color_by_id(RGB_LED_COLOR_WHITE);
        if (s_led_restore_timer != NULL)
        {
          esp_timer_start_once(s_led_restore_timer, SENSOR_DATA_FLASH_DURATION_MS * 1000ULL);
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

    // Relay state polling — update LED to YELLOW while water is on.
    bool relay_on = relay_get_state();
    if (relay_on != s_last_relay_state)
    {
      s_last_relay_state = relay_on;
      if (relay_on)
      {
        ESP_LOGI(TAG, "Relay ON — LED MAGENTA");
        s_current_led_color = RGB_LED_COLOR_MAGENTA;
        rgb_led_set_color_by_id(RGB_LED_COLOR_MAGENTA);
      }
      else
      {
        ESP_LOGI(TAG, "Relay OFF — restoring LED");
        s_current_led_color = s_status_led_color;
        rgb_led_set_color_by_id(s_status_led_color);
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