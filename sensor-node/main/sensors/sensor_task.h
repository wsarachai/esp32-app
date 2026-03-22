#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "esp_err.h"

/**
 * @brief Starts the sensor FreeRTOS task.
 *        The task reads DHT22 (temperature + humidity) and soil moisture ADC
 *        every SENSOR_POLL_INTERVAL_MS milliseconds, then sends the readings
 *        to the Bluetooth GATT server. Safe to call multiple times -
 *        subsequent calls are no-ops.
 */
esp_err_t sensor_task_start(void);

#endif // SENSOR_TASK_H
