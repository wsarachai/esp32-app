#ifndef SENSOR_CACHE_H_
#define SENSOR_CACHE_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/** Maximum number of unique sensor nodes tracked simultaneously. */
#define SENSOR_CACHE_MAX_DEVICES 8

typedef struct
{
    float humidity;
    float temperature;
    float soilMoisture;
    bool valid;
    int64_t timestamp_us;
} sensor_snapshot_t;

typedef struct
{
    uint8_t registered_devices;
    uint8_t online_devices;
    int64_t newest_timestamp_us;
} sensor_cache_stats_t;

esp_err_t sensor_cache_start(void);

/**
 * Returns the average of all registered device snapshots.
 * Falls back to built-in defaults when no devices have reported yet.
 */
bool sensor_cache_get_snapshot(sensor_snapshot_t *snapshot);

/**
 * Update (or register) a device's sensor reading.
 * @param device_id  Unique string identifier for the reporting node.
 *                   Must not be NULL or empty.
 */
esp_err_t sensor_cache_update_snapshot(const char *device_id,
                                       float temperature,
                                       float humidity,
                                       float soil_moisture);

bool sensor_cache_get_stats(sensor_cache_stats_t *stats, uint32_t offline_timeout_ms);

#endif // SENSOR_CACHE_H_
