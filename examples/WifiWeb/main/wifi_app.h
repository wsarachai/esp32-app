#ifndef WIFI_APP_H
#define WIFI_APP_H

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_wifi_types.h"

/**
 * WiFi application event group status bits
 */
#define WIFI_APP_STARTED_BIT BIT0          // WiFi application started
#define WIFI_APP_AP_STARTED_BIT BIT1       // AP interface is up
#define WIFI_APP_AP_CONNECTED_BIT BIT2     // A station connected to the AP
#define WIFI_APP_AP_DISCONNECTED_BIT BIT3  // A station disconnected from the AP
#define WIFI_APP_STA_CONNECTED_BIT BIT4    // ESP32 connected to a router
#define WIFI_APP_STA_DISCONNECTED_BIT BIT5 // ESP32 disconnected from a router
#define WIFI_APP_STA_GOT_IP_BIT BIT6       // ESP32 obtained an IP address from router

// WiFi application settings
#define WIFI_AP_SSID "MJU-SmartFarm-AP"  // AP name
#define WIFI_AP_PASSWORD "password"      // AP password
#define WIFI_AP_CHANNEL 1                // AP channel
#define WIFI_AP_SSID_HIDDEN 0            // AP visibility
#define WIFI_AP_MAX_CONNECTIONS 5        // AP max clients
#define WIFI_AP_BEACON_INTERVAL 100      // AP beacon: 100 milliseconds recommended
#define WIFI_AP_IP "192.168.0.1"         // AP default IP
#define WIFI_AP_GATEWAY "192.168.0.1"    // AP default Gateway (should be the same as the IP)
#define WIFI_AP_NETMASK "255.255.255.0"  // AP netmask
#define WIFI_AP_BANDWIDTH WIFI_BW_HT20   // AP bandwidth 20 MHz (40 MHz is the other option)
#define WIFI_STA_POWER_SAVE WIFI_PS_NONE // Power save not used
#define MAX_SSID_LENGTH 32               // IEEE standard maximum
#define MAX_PASSWORD_LENGTH 64           // IEEE standard maximum
#define MAX_CONNECTION_RETRIES 5         // Retry number on disconnect

/**
 * @brief Starts the WiFi Access Point.
 *        Spawns an internal FreeRTOS task that initialises the network stack,
 *        configures the AP with the settings above, and then self-deletes —
 *        the WiFi driver continues to run in the background.
 */
void wifi_app_start(void);

// Sets STA credentials into the WiFi configuration stored in this module.
esp_err_t wifi_app_set_sta_creds(const char *ssid, const char *password);

// Gets STA credentials from the WiFi configuration stored in this module.
esp_err_t wifi_app_get_sta_creds(char *ssid, size_t ssid_len,
                                 char *password, size_t password_len);

#endif // WIFI_APP_H
