#include <stdbool.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// WiFi application settings
#define WIFI_AP_SSID "MJU-SmartFarm-V1.01" // AP name
#define WIFI_AP_PASSWORD "password"         // AP password
#define WIFI_AP_CHANNEL 1                    // AP channel
#define WIFI_AP_SSID_HIDDEN 0                // AP visibility
#define WIFI_AP_MAX_CONNECTIONS 5            // AP max clients
#define WIFI_AP_BEACON_INTERVAL 100          // AP beacon: 100 milliseconds recommended
#define WIFI_AP_IP "192.168.0.1"            // AP default IP
#define WIFI_AP_GATEWAY "192.168.0.1"       // AP default Gateway (should be the same as the IP)
#define WIFI_AP_NETMASK "255.255.255.0"     // AP netmask
#define WIFI_AP_BANDWIDTH WIFI_BW_HT20       // AP bandwidth 20 MHz (40 MHz is the other option)
#define WIFI_STA_POWER_SAVE WIFI_PS_NONE     // Power save not used
#define WIFI_COUNTRY_CODE "US"               // Regulatory domain (channels 1-11)
#define WIFI_AP_MAX_TX_POWER_QDBM 78         // 19.5 dBm in quarter-dBm units
#define MAX_SSID_LENGTH 32                   // IEEE standard maximum
#define MAX_PASSWORD_LENGTH 64               // IEEE standard maximum
#define MAX_CONNECTION_RETRIES 5             // Retry number on disconnect

static const char *TAG = "wifi_softap";
static int s_disconnect_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base != WIFI_EVENT) {
        return;
    }

    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        s_disconnect_count = 0;
        ESP_LOGI(TAG, "station connected: " MACSTR ", aid=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        s_disconnect_count++;
        ESP_LOGW(TAG,
                 "station disconnected: " MACSTR ", aid=%d, disconnect_count=%d",
                 MAC2STR(event->mac),
                 event->aid,
                 s_disconnect_count);

        if (s_disconnect_count >= MAX_CONNECTION_RETRIES) {
            ESP_LOGW(TAG, "disconnect count reached retry threshold (%d)", MAX_CONNECTION_RETRIES);
            s_disconnect_count = 0;
        }
    }
}

static void wifi_init_softap(void)
{
    const size_t ssid_len = strlen(WIFI_AP_SSID);
    const size_t password_len = strlen(WIFI_AP_PASSWORD);
    const bool has_password = password_len > 0;

    if (ssid_len == 0 || ssid_len > MAX_SSID_LENGTH) {
        ESP_LOGE(TAG, "invalid SSID length: %u", (unsigned int)ssid_len);
        return;
    }

    if (password_len > MAX_PASSWORD_LENGTH) {
        ESP_LOGE(TAG, "password exceeds configured max length: %u", (unsigned int)password_len);
        return;
    }

    if (has_password && (password_len < 8 || password_len > 63)) {
        ESP_LOGE(TAG, "WPA/WPA2 password must be between 8 and 63 characters");
        return;
    }

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(ap_netif == NULL ? ESP_FAIL : ESP_OK);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        NULL));

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

    esp_netif_ip_info_t ip_info = {
        .ip.addr = ESP_IP4TOADDR(192, 168, 0, 1),
        .gw.addr = ESP_IP4TOADDR(192, 168, 0, 1),
        .netmask.addr = ESP_IP4TOADDR(255, 255, 255, 0),
    };

    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .channel = WIFI_AP_CHANNEL,
            .password = WIFI_AP_PASSWORD,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
            .authmode = has_password ? WIFI_AUTH_WPA_WPA2_PSK : WIFI_AUTH_OPEN,
            .ssid_hidden = WIFI_AP_SSID_HIDDEN,
            .beacon_interval = WIFI_AP_BEACON_INTERVAL,
        },
    };

    wifi_config.ap.ssid_len = (uint8_t)ssid_len;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_country_code(WIFI_COUNTRY_CODE, true));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_AP_BANDWIDTH));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_STA_POWER_SAVE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(WIFI_AP_MAX_TX_POWER_QDBM));

    int8_t applied_tx_power_qdbm = 0;
    ESP_ERROR_CHECK(esp_wifi_get_max_tx_power(&applied_tx_power_qdbm));

    ESP_LOGI(TAG, "SoftAP started");
    ESP_LOGI(TAG, "SSID      : %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "Password  : %s", has_password ? WIFI_AP_PASSWORD : "<open>");
    ESP_LOGI(TAG, "Channel   : %d", WIFI_AP_CHANNEL);
    ESP_LOGI(TAG, "Country   : %s", WIFI_COUNTRY_CODE);
    ESP_LOGI(TAG, "TX Power  : %.2f dBm", applied_tx_power_qdbm / 4.0f);
    ESP_LOGI(TAG, "IP        : %s", WIFI_AP_IP);
    ESP_LOGI(TAG, "Gateway   : %s", WIFI_AP_GATEWAY);
    ESP_LOGI(TAG, "Netmask   : %s", WIFI_AP_NETMASK);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_softap();
}