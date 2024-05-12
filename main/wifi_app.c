/*
 * wifi_app.c
 *
 *  Created on: May 12, 2024
 *      Author: keng
 */

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "lwip/netdb.h"

#include "rgb_led.h"
#include "wifi_app.h"

// Tag used for ESP serial console messages
static const char TAG [] = "wifi_app";

void wifi_app_start(void)
{
    while (true) {
		ESP_LOGI(TAG, "WIFI_APP...");

    	rgb_led_wifi_app_started();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    	rgb_led_http_server_started();
        vTaskDelay(1000 / portTICK_PERIOD_MS);

    	rgb_led_wifi_connected();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
