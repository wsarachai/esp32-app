/*
 * sensor_controller.c
 *
 *  Created on: May 17, 2024
 *      Author: keng
 */

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "lwip/ip4_addr.h"
#include "sys/param.h"

#include "water_ctl.h"
#include "DHT22.h"
#include "water_humidity_oneshot.h"
#include "tasks_common.h"
#include "sensor_controller.h"

// Tag used for ESP serial console messages
static const char TAG[] = "sensor_ctl";
/**
 * Water event group handle and status bits
 */
static EventGroupHandle_t water_event_group;
const int WATER_ON_AUTO_BIT			= BIT0;
const int WATER_ON_BY_USER_BIT		= BIT1;
const int WATER_ON_PROGRESS_BIT		= BIT2;

static const uint64_t one_min_in_us = 60000000;

/**
 * ESP32 timer configuration passed to esp_timer_create.
 */
const esp_timer_create_args_t water_on_period_args = {
		.callback = &sensor_ctl_water_turn_off_callback,
		.arg = NULL,
		.dispatch_method = ESP_TIMER_TASK,
		.name = "fw_update_reset"
};
esp_timer_handle_t water_on_period;

// Sensor monitor task handle
static TaskHandle_t task_sensor_monitor = NULL;

// Queue handle used to manipulate the sensor queue of events
static QueueHandle_t sensor_ctl_monitor_queue_handle;

void sensor_ctl_water_turn_off_callback(void *arg)
{
	sensor_ctl_monitor_send_message(SENSOR_CTL_WATER_OFF_BY_TIMER);
}

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameter which can be passed to the task.
 */
static void sensor_ctrl_monitor(void *parameter)
{
	sensor_ctl_queue_message_t msg;
	water_config_t *water_config = NULL;

	for (;;)
	{
		if (xQueueReceive(sensor_ctl_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
			case SENSOR_CTL_INIT:
				ESP_LOGI(TAG, "SENSOR_CTL_INIT");

				break;

			case SENSOR_CTL_WATER_ON:

				if (!water_ctl_is_on())
				{
					ESP_LOGI(TAG, "SENSOR_CTL_WATER_ON");
					water_ctl_on();
					xEventGroupSetBits(water_event_group, WATER_ON_AUTO_BIT);
					xEventGroupSetBits(water_event_group, WATER_ON_PROGRESS_BIT);

					water_config = water_ctl_get_config();
					ESP_ERROR_CHECK(esp_timer_create(&water_on_period_args, &water_on_period));
					ESP_ERROR_CHECK(esp_timer_start_once(water_on_period, one_min_in_us * water_config->duration));
				}

				break;

			case SENSOR_CTL_WATER_OFF:

				if (water_ctl_is_on()) {
					ESP_LOGI(TAG, "SENSOR_CTL_WATER_OFF");
					water_ctl_off();
					xEventGroupClearBits(water_event_group, WATER_ON_AUTO_BIT);
					xEventGroupClearBits(water_event_group, WATER_ON_BY_USER_BIT);
					xEventGroupClearBits(water_event_group, WATER_ON_PROGRESS_BIT);
					ESP_ERROR_CHECK(esp_timer_stop(water_on_period));
				}

				break;

			case SENSOR_CTL_WATER_ON_BY_USER:

				if (!water_ctl_is_on())
				{
					ESP_LOGI(TAG, "SENSOR_CTL_WATER_ON_BY_USER");
					water_ctl_on();
					xEventGroupSetBits(water_event_group, WATER_ON_BY_USER_BIT);
					xEventGroupSetBits(water_event_group, WATER_ON_PROGRESS_BIT);

					water_config = water_ctl_get_config();
					ESP_ERROR_CHECK(esp_timer_create(&water_on_period_args, &water_on_period));
					ESP_ERROR_CHECK(esp_timer_start_once(water_on_period, one_min_in_us * water_config->duration));
				}

				break;

			case SENSOR_CTL_WATER_OFF_BY_USER:

				if (water_ctl_is_on())
				{
					ESP_LOGI(TAG, "SENSOR_CTL_WATER_OFF_BY_USER");
					water_ctl_off();
					xEventGroupClearBits(water_event_group, WATER_ON_AUTO_BIT);
					xEventGroupClearBits(water_event_group, WATER_ON_BY_USER_BIT);
					xEventGroupClearBits(water_event_group, WATER_ON_PROGRESS_BIT);
					ESP_ERROR_CHECK(esp_timer_stop(water_on_period));
				}

				break;

			case SENSOR_CTL_WATER_OFF_BY_TIMER:

				if (water_ctl_is_on())
				{
					ESP_LOGI(TAG, "SENSOR_CTL_WATER_OFF_BY_TIMER");
					water_ctl_off();
					xEventGroupClearBits(water_event_group, WATER_ON_AUTO_BIT);
					xEventGroupClearBits(water_event_group, WATER_ON_BY_USER_BIT);
					xEventGroupClearBits(water_event_group, WATER_ON_PROGRESS_BIT);
				}

				break;

			default:
				break;
			}
		}

	}
}

void automatic_watering_decision(void) {
	water_config_t *water_config = NULL;
	float ival = 0.0;
	EventBits_t eventBits;

//	ESP_LOGI(TAG, "ival: %.2f%%", ival);

	eventBits = xEventGroupGetBits(water_event_group);
	if (eventBits & WATER_ON_PROGRESS_BIT)
	{
		ESP_LOGI(TAG, "Watering...");
	}
	else
	{
		water_config = water_ctl_get_config();
		ival = water_ctl_get_soil_moisture();

		if (ival < water_config->required_moiture_level)
		{
			sensor_ctl_monitor_send_message(SENSOR_CTL_WATER_ON);
		}
		else
		{
			sensor_ctl_monitor_send_message(SENSOR_CTL_WATER_OFF);
		}
	}
}

/**
 * SENSOR_CTRL Sensor task
 */
static void sensor_ctrl_task(void *pvParameter)
{
	DHT22_init();
	water_humidity_init();

	sensor_ctl_monitor_send_message(SENSOR_CTL_INIT);

	// Create water event group
	water_event_group = xEventGroupCreate();

	for (;;)
	{
		DHT22_sync_obtain_value();
		water_humidity_sync_obtain_value();

		automatic_watering_decision();

		// Wait at least 2 seconds before reading again
		// The interval of the whole process must be more than 2 seconds
		vTaskDelay(4000 / portTICK_PERIOD_MS);
	}

	water_humidity_tear_down();
}


BaseType_t sensor_ctl_monitor_send_message(sensor_ctl_message_e msgID)
{
	sensor_ctl_queue_message_t msg;
	msg.msgID = msgID;
	return xQueueSend(sensor_ctl_monitor_queue_handle, &msg, portMAX_DELAY);
}

void SENSOR_CTRL_task_start(void)
{
	// Create HTTP server monitor task
	xTaskCreatePinnedToCore(&sensor_ctrl_monitor, "sensor_monitor", SENSOR_MONITOR_TASK_STACK_SIZE, NULL, SENSOR_MONITOR_TASK_PRIORITY, &task_sensor_monitor, SENSOR_MONITOR_TASK_CORE_ID);

	// Create the message queue
	sensor_ctl_monitor_queue_handle = xQueueCreate(3, sizeof(sensor_ctl_queue_message_t));

	xTaskCreatePinnedToCore(&sensor_ctrl_task, "SENSOR_CTRL_task", SENSOR_CTRL_TASK_STACK_SIZE, NULL, SENSOR_CTRL_TASK_PRIORITY, NULL, SENSOR_CTRL_TASK_CORE_ID);
}

