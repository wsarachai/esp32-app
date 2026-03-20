/*
 * sntp_time_sync.c
 *
 *  Created on: May 13, 2024
 *      Author: keng
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/apps/sntp.h"

#include "ds3231.h"
#include "tasks_common.h"
#include "http_server.h"
#include "sntp_time_sync.h"
#include "wifi_app.h"

static const char TAG[] = "sntp_time_sync";
static const int year_ref = 2016;

// SNTP operating mode set status
static bool sntp_op_mode_set = false;

/**
 * Initialize SNTP service using SNTP_OPMODE_POLL mode.
 */
static void sntp_time_sync_init_sntp(void)
{
	ESP_LOGI(TAG, "Initializing the SNTP service");

	if (!sntp_op_mode_set)
	{
		// Set the operating mode
		sntp_setoperatingmode(SNTP_OPMODE_POLL);
		sntp_op_mode_set = true;
	}

	sntp_setservername(0, "pool.ntp.org");

	// Initialize the servers
	sntp_init();

	// Let the http_server know service is initialized
	http_server_monitor_send_message(HTTP_MSG_TIME_SERVICE_INITIALIZED);
}


/**
 * Gets the current time and if the current time is not up to date,
 * the sntp_time_synch_init_sntp function is called.
 */
static bool sntp_time_sync_obtain_time(void)
{
	bool ret = false;
	time_t now = 0;
	struct tm time_info = {0};
	struct tm rtcinfo = {0};

	time(&now);
	localtime_r(&now, &time_info);

	// Check the time, in case we need to initialize/reinitialize
	if (time_info.tm_year < (year_ref - 1900))
	{
		sntp_time_sync_init_sntp();

		// Set the local time zone
		setenv("TZ", "CST-7", 1);
		tzset();
	}
	else {
		if (ds3231_get_time(&rtcinfo) != ESP_OK) {
			ESP_LOGE(pcTaskGetName(0), "Could not get time.");
		}
		else
		{
			if (time_info.tm_year != rtcinfo.tm_year)
			{
//				char time_buffer[100];
//				ESP_LOGE(TAG, "timeinfo.tm_sec=%d", time_info.tm_sec);
//				ESP_LOGE(TAG, "timeinfo.tm_min=%d", time_info.tm_min);
//				ESP_LOGE(TAG, "timeinfo.tm_hour=%d", time_info.tm_hour);
//				ESP_LOGE(TAG, "timeinfo.tm_wday=%d", time_info.tm_wday);
//				ESP_LOGE(TAG, "timeinfo.tm_mday=%d", time_info.tm_mday);
//				ESP_LOGE(TAG, "timeinfo.tm_mon=%d", time_info.tm_mon);
//				ESP_LOGE(TAG, "timeinfo.tm_year=%d", time_info.tm_year);
//				strftime(time_buffer, sizeof(time_buffer), "%d.%m.%Y %H:%M:%S", &time_info);
//				ESP_LOGE(TAG, "Current time info: %s", time_buffer);

				struct tm time = {
					.tm_year = (time_info.tm_year % 100) + 2000,
					.tm_mon  = time_info.tm_mon,  // 0-based
					.tm_mday = time_info.tm_mday,
					.tm_hour = time_info.tm_hour,
					.tm_min  = time_info.tm_min,
					.tm_sec  = time_info.tm_sec
				};

				if (ds3231_set_time(&time) != ESP_OK) {
					ESP_LOGE(TAG, "Could not set time.");
				}
				else {
					ESP_LOGI(TAG, "DS3231 initial date time done");
					ret = true;
				}
			}
		}
	}
	return ret;
}

/**
 * The SNTP time synchronization task.
 * @param arg pvParam.
 */
static void sntp_time_sync(void *pvParam)
{
	int tried = 0;
	for (;;)
	{
		if (!sntp_time_sync_obtain_time())
		{
			if (++tried > 5)
			{
				break;
			}
		}
		else
		{
			tried = 0;
		}
		vTaskDelay(10000 / portTICK_PERIOD_MS);
	}

	vTaskDelete(NULL);
}

char* sntp_time_sync_get_time(void)
{
	static char time_buffer[100] = {0};

	time_t now = 0;
	struct tm time_info = {0};

	time(&now);
	localtime_r(&now, &time_info);

	if (time_info.tm_year < (year_ref - 1900))
	{
		ESP_LOGI(TAG, "Time is not set yet");
	}
	else
	{
		strftime(time_buffer, sizeof(time_buffer), "%d.%m.%Y %H:%M:%S", &time_info);
		ESP_LOGI(TAG, "Current time info: %s", time_buffer);
	}

	return time_buffer;
}

void sntp_time_sync_task_start(void)
{
	xTaskCreatePinnedToCore(&sntp_time_sync, "sntp_time_sync", SNTP_TIME_SYNC_TASK_STACK_SIZE, NULL, SNTP_TIME_SYNC_TASK_PRIORITY, NULL, SNTP_TIME_SYNC_TASK_CORE_ID);
}
