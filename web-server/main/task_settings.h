#ifndef TASK_SETTINGS_H
#define TASK_SETTINGS_H

// Shared FreeRTOS task defaults for this component.
#define TASK_STACK_SIZE_DEFAULT 4096
#define TASK_PRIORITY_DEFAULT 5
#define TASK_CORE_ID_DEFAULT 0

// Main application task settings
#define MAIN_TASK_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define MAIN_TASK_PRIORITY TASK_PRIORITY_DEFAULT
#define MAIN_TASK_CORE_ID TASK_CORE_ID_DEFAULT

// WiFi application task settings
// NOTE: 12288 bytes required — wifi_app_task calls esp_wifi_start() which triggers
// full RF phy calibration on first boot. 4096 bytes is not enough for the full
// calibration call stack and causes a silent stack overflow → SW_RESET boot loop.
#define WIFI_APP_TASK_STACK_SIZE 12288
#define WIFI_APP_TASK_PRIORITY TASK_PRIORITY_DEFAULT
#define WIFI_APP_TASK_CORE_ID TASK_CORE_ID_DEFAULT

// Sensor cache task settings
#define SENSOR_TASK_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define SENSOR_TASK_PRIORITY (TASK_PRIORITY_DEFAULT + 3)
#define SENSOR_TASK_CORE_ID 1

// Irrigation control task settings
#define IRRIGATION_TASK_STACK_SIZE TASK_STACK_SIZE_DEFAULT
#define IRRIGATION_TASK_PRIORITY (TASK_PRIORITY_DEFAULT - 1)
#define IRRIGATION_TASK_CORE_ID 1

#endif // TASK_SETTINGS_H