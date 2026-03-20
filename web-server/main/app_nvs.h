/*
 * app_nvs.h
 *
 *  Created on: May 12, 2024
 *      Author: keng
 */

#ifndef MAIN_APP_NVS_H_
#define MAIN_APP_NVS_H_

/**
 * Saves station mode Wifi credentials to NVS
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_save_sta_creds(void);

/**
 * Loads the previously saved credentials from NVS.
 * @return true if previously saved credentials were found.
 */
bool app_nvs_load_sta_creds(void);

/**
 * Clears station mode credentials from NVS
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_clear_sta_creds(void);


/**
 * Saves water config values to NVS
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_save_water_configs(void);

/**
 * Loads the previously saved water configs from NVS.
 * @return true if previously saved water configs were found.
 */
bool app_nvs_load_water_configs(void);

/**
 * Clears water configs from NVS
 * @return ESP_OK if successful.
 */
esp_err_t app_nvs_clear_water_configs(void);

#endif /* MAIN_APP_NVS_H_ */
