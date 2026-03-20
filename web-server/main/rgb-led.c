#include <stdio.h>
#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rgb-led.h"

#define RGB_LED_GPIO_R 25
#define RGB_LED_GPIO_G 26
#define RGB_LED_GPIO_B 27

#define RGB_LED_ACTIVE_LOW 0

#define RGB_LEDC_MODE LEDC_LOW_SPEED_MODE
#define RGB_LEDC_TIMER LEDC_TIMER_0
#define RGB_LEDC_RESOLUTION LEDC_TIMER_8_BIT
#define RGB_LEDC_FREQUENCY_HZ 5000

static const char *TAG = "rgb_led";

static const rgb_led_color_step_t RGB_COLOR_SEQUENCE[RGB_LED_COLOR_COUNT] = {
    {.r = 255, .g = 0, .b = 0, .name = "RED"},
    {.r = 0, .g = 255, .b = 0, .name = "GREEN"},
    {.r = 0, .g = 0, .b = 255, .name = "BLUE"},
    {.r = 255, .g = 255, .b = 0, .name = "YELLOW"},
    {.r = 0, .g = 255, .b = 255, .name = "CYAN"},
    {.r = 255, .g = 0, .b = 255, .name = "MAGENTA"},
    {.r = 255, .g = 255, .b = 255, .name = "WHITE"},
    {.r = 0, .g = 0, .b = 0, .name = "OFF"},
};

static uint32_t channel_duty_from_8bit(uint8_t value)
{
#if RGB_LED_ACTIVE_LOW
    return 255 - value;
#else
    return value;
#endif
}

esp_err_t rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_0, channel_duty_from_8bit(r));
    ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_0);

    ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_1, channel_duty_from_8bit(g));
    ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_1);

    ledc_set_duty(RGB_LEDC_MODE, LEDC_CHANNEL_2, channel_duty_from_8bit(b));
    ledc_update_duty(RGB_LEDC_MODE, LEDC_CHANNEL_2);
    return ESP_OK;
}

esp_err_t rgb_led_set_off(void)
{
    return rgb_led_set_color(0, 0, 0);
}

esp_err_t rgb_led_set_color_by_id(rgb_led_color_id_t color_id)
{
    if (color_id < 0 || color_id >= RGB_LED_COLOR_COUNT)
    {
        return ESP_ERR_INVALID_ARG;
    }

    return rgb_led_set_color_by_index((size_t)color_id);
}

esp_err_t rgb_led_set_color_by_index(size_t index)
{
    if (index >= RGB_LED_COLOR_COUNT)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const rgb_led_color_step_t *step = &RGB_COLOR_SEQUENCE[index];
    return rgb_led_set_color(step->r, step->g, step->b);
}

const rgb_led_color_step_t *rgb_led_get_color_sequence(size_t *count)
{
    if (count != NULL)
    {
        *count = RGB_LED_COLOR_COUNT;
    }

    return RGB_COLOR_SEQUENCE;
}

void rgb_led_wifi_app_started(void)
{
    (void)rgb_led_set_color_by_id(RGB_LED_COLOR_BLUE);
}

void rgb_led_http_server_started(void)
{
    (void)rgb_led_set_color_by_id(RGB_LED_COLOR_CYAN);
}

void rgb_led_wifi_connected(void)
{
    (void)rgb_led_set_color_by_id(RGB_LED_COLOR_GREEN);
}

void rgb_led_wifi_disconnected(void)
{
    (void)rgb_led_set_color_by_id(RGB_LED_COLOR_RED);
}

esp_err_t rgb_led_init(void)
{
    ESP_LOGI(TAG, "Initializing RGB LED...");

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