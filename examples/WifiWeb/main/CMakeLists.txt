idf_component_register(SRCS "main.c"
                             "app_nvs.c"
                             "wifi_app.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi esp_event esp_netif nvs_flash)