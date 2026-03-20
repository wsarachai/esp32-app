idf_component_register(SRCS "relay.c" "http_server_monitor.c" "sensor_cache.c" "soil_moisture_adc.c" "main.c"
                             "app_nvs.c"
                             "http_server.c"
                             "wifi_app.c"
                             "rgb-led.c"
                             "dht22.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "webpage/app.css" "webpage/app.js" "webpage/favicon.ico" "webpage/index.html" "webpage/jquery-3.3.1.min.js")