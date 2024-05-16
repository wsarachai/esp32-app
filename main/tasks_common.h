/*
 * tasks_common.h
 *
 *  Created on: May 12, 2024
 *      Author: keng
 */

#ifndef MAIN_TASKS_COMMON_H_
#define MAIN_TASKS_COMMON_H_

// WiFi application task
#define WIFI_APP_TASK_STACK_SIZE			4096
#define WIFI_APP_TASK_PRIORITY				5
#define WIFI_APP_TASK_CORE_ID				0

// HTTP Server task
#define HTTP_SERVER_TASK_STACK_SIZE			8192
#define HTTP_SERVER_TASK_PRIORITY			4
#define HTTP_SERVER_TASK_CORE_ID			0

// HTTP Server Monitor task
#define HTTP_SERVER_MONITOR_TASK_STACK_SIZE	4096
#define HTTP_SERVER_MONITOR_TASK_PRIORITY	3
#define HTTP_SERVER_MONITOR_TASK_CORE_ID	0

// Wifi Reset Button Task
#define WIFI_RESET_BUTTON_TASK_STACK_SIZE	2048
#define WIFI_RESET_BUTTON_TASK_PRIORITY		6
#define WIFI_RESET_BUTTON_TASK_CORE_ID		0

// DS3231 RTC task
#define DS3231_TASK_STACK_SIZE				4096
#define DS3231_TASK_PRIORITY				5
#define DS3231_TASK_CORE_ID					1

// DHT22 Sensor task
#define DHT22_TASK_STACK_SIZE				4096
#define DHT22_TASK_PRIORITY					5
#define DHT22_TASK_CORE_ID					1

// DHT22 Sensor task
#define WATER_HUMIDITY_TASK_STACK_SIZE		4096
#define WATER_HUMIDITY_TASK_PRIORITY		5
#define WATER_HUMIDITY_TASK_CORE_ID			1

// SNTP Time sync task
#define SNTP_TIME_SYNC_TASK_STACK_SIZE		4096
#define SNTP_TIME_SYNC_TASK_PRIORITY		4
#define SNTP_TIME_SYNC_TASK_CORE_ID			1

// AWS IoT Task
#define AWS_IOT_TASK_STACK_SIZE				9216
#define AWS_IOT_TASK_PRIORITY				6
#define AWS_IOT_TASK_CORE_ID				1

#endif /* MAIN_TASKS_COMMON_H_ */
