#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "relay.h"
#include "sensor_cache.h"
#include "time_sync.h"
#include "wifi_app.h"
#include "water_config.h"

static const char TAG[] = "http_server_status";

static esp_err_t parse_header_u16(httpd_req_t *req, const char *header_name, uint16_t *out)
{
  char value[16] = {0};
  size_t value_len = httpd_req_get_hdr_value_len(req, header_name);
  if (value_len == 0 || value_len >= sizeof(value))
  {
    return ESP_ERR_INVALID_SIZE;
  }

  if (httpd_req_get_hdr_value_str(req, header_name, value, sizeof(value)) != ESP_OK)
  {
    return ESP_FAIL;
  }

  char *end_ptr = NULL;
  unsigned long parsed = strtoul(value, &end_ptr, 10);
  if (end_ptr == value || *end_ptr != '\0' || parsed > 65535UL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  *out = (uint16_t)parsed;
  return ESP_OK;
}

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
  water_config_t water_cfg = water_config_get();

  char time_buf[32];
  time_sync_get_local_time(time_buf, sizeof(time_buf));

  char json_response[352];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"time\":\"%s\",\"temp\":%.2f,\"humidity\":%.2f,\"soil-moisture\":%.2f,\"min-moiture-level\":%u,\"max-moiture-level\":%u,\"duration\":%u,\"water-status\":\"OFF\",\"wifi-connect-status\":%u,\"relay-status\":\"%s\"}",
      time_buf,
      snapshot.temperature,
      snapshot.humidity,
      snapshot.soilMoisture,
      (unsigned int)water_cfg.min_moisture_level,
      (unsigned int)water_cfg.max_moisture_level,
      (unsigned int)water_cfg.duration_minutes,
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

static esp_err_t http_server_save_water_config_handler(httpd_req_t *req)
{
  uint16_t min_moisture = 0;
  uint16_t max_moisture = 0;
  uint16_t duration_minutes = 0;

  if (parse_header_u16(req, "min-moisture-level", &min_moisture) != ESP_OK ||
      parse_header_u16(req, "max-moisture-level", &max_moisture) != ESP_OK ||
      parse_header_u16(req, "duration", &duration_minutes) != ESP_OK)
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid water configuration headers");
    return ESP_FAIL;
  }

  if (min_moisture > 100 || max_moisture > 100 || min_moisture > max_moisture)
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid moisture range");
    return ESP_FAIL;
  }

  water_config_t cfg = {
      .min_moisture_level = min_moisture,
      .max_moisture_level = max_moisture,
      .duration_minutes = duration_minutes,
  };

  esp_err_t err = water_config_save(cfg);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to save water configuration: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save water configuration");
    return ESP_FAIL;
  }

  char json_response[128];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"status\":\"saved\",\"min-moiture-level\":%u,\"max-moiture-level\":%u,\"duration\":%u}",
      (unsigned int)cfg.min_moisture_level,
      (unsigned int)cfg.max_moisture_level,
      (unsigned int)cfg.duration_minutes);

  if (written < 0 || written >= (int)sizeof(json_response))
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_local_time_handler(httpd_req_t *req)
{
  char time_buf[32];
  time_sync_get_local_time(time_buf, sizeof(time_buf));

  char json_response[64];
  int written = snprintf(json_response, sizeof(json_response),
                         "{\"time\":\"%s\"}", time_buf);

  if (written < 0 || written >= (int)sizeof(json_response))
  {
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
  esp_err_t err = httpd_register_uri_handler(server, &esp_server_status);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t save_water_config = {
      .uri = "/saveWaterConfigure.json",
      .method = HTTP_POST,
      .handler = http_server_save_water_config_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &save_water_config);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t local_time = {
      .uri = "/localTime.json",
      .method = HTTP_GET,
      .handler = http_server_local_time_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &local_time);
}