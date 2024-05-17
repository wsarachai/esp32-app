# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS "main.c" "rgb_led.c" "wifi_app.c" "http_server.c" "DHT22.c" "app_nvs.c" "wifi_reset_button.c" "sntp_time_sync.c" "aws_iot.c" "i2cdev.c" "ds3231.c" "water_humidity_oneshot.c" "sensor_controller.c" "water_ctl.c"
    INCLUDE_DIRS "."
    EMBED_FILES "webpage/app.css" "webpage/app.js" "webpage/favicon.ico" "webpage/index.html" "webpage/jquery-3.3.1.min.js"
)
