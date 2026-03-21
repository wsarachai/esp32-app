#ifndef IRRIGATION_CTRL_H_
#define IRRIGATION_CTRL_H_

#include "esp_err.h"

/**
 * @brief Start the irrigation control task.
 *
 * The task periodically reads the soil moisture from sensor_cache and
 * controls the relay automatically based on water_config thresholds:
 *   - Relay ON  when soil moisture falls below min_moisture_level.
 *   - Relay OFF when soil moisture reaches max_moisture_level, or when
 *     the relay has been ON for longer than duration_minutes (safety cap).
 *
 * Manual override (set via relay_set_manual_override()) takes the highest
 * priority: while active, automatic switching is completely suspended.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t irrigation_ctrl_start(void);

#endif // IRRIGATION_CTRL_H_
