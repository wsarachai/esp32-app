#ifndef RELAY_H
#define RELAY_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Initialize the relay GPIO and set to inactive state
 */
void relay_init(void);

/**
 * @brief Control the relay state
 * 
 * @param enabled true to activate relay, false to deactivate
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t relay_set(bool enabled);

/**
 * @brief Get current relay state
 * 
 * @return true if relay is ON, false if OFF
 */
bool relay_get_state(void);

#endif // RELAY_H
