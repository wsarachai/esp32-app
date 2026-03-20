#ifndef APP_NVS_H
#define APP_NVS_H

#include "esp_err.h"

// Initializes the default NVS partition with standard recovery logic.
esp_err_t app_nvs_init(void);

// Erases the default NVS partition.
esp_err_t app_nvs_erase(void);

// Saves STA credentials in NVS under the "wifi" namespace.
esp_err_t app_nvs_save_sta_creds(void);

// Loads STA credentials from NVS and applies them to the WiFi STA config.
esp_err_t app_nvs_load_sta_creds(void);

#endif // APP_NVS_H
