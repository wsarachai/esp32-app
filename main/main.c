/**
 * Application entry point.
 */

#include "esp_log.h"
#include "nvs_flash.h"

#include "sensor_controller.h"
#include "ds3231.h"
#include "aws_iot.h"
#include "sntp_time_sync.h"
#include "wifi_app.h"
#include "wifi_reset_button.h"

#include "esp_log.h"

static const char TAG[] = "main";

void wifi_application_connected_events(void)
{
	ESP_LOGI(TAG, "WiFi Application Connected!!");
	sntp_time_sync_task_start();
	aws_iot_start();
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %"PRIu32" bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Start Wifi
	wifi_app_start();

	// Configure Wifi reset button
	wifi_reset_button_config();

	// Start DS3231 RTC task
	DS3231_task_start();

	// Start sensor controller task
	SENSOR_CTRL_task_start();

	// Set connected event callback
	wifi_app_set_callback(&wifi_application_connected_events);
}
