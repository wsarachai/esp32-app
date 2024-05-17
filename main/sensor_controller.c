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

#include "DHT22.h"
#include "water_humidity_oneshot.h"
#include "tasks_common.h"
#include "sensor_controller.h"

/**
 * SENSOR_CTRL Sensor task
 */
static void SENSOR_CTRL_task(void *pvParameter)
{
	DHT22_init();
	water_humidity_init();

	for (;;)
	{
		DHT22_sync_obtain_value();
		water_humidity_sync_obtain_value();

		// Wait at least 2 seconds before reading again
		// The interval of the whole process must be more than 2 seconds
		vTaskDelay(4000 / portTICK_PERIOD_MS);
	}

	water_humidity_tear_down();
}

void SENSOR_CTRL_task_start(void)
{
	xTaskCreatePinnedToCore(&SENSOR_CTRL_task, "SENSOR_CTRL_task", SENSOR_CTRL_TASK_STACK_SIZE, NULL, SENSOR_CTRL_TASK_PRIORITY, NULL, SENSOR_CTRL_TASK_CORE_ID);
}

