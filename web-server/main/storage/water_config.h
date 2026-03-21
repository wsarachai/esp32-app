#ifndef WATER_CONFIG_H
#define WATER_CONFIG_H

#include <stdint.h>

#include "esp_err.h"

typedef struct
{
  uint16_t min_moisture_level;
  uint16_t max_moisture_level;
  uint16_t duration_minutes;
} water_config_t;

// Returns in-memory config; lazy-loads from NVS on first call.
water_config_t water_config_get(void);

// Saves config to memory and NVS.
esp_err_t water_config_save(water_config_t cfg);

#endif // WATER_CONFIG_H
