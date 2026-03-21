#ifndef SENSOR_CACHE_H_
#define SENSOR_CACHE_H_

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct
{
    float humidity;
    float temperature;
    float soilMoisture;
    bool valid;
    int64_t timestamp_us;
} sensor_snapshot_t;

esp_err_t sensor_cache_start(void);
bool sensor_cache_get_snapshot(sensor_snapshot_t *snapshot);
esp_err_t sensor_cache_update_snapshot(float temperature, float humidity, float soil_moisture);

#endif // SENSOR_CACHE_H_
