#ifndef WIFI_STA_H
#define WIFI_STA_H

#include <stdbool.h>

// Target AP credentials — must match the web-server Access Point settings.
#define WIFI_STA_AP_SSID "MJU-SmartFarm-AP"
#define WIFI_STA_AP_PASSWORD "password"

// Web-server IP (AP gateway) and HTTP port.
#define WIFI_SERVER_HOST "192.168.0.1"
#define WIFI_SERVER_PORT 80

/**
 * @brief Starts WiFi in STA-only mode and connects to WIFI_STA_AP_SSID.
 *        Spawns an internal FreeRTOS task that initialises the network stack,
 *        connects to the AP, and self-deletes after configuration.
 *        Connection events are forwarded to the main-task queue via
 *        app_send_message().
 */
void wifi_sta_start(void);

/**
 * @brief Returns true when the STA interface has a valid IP address.
 */
bool wifi_sta_is_connected(void);

/**
 * @brief Forces a WiFi disconnect + reconnect cycle.
 *        Call when the server is unreachable despite WiFi appearing connected
 *        (e.g. AP rebooted but deauth frame was never received).
 */
void wifi_sta_force_reconnect(void);

#endif // WIFI_STA_H
