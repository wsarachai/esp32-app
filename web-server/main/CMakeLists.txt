idf_component_register(SRCS "main.c"
                             "app_nvs.c"
                             "http_server.c"
                             "wifi_app.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "webpage/app.css" "webpage/app.js" "webpage/favicon.ico" "webpage/index.html" "webpage/jquery-3.3.1.min.js"
                    REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_http_server)