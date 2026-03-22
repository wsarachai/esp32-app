#include "wifi_sta.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "nvs_flash.h"

#include "main.h"
#include "task_settings.h"

static const char *TAG = "wifi_sta";

static EventGroupHandle_t s_event_group;
static volatile bool s_connected = false;
static esp_timer_handle_t s_reconnect_timer = NULL;
static esp_timer_handle_t s_restart_timer = NULL;

#define WIFI_CONNECTED_BIT BIT0

// Reconnect backoff: 3 s → 6 s → 10 s (capped).
#define WIFI_RECONNECT_DELAY_MIN_US (3 * 1000 * 1000)
#define WIFI_RECONNECT_DELAY_MAX_US (10 * 1000 * 1000)
// After this many consecutive failures, do a full radio reset.
#define WIFI_FULL_RESTART_AFTER 8

static volatile uint8_t s_reconnect_attempts = 0;

static uint32_t reconnect_delay_us(void)
{
    if (s_reconnect_attempts <= 1)
        return WIFI_RECONNECT_DELAY_MIN_US; // 3 s
    if (s_reconnect_attempts == 2)
        return 6 * 1000 * 1000;         // 6 s
    return WIFI_RECONNECT_DELAY_MAX_US; // 10 s
}

// Scan for the target SSID before connecting so the driver learns the correct
// channel and BSSID.  Direct esp_wifi_connect() uses the cached channel which
// may be stale or wrong (ap:<255,255>), causing auth→init(0x200) timeouts.
static void scan_and_connect(void)
{
    if (s_connected)
        return;
    ESP_LOGI(TAG, "Scanning for \"%s\"...", WIFI_STA_AP_SSID);
    wifi_scan_config_t scan_cfg = {
        .ssid = (uint8_t *)WIFI_STA_AP_SSID,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 120,
        .scan_time.active.max = 400,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Scan start failed (%s) — attempting direct connect", esp_err_to_name(err));
        esp_wifi_connect();
    }
}

static void wifi_restart_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "Radio reset: restarting WiFi stack...");
    esp_wifi_start(); // fires WIFI_EVENT_STA_START → scan_and_connect()
}

static void reconnect_timer_cb(void *arg)
{
    if (s_connected)
        return;
    if (s_reconnect_attempts >= WIFI_FULL_RESTART_AFTER)
    {
        ESP_LOGW(TAG, "Full WiFi radio reset after %u consecutive failures",
                 s_reconnect_attempts);
        s_reconnect_attempts = 0;
        esp_wifi_stop();
        // Allow 2 s for the driver to fully stop before restart.
        esp_timer_start_once(s_restart_timer, 2 * 1000 * 1000);
        return;
    }
    scan_and_connect();
}

bool wifi_sta_is_connected(void)
{
    return s_connected;
}

// ---------------------------------------------------------------------------
// Event handler (runs in the WiFi/IP event loop task)
// ---------------------------------------------------------------------------

static void wifi_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    if (base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started — scanning for \"%s\"", WIFI_STA_AP_SSID);
            scan_and_connect();
            break;

        case WIFI_EVENT_SCAN_DONE:
        {
            if (s_connected)
                break;
            uint16_t n = 1;
            wifi_ap_record_t ap_rec = {0};
            // Always call get_ap_records to free the internal scan buffer.
            esp_wifi_scan_get_ap_records(&n, &ap_rec);
            if (n > 0)
            {
                ESP_LOGI(TAG, "Found \"%s\" ch%u RSSI:%d — connecting (attempt %u)...",
                         ap_rec.ssid, ap_rec.primary, ap_rec.rssi,
                         s_reconnect_attempts + 1);
                // Pin BSSID+channel so the driver hits the right AP directly.
                wifi_config_t sta_cfg;
                esp_wifi_get_config(WIFI_IF_STA, &sta_cfg);
                memcpy(sta_cfg.sta.bssid, ap_rec.bssid, sizeof(sta_cfg.sta.bssid));
                sta_cfg.sta.bssid_set = 1;
                esp_wifi_set_config(WIFI_IF_STA, &sta_cfg);
                esp_wifi_connect();
            }
            else
            {
                uint32_t delay_us = reconnect_delay_us();
                s_reconnect_attempts++;
                ESP_LOGW(TAG, "AP \"%s\" not found — retry in %lus (attempt %u)",
                         WIFI_STA_AP_SSID, (unsigned long)(delay_us / 1000000),
                         s_reconnect_attempts);
                esp_timer_stop(s_reconnect_timer);
                esp_timer_start_once(s_reconnect_timer, delay_us);
            }
            break;
        }

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "STA connected to AP");
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            s_connected = false;
            app_send_message(APP_MSG_WIFI_DISCONNECTED);
            uint32_t delay_us = reconnect_delay_us();
            s_reconnect_attempts++;
            ESP_LOGW(TAG, "STA disconnected — retrying in %lus (attempt %u)",
                     (unsigned long)(delay_us / 1000000), s_reconnect_attempts);
            esp_timer_stop(s_reconnect_timer);
            esp_timer_start_once(s_reconnect_timer, delay_us);
            break;
        }

        default:
            break;
        }
    }
    else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_reconnect_attempts = 0; // reset backoff on successful connection
        xEventGroupSetBits(s_event_group, WIFI_CONNECTED_BIT);
        app_send_message(APP_MSG_WIFI_CONNECTED_GOT_IP);
    }
}

// ---------------------------------------------------------------------------
// Init task — self-deletes after WiFi driver setup
// ---------------------------------------------------------------------------

static void wifi_sta_task(void *pvParameters)
{
    s_event_group = xEventGroupCreate();

    // 1. Init NVS (required by the WiFi driver)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "Erasing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Initialise TCP/IP stack and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 3. Initialise the WiFi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 4. Create timers.
    const esp_timer_create_args_t reconnect_args = {
        .callback = reconnect_timer_cb,
        .name = "wifi_reconnect",
    };
    ESP_ERROR_CHECK(esp_timer_create(&reconnect_args, &s_reconnect_timer));

    const esp_timer_create_args_t restart_args = {
        .callback = wifi_restart_timer_cb,
        .name = "wifi_restart",
    };
    ESP_ERROR_CHECK(esp_timer_create(&restart_args, &s_restart_timer));

    // 5. Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL));

    // 6. Configure STA with the target AP credentials
    wifi_config_t sta_cfg = {
        .sta = {
            .ssid = WIFI_STA_AP_SSID,
            .password = WIFI_STA_AP_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false,
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    // WIFI_EVENT_STA_START fires → scan_and_connect() → WIFI_EVENT_SCAN_DONE → connect

    // Wait until connected (or forever — reconnect is automatic)
    xEventGroupWaitBits(s_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdFALSE, portMAX_DELAY);

    ESP_LOGI(TAG, "Initial connection to \"%s\" established", WIFI_STA_AP_SSID);
    vTaskDelete(NULL);
}

void wifi_sta_force_reconnect(void)
{
    if (!s_connected)
    {
        return; // already cycling, reconnect timer will handle it
    }
    ESP_LOGW(TAG, "Server unreachable — forcing WiFi reconnect");
    s_connected = false;
    // Disconnect fires WIFI_EVENT_STA_DISCONNECTED -> reconnect timer -> esp_wifi_connect()
    esp_wifi_disconnect();
}

void wifi_sta_start(void)
{
    xTaskCreatePinnedToCore(
        wifi_sta_task,
        "wifi_sta_task",
        WIFI_STA_TASK_STACK_SIZE,
        NULL,
        WIFI_STA_TASK_PRIORITY,
        NULL,
        WIFI_STA_TASK_CORE_ID);
}
