#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "rgb_led.h"

void app_main(void)
{
    while (true) {
    	rgb_led_wifi_app_started();
        sleep(1);

    	rgb_led_http_server_started();
        sleep(1);

    	rgb_led_wifi_connected();
        sleep(1);
    }
}
