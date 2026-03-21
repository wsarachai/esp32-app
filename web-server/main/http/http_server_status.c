#include "http_server.h"

#include <stdio.h>

#include "esp_log.h"
#include "relay.h"
#include "sensor_cache.h"
#include "wifi_app.h"

static const char TAG[] = "http_server_status";

static esp_err_t http_server_status_handler(httpd_req_t *req)
{
  sensor_snapshot_t snapshot = {
      .humidity = -1.0f,
      .temperature = -1000.0f,
      .soilMoisture = 0.0f,
      .valid = false,
      .timestamp_us = 0,
  };
  bool has_snapshot = sensor_cache_get_snapshot(&snapshot);

  if (!has_snapshot)
  {
    ESP_LOGW(TAG, "Sensor cache not ready; returning fallback values");
  }

  const char *relay_status = relay_get_state() ? "ON" : "OFF";
  uint8_t wifi_connect_status = wifi_app_get_sta_connect_status();

  char json_response[320];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"time\":\"--:--:--\",\"temp\":%.2f,\"humidity\":%.2f,\"soil-moisture\":%.2f,\"min-moiture-level\":0,\"max-moiture-level\":0,\"duration\":0,\"water-status\":\"OFF\",\"wifi-connect-status\":%u,\"relay-status\":\"%s\"}",
      snapshot.temperature,
      snapshot.humidity,
      snapshot.soilMoisture,
      (unsigned int)wifi_connect_status,
      relay_status);

  if (written < 0 || written >= (int)sizeof(json_response))
  {
    ESP_LOGE(TAG, "Failed to create ESPServerStatus JSON response");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_register_status_handlers(httpd_handle_t server)
{
  httpd_uri_t esp_server_status = {
      .uri = "/ESPServerStatus.json",
      .method = HTTP_GET,
      .handler = http_server_status_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &esp_server_status);
}