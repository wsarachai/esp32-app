#include <stdbool.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "relay.h"

#ifndef RELAY_GPIO
#define RELAY_GPIO GPIO_NUM_2
#endif

// Many single-channel relay boards are active-low. Change this to 1 if yours is active-high.
#ifndef RELAY_ACTIVE_LEVEL
#define RELAY_ACTIVE_LEVEL 0
#endif

#define RELAY_INACTIVE_LEVEL ((RELAY_ACTIVE_LEVEL) ? 0 : 1)
#define RELAY_SWITCH_DELAY_MS 2000

static const char *TAG = "relay";
static bool relay_current_state = false;
static bool relay_manual_override = false;

esp_err_t relay_set(bool enabled)
{
	int level = enabled ? RELAY_ACTIVE_LEVEL : RELAY_INACTIVE_LEVEL;
	esp_err_t err = gpio_set_level(RELAY_GPIO, level);

	if (err == ESP_OK) {
		relay_current_state = enabled;
		ESP_LOGI(TAG, "Relay %s", enabled ? "ON" : "OFF");
	}

	return err;
}

bool relay_get_state(void)
{
	return relay_current_state;
}

void relay_set_manual_override(bool active)
{
	relay_manual_override = active;
	ESP_LOGI(TAG, "Manual override: %s", active ? "ON" : "OFF");
}

bool relay_is_manual_override(void)
{
	return relay_manual_override;
}

void relay_init(void)
{
	gpio_config_t relay_gpio_config = {
		.pin_bit_mask = 1ULL << RELAY_GPIO,
		.mode = GPIO_MODE_OUTPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.intr_type = GPIO_INTR_DISABLE,
	};

	ESP_ERROR_CHECK(gpio_config(&relay_gpio_config));
	ESP_ERROR_CHECK(relay_set(false));
}
