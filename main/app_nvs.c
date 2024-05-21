/*
 * app_nvs.c
 *
 *  Created on: May 12, 2024
 *      Author: keng
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"

#include "water_ctl.h"
#include "app_nvs.h"
#include "wifi_app.h"

#define ANALOG_VOLTAGE_MAX 		"avol_max"
#define WATER_THRESHOLD_VAL		"threshold_v"
#define WATER_DURATION_VAL		"wduration"

// Tag for logging to the monitor
static const char TAG[] = "nvs";

// NVS name space used for station mode credentials
const char app_nvs_sta_creds_namespace[] = "stacreds";

// NVS name space used for water configs
const char app_nvs_water_config_namespace[] = "water_conf";

esp_err_t app_nvs_save_sta_creds(void)
{
	nvs_handle handle;
	esp_err_t esp_err;
	ESP_LOGI(TAG, "app_nvs_save_sta_creds: Saving station mode credentials to flash");

	wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();

	if (wifi_sta_config)
	{
		esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Set SSID
		esp_err = nvs_set_blob(handle, "ssid", wifi_sta_config->sta.ssid, MAX_SSID_LENGTH);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) setting SSID to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Set Password
		esp_err = nvs_set_blob(handle, "password", wifi_sta_config->sta.password, MAX_PASSWORD_LENGTH);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) setting Password to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Commit credentials to NVS
		esp_err = nvs_commit(handle);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_sta_creds: Error (%s) comitting credentials to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}
		nvs_close(handle);
		ESP_LOGI(TAG, "app_nvs_save_sta_creds: wrote wifi_sta_config: Station SSID: %s Password: %s", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
	}

	printf("app_nvs_save_sta_creds: returned ESP_OK\n");
	return ESP_OK;
}

bool app_nvs_load_sta_creds(void)
{
	nvs_handle handle;
	esp_err_t esp_err;

	ESP_LOGI(TAG, "app_nvs_load_sta_creds: Loading Wifi credentials from flash");

	if (nvs_open(app_nvs_sta_creds_namespace, NVS_READONLY, &handle) == ESP_OK)
	{
		wifi_config_t *wifi_sta_config = wifi_app_get_wifi_config();

		memset(wifi_sta_config, 0x00, sizeof(wifi_config_t));

		// Allocate buffer
		size_t wifi_config_size = sizeof(wifi_config_t);
		uint8_t *wifi_config_buff = (uint8_t*)malloc(sizeof(uint8_t) * wifi_config_size);
		memset(wifi_config_buff, 0x00, sizeof(wifi_config_size));

		// Load SSID
		wifi_config_size = sizeof(wifi_sta_config->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", wifi_config_buff, &wifi_config_size);
		if (esp_err != ESP_OK)
		{
			free(wifi_config_buff);
			printf("app_nvs_load_sta_creds: (%s) no station SSID found in NVS\n", esp_err_to_name(esp_err));
			return false;
		}
		memcpy(wifi_sta_config->sta.ssid, wifi_config_buff, wifi_config_size);

		// Load Password
		wifi_config_size = sizeof(wifi_sta_config->sta.password);
		esp_err = nvs_get_blob(handle, "password", wifi_config_buff, &wifi_config_size);
		if (esp_err != ESP_OK)
		{
			free(wifi_config_buff);
			printf("app_nvs_load_sta_creds: (%s) retrieving password!\n", esp_err_to_name(esp_err));
			return false;
		}
		memcpy(wifi_sta_config->sta.password, wifi_config_buff, wifi_config_size);

		free(wifi_config_buff);
		nvs_close(handle);

		printf("app_nvs_load_sta_creds: SSID: %s Password: %s\n", wifi_sta_config->sta.ssid, wifi_sta_config->sta.password);
		return wifi_sta_config->sta.ssid[0] != '\0';
	}
	else
	{
		return false;
	}
}

esp_err_t app_nvs_clear_sta_creds(void)
{
	nvs_handle handle;
	esp_err_t esp_err;
	ESP_LOGI(TAG, "app_nvs_clear_sta_creds: Clearing Wifi station mode credentials from flash");

	esp_err = nvs_open(app_nvs_sta_creds_namespace, NVS_READWRITE, &handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_sta_creds: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Erase credentials
	esp_err = nvs_erase_all(handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_sta_creds: Error (%s) erasing station mode credentials!\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Commit clearing credentials from NVS
	esp_err = nvs_commit(handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_sta_creds: Error (%s) NVS commit!\n", esp_err_to_name(esp_err));
		return esp_err;
	}
	nvs_close(handle);

	printf("app_nvs_clear_sta_creds: returned ESP_OK\n");
	return ESP_OK;
}

esp_err_t app_nvs_save_water_configs(void)
{
	nvs_handle handle;
	esp_err_t esp_err;

	ESP_LOGI(TAG, "app_nvs_save_water_configs: Saving water configs to flash");

	water_config_t *water_config = water_ctl_get_config();

	if (water_config)
	{
		esp_err = nvs_open(app_nvs_water_config_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_water_configs: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Set analog voltage max
		esp_err = nvs_set_i16(handle, ANALOG_VOLTAGE_MAX, water_config->analog_voltage_max);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_water_configs: Error (%s) setting analog voltage max to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Set threshold
		esp_err = nvs_set_i16(handle, WATER_THRESHOLD_VAL, water_config->threshold);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_water_configs: Error (%s) setting threshold to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Set duration
		esp_err = nvs_set_i16(handle, WATER_DURATION_VAL, water_config->duration);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_water_configs: Error (%s) setting duration to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		// Commit water configs to NVS
		esp_err = nvs_commit(handle);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_save_water_configs: Error (%s) comitting water configs to NVS!\n", esp_err_to_name(esp_err));
			return esp_err;
		}

		nvs_close(handle);

		ESP_LOGI(TAG, "app_nvs_save_water_configs: wrote water_config: Analog voltage max: %d Threshold: %d Duration: %d", water_config->analog_voltage_max, water_config->threshold, water_config->duration);
	}

	printf("app_nvs_save_water_configs: returned ESP_OK\n");
	return ESP_OK;
}

bool app_nvs_load_water_configs(void)
{
	nvs_handle handle;
	esp_err_t esp_err;

	ESP_LOGI(TAG, "app_nvs_load_sta_water_configs: Loading water configs from flash");

	if (nvs_open(app_nvs_water_config_namespace, NVS_READONLY, &handle) == ESP_OK)
	{
		water_config_t *water_config = water_ctl_get_config();

		memset(water_config, 0x00, sizeof(water_config_t));

		int16_t water_config_tmp = 0;

		// Load analog voltage max
		esp_err = nvs_get_i16(handle, ANALOG_VOLTAGE_MAX, &water_config_tmp);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_load_sta_water_configs: (%s) no analog voltage max found in NVS\n", esp_err_to_name(esp_err));
			return false;
		}
		water_config->analog_voltage_max = water_config_tmp;

		// Load threshold
		esp_err = nvs_get_i16(handle, WATER_THRESHOLD_VAL, &water_config_tmp);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_load_sta_water_configs: (%s) retrieving threshold!\n", esp_err_to_name(esp_err));
			return false;
		}
		water_config->threshold = water_config_tmp;


		// Load duration
		esp_err = nvs_get_i16(handle, WATER_DURATION_VAL, &water_config_tmp);
		if (esp_err != ESP_OK)
		{
			printf("app_nvs_load_sta_water_configs: (%s) retrieving duration!\n", esp_err_to_name(esp_err));
			return false;
		}
		water_config->duration = water_config_tmp;

		nvs_close(handle);

		printf("app_nvs_load_sta_water_configs: Analog voltage max: %d Threshold: %d Duration: %d\n", water_config->analog_voltage_max, water_config->threshold, water_config->duration);
		return true;
	}
	else
	{
		return false;
	}
}

esp_err_t app_nvs_clear_water_configs(void)
{
	nvs_handle handle;
	esp_err_t esp_err;
	ESP_LOGI(TAG, "app_nvs_clear_water_configs: Clearing water configs from flash");

	esp_err = nvs_open(app_nvs_water_config_namespace, NVS_READWRITE, &handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_water_configs: Error (%s) opening NVS handle!\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Erase credentials
	esp_err = nvs_erase_all(handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_water_configs: Error (%s) erasing water configs!\n", esp_err_to_name(esp_err));
		return esp_err;
	}

	// Commit clearing credentials from NVS
	esp_err = nvs_commit(handle);
	if (esp_err != ESP_OK)
	{
		printf("app_nvs_clear_water_configs: Error (%s) NVS commit!\n", esp_err_to_name(esp_err));
		return esp_err;
	}
	nvs_close(handle);

	printf("app_nvs_clear_water_configs: returned ESP_OK\n");
	return ESP_OK;
}
