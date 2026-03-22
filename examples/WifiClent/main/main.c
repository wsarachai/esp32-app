#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

// Target AP credentials - must match the web-server Access Point settings.
#define WIFI_STA_AP_SSID "MJU-SmartFarm-AP"
#define WIFI_STA_AP_PASSWORD "password"

// Web-server IP (AP gateway) and HTTP port.
#define WIFI_SERVER_HOST "192.168.0.1"
#define WIFI_SERVER_PORT 80

#define WIFI_MAXIMUM_RETRY 10

static const char *TAG = "wifi_station_test";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

enum
{
  WIFI_CONNECTED_BIT = BIT0,
  WIFI_FAIL_BIT = BIT1,
};

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
  {
    esp_wifi_connect();
  }
  else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
  {
    if (s_retry_num < WIFI_MAXIMUM_RETRY)
    {
      esp_wifi_connect();
      s_retry_num++;
      ESP_LOGW(TAG, "Retry to connect to the AP (%d/%d)", s_retry_num, WIFI_MAXIMUM_RETRY);
    }
    else
    {
      xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    }
  }
  else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
  {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    s_retry_num = 0;
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static esp_err_t init_wifi_station(void)
{
  s_wifi_event_group = xEventGroupCreate();
  if (s_wifi_event_group == NULL)
  {
    ESP_LOGE(TAG, "Failed to create wifi event group");
    return ESP_FAIL;
  }

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT,
      ESP_EVENT_ANY_ID,
      &wifi_event_handler,
      NULL,
      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT,
      IP_EVENT_STA_GOT_IP,
      &wifi_event_handler,
      NULL,
      &instance_got_ip));

  wifi_config_t wifi_config = {
      .sta = {
          .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          .pmf_cfg = {
              .capable = true,
              .required = false,
          },
      },
  };

  strlcpy((char *)wifi_config.sta.ssid, WIFI_STA_AP_SSID, sizeof(wifi_config.sta.ssid));
  strlcpy((char *)wifi_config.sta.password, WIFI_STA_AP_PASSWORD, sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "wifi_init_sta finished");

  EventBits_t bits = xEventGroupWaitBits(
      s_wifi_event_group,
      WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
      pdFALSE,
      pdFALSE,
      portMAX_DELAY);

  esp_err_t result = ESP_FAIL;
  if (bits & WIFI_CONNECTED_BIT)
  {
    ESP_LOGI(TAG, "Connected to AP SSID:%s", WIFI_STA_AP_SSID);
    result = ESP_OK;
  }
  else if (bits & WIFI_FAIL_BIT)
  {
    ESP_LOGE(TAG, "Failed to connect to AP SSID:%s", WIFI_STA_AP_SSID);
  }
  else
  {
    ESP_LOGE(TAG, "Unexpected event");
  }

  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

  return result;
}

static esp_err_t test_http_server(void)
{
  char url[96];
  snprintf(url, sizeof(url), "http://%s:%d/", WIFI_SERVER_HOST, WIFI_SERVER_PORT);

  esp_http_client_config_t config = {
      .url = url,
      .method = HTTP_METHOD_GET,
      .timeout_ms = 5000,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == NULL)
  {
    ESP_LOGE(TAG, "Failed to initialize HTTP client");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Testing web-server at %s", url);
  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK)
  {
    int status = esp_http_client_get_status_code(client);
    int64_t content_length = esp_http_client_get_content_length(client);
    ESP_LOGI(TAG, "HTTP GET success, status=%d, content_length=%lld", status, content_length);
  }
  else
  {
    ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
  return err;
}

void app_main(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  if (init_wifi_station() != ESP_OK)
  {
    ESP_LOGE(TAG, "Wi-Fi setup failed; skip server test");
    return;
  }

  if (test_http_server() == ESP_OK)
  {
    ESP_LOGI(TAG, "Web-server access test: PASS");
  }
  else
  {
    ESP_LOGE(TAG, "Web-server access test: FAIL");
  }
}