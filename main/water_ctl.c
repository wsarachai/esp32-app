/*
 * water_ctl.c
 *
 *  Created on: May 17, 2024
 *      Author: keng
 */

#include "driver/gpio.h"
#include "esp_log.h"

#include "app_nvs.h"
#include "wifi_app.h"
#include "water_humidity_oneshot.h"
#include "water_ctl.h"

#define WATER_GPIO 			2

#define WATER_STATUS_ON		1
#define WATER_STATUS_OFF	0

static char TAG[] = "water_ctl";

static uint8_t s_water_state = WATER_STATUS_OFF;


static water_config_t water_config = {0};

void water_ctl_configure(void)
{
    ESP_LOGI(TAG, "Configured to water GPIO!");
    gpio_reset_pin(WATER_GPIO);

	if (app_nvs_load_water_configs())
	{
		ESP_LOGI(TAG, "Loaded water configuration");
	}
	else
	{
	    water_config.analog_voltage_max = ANALOG_VOLTAGE_MAX_DEFAULT;
	    water_config.required_moiture_level = REQUIRED_MOI_LEVEL_DEFAULT;
	    water_config.duration = WATER_DURATION_DEFAULT;

	    app_nvs_save_water_configs();

		ESP_LOGI(TAG, "Unable to water configuration, use default value instead");
	}

    /* Set the GPIO as a push/pull output */
    gpio_set_direction(WATER_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(WATER_GPIO, WATER_STATUS_OFF);
}

void water_ctl_on(void)
{
	if (wifi_app_ready())
	{
		s_water_state = WATER_STATUS_ON;
		gpio_set_level(WATER_GPIO, s_water_state);
	}
}

void water_ctl_off(void)
{
	if (wifi_app_ready())
	{
		s_water_state = WATER_STATUS_OFF;
		gpio_set_level(WATER_GPIO, s_water_state);
	}
}

uint8_t water_ctl_is_on(void)
{
	return s_water_state == WATER_STATUS_ON;
}

water_config_t *water_ctl_get_config(void)
{
	return &water_config;
}

float water_ctl_get_soil_moisture(void)
{
	float current_voltage = water_humidity_get_voltage();

//	ESP_LOGI(TAG, "current_voltage: %.2f", current_voltage);

	float ival = (water_config.analog_voltage_max - current_voltage) / water_config.analog_voltage_max * 100.0;

	return ival;
}
