/*
 * sensor_controller.h
 *
 *  Created on: May 17, 2024
 *      Author: keng
 */

#ifndef MAIN_SENSOR_CONTROLLER_H_
#define MAIN_SENSOR_CONTROLLER_H_

#include "freertos/FreeRTOS.h"

/**
 * Messages for the sensor monitor
 */
typedef enum sensor_ctl_message
{
	SENSOR_CTL_INIT = 0,
	SENSOR_CTL_WATER_ON,
	SENSOR_CTL_WATER_OFF,
	SENSOR_CTL_WATER_OFF_BY_TIMER,
	SENSOR_CTL_WATER_ON_BY_USER,
	SENSOR_CTL_WATER_OFF_BY_USER
} sensor_ctl_message_e;

/**
 * Structure for the message queue
 */
typedef struct sensor_ctl_queue_message
{
	sensor_ctl_message_e msgID;
} sensor_ctl_queue_message_t;

/**
 * Starts DHT22 sensor task
 */
void SENSOR_CTRL_task_start(void);


/**
 * Sends a message to the queue
 * @param msgID message ID from the sensor_ctl_message_e enum.
 * @return pdTRUE if an item was successfully sent to the queue, otherwise pdFALSE.
 * @note Expand the parameter list based on your requirements e.g. how you've expanded the sensor_ctl_queue_message_t.
 */
BaseType_t sensor_ctl_monitor_send_message(sensor_ctl_message_e msgID);


/**
 * Timer callback function which turn off water.
 */
void sensor_ctl_water_turn_off_callback(void *arg);


#endif /* MAIN_SENSOR_CONTROLLER_H_ */
