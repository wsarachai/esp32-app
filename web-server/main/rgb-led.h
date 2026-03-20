#ifndef RGB_LED_H
#define RGB_LED_H

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    const char *name;
} rgb_led_color_step_t;

typedef enum
{
    RGB_LED_COLOR_RED = 0,
    RGB_LED_COLOR_GREEN,
    RGB_LED_COLOR_BLUE,
    RGB_LED_COLOR_YELLOW,
    RGB_LED_COLOR_CYAN,
    RGB_LED_COLOR_MAGENTA,
    RGB_LED_COLOR_WHITE,
    RGB_LED_COLOR_OFF,
    RGB_LED_COLOR_COUNT,
} rgb_led_color_id_t;

esp_err_t rgb_led_init(void);
esp_err_t rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b);
esp_err_t rgb_led_set_off(void);
esp_err_t rgb_led_set_color_by_id(rgb_led_color_id_t color_id);
esp_err_t rgb_led_set_color_by_index(size_t index);
const rgb_led_color_step_t *rgb_led_get_color_sequence(size_t *count);

// Additional functions for setting specific colors based on application events

/**
 * Color to indicate WiFi application has started.
 */
void rgb_led_wifi_app_started(void);

/**
 * Color to indicate HTTP server has started.
 */
void rgb_led_http_server_started(void);

/**
 * Color to indicate that the ESP32 is connected to an access point.
 */
void rgb_led_wifi_connected(void);

/**
 * Color to indicate that the ESP32 is disconnected to an access point.
 */
void rgb_led_wifi_disconnected(void);

#endif // RGB_LED_H
