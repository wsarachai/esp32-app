#include <stdio.h>
#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RGB_LED_GPIO_R 25
#define RGB_LED_GPIO_G 26
#define RGB_LED_GPIO_B 27

#define RGB_LED_ACTIVE_LOW 0

#define RGB_LEDC_MODE LEDC_LOW_SPEED_MODE
#define RGB_LEDC_TIMER LEDC_TIMER_0
#define RGB_LEDC_RESOLUTION LEDC_TIMER_8_BIT
#define RGB_LEDC_FREQUENCY_HZ 5000

static const char *TAG = "rgb_test";

typedef struct {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	const char *name;
} rgb_color_step_t;

static uint32_t channel_duty_from_8bit(uint8_t value)
{
#if RGB_LED_ACTIVE_LOW
	return 255 - value;
#else
	return value;
#endif
}

static void rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
	ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_0, channel_duty_from_8bit(r));
	ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_0);

	ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_1, channel_duty_from_8bit(g));
	ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_1);

	ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_2, channel_duty_from_8bit(b));
	ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_2);
}

static esp_err_t rgb_led_init(void)
{
	ledc_timer_config_t timer_cfg = {
		.speed_mode = RGB_LEDC_MODE,
		.duty_resolution = RGB_LEDC_RESOLUTION,
		.timer_num = RGB_LEDC_TIMER,
		.freq_hz = RGB_LEDC_FREQUENCY_HZ,
		.clk_cfg = LEDC_AUTO_CLK,
	};
	ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

	ledc_channel_config_t red_cfg = {
		.gpio_num = RGB_LED_GPIO_R,
		.speed_mode = RGB_LEDC_MODE,
		.channel = LEDC_CHANNEL_0,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = RGB_LEDC_TIMER,
		.duty = channel_duty_from_8bit(0),
		.hpoint = 0,
	};

	ledc_channel_config_t green_cfg = {
		.gpio_num = RGB_LED_GPIO_G,
		.speed_mode = RGB_LEDC_MODE,
		.channel = LEDC_CHANNEL_1,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = RGB_LEDC_TIMER,
		.duty = channel_duty_from_8bit(0),
		.hpoint = 0,
	};

	ledc_channel_config_t blue_cfg = {
		.gpio_num = RGB_LED_GPIO_B,
		.speed_mode = RGB_LEDC_MODE,
		.channel = LEDC_CHANNEL_2,
		.intr_type = LEDC_INTR_DISABLE,
		.timer_sel = RGB_LEDC_TIMER,
		.duty = channel_duty_from_8bit(0),
		.hpoint = 0,
	};

	ESP_ERROR_CHECK(ledc_channel_config(&red_cfg));
	ESP_ERROR_CHECK(ledc_channel_config(&green_cfg));
	ESP_ERROR_CHECK(ledc_channel_config(&blue_cfg));

	return ESP_OK;
}

void app_main(void)
{
	ESP_ERROR_CHECK(rgb_led_init());

	const rgb_color_step_t sequence[] = {
		{.r = 255, .g = 0, .b = 0, .name = "RED"},
		{.r = 0, .g = 255, .b = 0, .name = "GREEN"},
		{.r = 0, .g = 0, .b = 255, .name = "BLUE"},
		{.r = 255, .g = 255, .b = 0, .name = "YELLOW"},
		{.r = 0, .g = 255, .b = 255, .name = "CYAN"},
		{.r = 255, .g = 0, .b = 255, .name = "MAGENTA"},
		{.r = 255, .g = 255, .b = 255, .name = "WHITE"},
		{.r = 0, .g = 0, .b = 0, .name = "OFF"},
	};

	const size_t steps = sizeof(sequence) / sizeof(sequence[0]);

	ESP_LOGI(TAG, "RGB LED test started (R=%d, G=%d, B=%d)", RGB_LED_GPIO_R, RGB_LED_GPIO_G, RGB_LED_GPIO_B);

	while (1) {
		for (size_t i = 0; i < steps; ++i) {
			rgb_set(sequence[i].r, sequence[i].g, sequence[i].b);
			ESP_LOGI(TAG, "Color: %s (R:%u G:%u B:%u)",
					 sequence[i].name,
					 sequence[i].r,
					 sequence[i].g,
					 sequence[i].b);
			vTaskDelay(pdMS_TO_TICKS(1000));
		}
	}
}