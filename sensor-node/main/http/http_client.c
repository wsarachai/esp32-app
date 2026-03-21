#include "http_client.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_client.h"
#include "esp_log.h"
#include "wifi_sta.h"

#define SENSOR_UPDATE_PATH "/sensor-update"
#define SENSOR_UPDATE_URL "http://" WIFI_SERVER_HOST SENSOR_UPDATE_PATH

static const char *TAG = "http_client";

esp_err_t http_client_post_sensor_data(float temperature, float humidity, float soil_moisture)
{
    char body[128];
    int written = snprintf(body, sizeof(body),
                           "{\"temperature\":%.2f,\"humidity\":%.2f,\"soil_moisture\":%.2f}",
                           temperature, humidity, soil_moisture);
    if (written < 0 || written >= (int)sizeof(body))
    {
        ESP_LOGE(TAG, "JSON body truncated");
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_config_t config = {
        .url = SENSOR_UPDATE_URL,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set_header failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = esp_http_client_set_post_field(client, body, (int)strlen(body));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "set_post_field failed: %s", esp_err_to_name(err));
        goto cleanup;
    }

    err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300)
        {
            ESP_LOGI(TAG, "POST %s -> HTTP %d  body=%s", SENSOR_UPDATE_PATH, status, body);
        }
        else
        {
            ESP_LOGW(TAG, "POST %s -> unexpected HTTP %d", SENSOR_UPDATE_PATH, status);
            err = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "HTTP perform failed: %s", esp_err_to_name(err));
    }

cleanup:
    esp_http_client_cleanup(client);
    return err;
}
