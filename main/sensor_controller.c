/*
 * sensor_controller.c
 *
 *  Created on: May 17, 2024
 *      Author: keng
 */

#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
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

// Sensor monitor task handle
static TaskHandle_t task_sensor_monitor = NULL;

// Queue handle used to manipulate the sensor queue of events
static QueueHandle_t sensor_ctl_monitor_queue_handle;

/**
 * HTTP server monitor task used to track events of the HTTP server
 * @param pvParameters parameter which can be passed to the task.
 */
static void sensor_ctrl_monitor(void *parameter)
{
	sensor_ctl_queue_message_t msg;

	for (;;)
	{
		if (xQueueReceive(sensor_ctl_monitor_queue_handle, &msg, portMAX_DELAY))
		{
			switch (msg.msgID)
			{
			case SENSOR_CTL_INIT:
				ESP_LOGI(TAG, "SENSOR_CTL_INIT");

				configure_water();

				break;

			case SENSOR_CTL_WATER_ON:
				ESP_LOGI(TAG, "SENSOR_CTL_WATER_ON");

				water_on();

				break;

			case SENSOR_CTL_WATER_OFF:
				ESP_LOGI(TAG, "SENSOR_CTL_WATER_OFF");

				water_off();

				break;

			default:
				break;
			}
		}

	}
}

void automatic_watering_decision(void) {
	float current_voltage = water_humidity_get_voltage();
	water_config_t* water_config = get_water_config();

	ESP_LOGI(TAG, "current_voltage: %.2f", current_voltage);

	float ival = (water_config->analog_voltage_max - current_voltage) / water_config->analog_voltage_max * 100.0;

	ESP_LOGI(TAG, "ival: %.2f%%", ival);

	if (ival < water_config->low_bound)
	{
		sensor_ctl_monitor_send_message(SENSOR_CTL_WATER_ON);
	}
	else
	{
		sensor_ctl_monitor_send_message(SENSOR_CTL_WATER_OFF);
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

