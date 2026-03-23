idf_component_register(SRCS "actuators/relay.c" "actuators/irrigation_ctrl.c" "http/http_server_monitor.c" "sensors/sensor_cache.c" "main.c"
                             "storage/app_nvs.c"
                             "storage/water_config.c"
                             "http/http_server.c"
                             "http/http_server_relay.c"
                             "http/http_server_static.c"
                             "http/http_server_status.c"
                             "http/http_server_ota.c"
                             "http/http_server_wifi.c"
                             "network/rtc_ds3231.c"
                             "network/wifi_app.c"
                             "network/time_sync.c"
                             "actuators/rgb-led.c"
                    INCLUDE_DIRS "." "http" "network" "storage" "sensors" "actuators"
                    REQUIRES app_update
                             driver
                             esp_driver_gpio
                             esp_driver_ledc
                             esp_event
                             esp_http_server
                             esp_netif
                             esp_partition
                             esp_timer
                             esp_wifi
                             lwip
                             nvs_flash
                    EMBED_FILES "webpage/app.css" "webpage/app.js" "webpage/favicon.ico" "webpage/index.html" "webpage/jquery-3.3.1.min.js")