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

/**
 * @brief Set or clear the manual override flag.
 *
 * When active, the automatic irrigation controller skips all decisions
 * so the relay stays under direct user control.
 *
 * @param active true to enable manual override, false to return to automatic mode
 */
void relay_set_manual_override(bool active);

/**
 * @brief Check whether manual override is currently active.
 *
 * @return true if the relay is under manual control, false if in automatic mode
 */
bool relay_is_manual_override(void);

#endif // RELAY_H
