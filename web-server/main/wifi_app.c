#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"

#include "app_nvs.h"
#include "main.h"
#include "rgb-led.h"
#include "task_settings.h"
#include "wifi_app.h"

static const char *TAG = "wifi_app";
static wifi_config_t s_ap_config;
static wifi_config_t s_sta_config;

/**
 * Wifi application event group handle and status bits
 */
static EventGroupHandle_t wifi_app_event_group;

esp_err_t wifi_app_set_sta_creds(const char *ssid, const char *password)
{
  if (ssid == NULL || password == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  size_t ssid_len = strnlen(ssid, MAX_SSID_LENGTH + 1);
  size_t pass_len = strnlen(password, MAX_PASSWORD_LENGTH + 1);
  if (ssid_len == 0 || pass_len == 0 || ssid_len > MAX_SSID_LENGTH || pass_len > MAX_PASSWORD_LENGTH)
  {
    return ESP_ERR_INVALID_SIZE;
  }

  memset(&s_sta_config, 0, sizeof(s_sta_config));
  memcpy(s_sta_config.sta.ssid, ssid, ssid_len);
  memcpy(s_sta_config.sta.password, password, pass_len);

  return ESP_OK;
}

esp_err_t wifi_app_get_sta_creds(char *ssid, size_t ssid_len,
                                 char *password, size_t password_len)
{
  if (ssid == NULL || password == NULL || ssid_len == 0 || password_len == 0)
  {
    return ESP_ERR_INVALID_ARG;
  }

  size_t src_ssid_len = strnlen((const char *)s_sta_config.sta.ssid, sizeof(s_sta_config.sta.ssid));
  size_t src_pass_len = strnlen((const char *)s_sta_config.sta.password, sizeof(s_sta_config.sta.password));
  if (src_ssid_len == 0 || src_pass_len == 0)
  {
    return ESP_ERR_INVALID_STATE;
  }

  if (src_ssid_len >= ssid_len || src_pass_len >= password_len)
  {
    return ESP_ERR_INVALID_SIZE;
  }

  memcpy(ssid, s_sta_config.sta.ssid, src_ssid_len);
  ssid[src_ssid_len] = '\0';

  memcpy(password, s_sta_config.sta.password, src_pass_len);
  password[src_pass_len] = '\0';

  return ESP_OK;
}

// ---------------------------------------------------------------------------
// Event handler
// ---------------------------------------------------------------------------

static void wifi_app_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT)
  {
    switch (event_id)
    {
    case WIFI_EVENT_AP_START:
      ESP_LOGI(TAG, "AP started  -  SSID: %s", WIFI_AP_SSID);
      xEventGroupSetBits(wifi_app_event_group, WIFI_APP_AP_STARTED_BIT);
      break;

    case WIFI_EVENT_AP_STOP:
      ESP_LOGI(TAG, "AP stopped");
      xEventGroupSetBits(wifi_app_event_group, WIFI_APP_AP_STOPPED_BIT);
      break;

    case WIFI_EVENT_AP_STACONNECTED:
    {
      wifi_event_ap_staconnected_t *e = (wifi_event_ap_staconnected_t *)event_data;
      ESP_LOGI(TAG, "Station " MACSTR " connected (AID=%d)", MAC2STR(e->mac), e->aid);
      xEventGroupSetBits(wifi_app_event_group, WIFI_APP_AP_CONNECTED_BIT);
      xEventGroupClearBits(wifi_app_event_group, WIFI_APP_AP_DISCONNECTED_BIT);
      break;
    }

    case WIFI_EVENT_AP_STADISCONNECTED:
    {
      wifi_event_ap_stadisconnected_t *e = (wifi_event_ap_stadisconnected_t *)event_data;
      ESP_LOGI(TAG, "Station " MACSTR " disconnected (AID=%d)", MAC2STR(e->mac), e->aid);
      xEventGroupSetBits(wifi_app_event_group, WIFI_APP_AP_DISCONNECTED_BIT);
      xEventGroupClearBits(wifi_app_event_group, WIFI_APP_AP_CONNECTED_BIT);
      break;
    }

    // STA events
    case WIFI_EVENT_STA_START:
      ESP_LOGI(TAG, "STA started");
      break;

    case WIFI_EVENT_STA_CONNECTED:
      ESP_LOGI(TAG, "STA connected to AP");
      xEventGroupSetBits(wifi_app_event_group, WIFI_APP_STA_CONNECTED_BIT);
      xEventGroupClearBits(wifi_app_event_group, WIFI_APP_STA_DISCONNECTED_BIT);
      rgb_led_wifi_connected();
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      ESP_LOGI(TAG, "STA disconnected from AP");
      xEventGroupSetBits(wifi_app_event_group, WIFI_APP_STA_DISCONNECTED_BIT);
      xEventGroupClearBits(wifi_app_event_group, WIFI_APP_STA_CONNECTED_BIT | WIFI_APP_STA_GOT_IP_BIT);
      rgb_led_wifi_disconnected();
      break;

    default:
      break;
    }
  }
  else if (event_base == IP_EVENT)
  {
    if (event_id == IP_EVENT_STA_GOT_IP)
    {
      ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
      ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
      xEventGroupSetBits(wifi_app_event_group, WIFI_APP_STA_GOT_IP_BIT);
      app_send_message(WIFI_APP_MSG_STA_CONNECTED_GOT_IP);
    }
  }
}

// ---------------------------------------------------------------------------
// Initialisation task (self-deletes after setup)
// ---------------------------------------------------------------------------

static void wifi_app_task(void *pvParameters)
{
  // Create the WiFi application event group
  wifi_app_event_group = xEventGroupCreate();
  xEventGroupSetBits(wifi_app_event_group, WIFI_APP_STARTED_BIT);

  // 1. Initialise NVS (required by the WiFi driver)
  ESP_ERROR_CHECK(app_nvs_init());

  // 2. Initialise TCP/IP stack
  ESP_ERROR_CHECK(esp_netif_init());

  // 3. Create the default event loop
  ESP_ERROR_CHECK(esp_event_loop_create_default());

  // 4. Create the default network interfaces (AP + STA)
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
  esp_netif_create_default_wifi_sta();

  // 5. Set static IP / gateway / netmask on the AP interface
  ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

  esp_netif_ip_info_t ip_info = {0};
  ip_info.ip.addr = inet_addr(WIFI_AP_IP);
  ip_info.gw.addr = inet_addr(WIFI_AP_GATEWAY);
  ip_info.netmask.addr = inet_addr(WIFI_AP_NETMASK);
  ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));

  ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

  // 6. Initialise the WiFi driver with default configuration
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // 7. Register event handlers for WiFi and IP events
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID,
      wifi_app_event_handler, NULL, NULL));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP,
      wifi_app_event_handler, NULL, NULL));

  // 8. Set power-save mode
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));

  // 9. Configure the AP
  memset(&s_ap_config, 0, sizeof(s_ap_config));
  s_ap_config = (wifi_config_t){
      .ap = {
          .ssid = WIFI_AP_SSID,
          .ssid_len = (uint8_t)strlen(WIFI_AP_SSID),
          .password = WIFI_AP_PASSWORD,
          .channel = WIFI_AP_CHANNEL,
          .ssid_hidden = WIFI_AP_SSID_HIDDEN,
          .max_connection = WIFI_AP_MAX_CONNECTIONS,
          .beacon_interval = WIFI_AP_BEACON_INTERVAL,
          .authmode = WIFI_AUTH_WPA2_PSK,
      },
  };

  ESP_ERROR_CHECK(wifi_app_set_sta_creds(WIFI_AP_SSID, WIFI_AP_PASSWORD));

  // 10. Set bandwidth, mode (APSTA), apply config, and start
  ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_AP_BANDWIDTH));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &s_ap_config));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &s_sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  ESP_LOGI(TAG, "WiFi AP ready. Connect to SSID \"%s\" with password \"%s\"",
           WIFI_AP_SSID, WIFI_AP_PASSWORD);
  ESP_LOGI(TAG, "AP IP: %s", WIFI_AP_IP);

  // Start HTTP server only after TCP/IP stack and WiFi driver are up.
  app_send_message(WIFI_APP_MSG_START_HTTP_SERVER);

  // Initialisation is complete; the WiFi driver runs autonomously.
  vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void wifi_app_start(void)
{
  xTaskCreatePinnedToCore(wifi_app_task,
                          "wifi_app_task",
                          WIFI_APP_TASK_STACK_SIZE,
                          NULL,
                          WIFI_APP_TASK_PRIORITY,
                          NULL,
                          WIFI_APP_TASK_CORE_ID);
}
