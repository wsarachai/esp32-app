#include "http_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "relay.h"
#include "sensor_cache.h"
#include "time_sync.h"
#include "http_server_monitor.h"
#include "wifi_app.h"
#include "water_config.h"
#include "main.h"

static const char TAG[] = "http_server_status";

#define SENSOR_UPDATE_MAX_BODY_LEN 256
#define WEB_SESSION_ID_MAX_LEN 64
#define WEB_SESSION_MAX_CLIENTS 16
#define WEB_SESSION_TIMEOUT_US (30LL * 1000LL * 1000LL)

typedef struct
{
  bool in_use;
  char session_id[WEB_SESSION_ID_MAX_LEN];
  int64_t last_seen_us;
} web_session_t;

static web_session_t s_web_sessions[WEB_SESSION_MAX_CLIENTS] = {0};

static uint8_t web_session_count_active(int64_t now_us)
{
  uint8_t active_count = 0;
  for (size_t i = 0; i < WEB_SESSION_MAX_CLIENTS; i++)
  {
    if (!s_web_sessions[i].in_use)
    {
      continue;
    }

    if ((now_us - s_web_sessions[i].last_seen_us) > WEB_SESSION_TIMEOUT_US)
    {
      s_web_sessions[i].in_use = false;
      s_web_sessions[i].session_id[0] = '\0';
      s_web_sessions[i].last_seen_us = 0;
      continue;
    }

    active_count++;
  }
  return active_count;
}

static void web_session_touch(const char *session_id)
{
  if (session_id == NULL || session_id[0] == '\0')
  {
    return;
  }

  int64_t now_us = esp_timer_get_time();
  web_session_count_active(now_us);

  // Refresh existing session if present.
  for (size_t i = 0; i < WEB_SESSION_MAX_CLIENTS; i++)
  {
    if (!s_web_sessions[i].in_use)
    {
      continue;
    }
    if (strncmp(s_web_sessions[i].session_id, session_id, WEB_SESSION_ID_MAX_LEN) == 0)
    {
      s_web_sessions[i].last_seen_us = now_us;
      return;
    }
  }

  // Add new session in first free slot.
  for (size_t i = 0; i < WEB_SESSION_MAX_CLIENTS; i++)
  {
    if (!s_web_sessions[i].in_use)
    {
      s_web_sessions[i].in_use = true;
      strlcpy(s_web_sessions[i].session_id, session_id, sizeof(s_web_sessions[i].session_id));
      s_web_sessions[i].last_seen_us = now_us;
      return;
    }
  }

  // If full, replace the oldest slot.
  size_t oldest_idx = 0;
  int64_t oldest_seen = s_web_sessions[0].last_seen_us;
  for (size_t i = 1; i < WEB_SESSION_MAX_CLIENTS; i++)
  {
    if (s_web_sessions[i].last_seen_us < oldest_seen)
    {
      oldest_seen = s_web_sessions[i].last_seen_us;
      oldest_idx = i;
    }
  }

  s_web_sessions[oldest_idx].in_use = true;
  strlcpy(s_web_sessions[oldest_idx].session_id, session_id, sizeof(s_web_sessions[oldest_idx].session_id));
  s_web_sessions[oldest_idx].last_seen_us = now_us;
}

static uint8_t web_session_get_connected_count(void)
{
  return web_session_count_active(esp_timer_get_time());
}

static esp_err_t web_session_get_header_id(httpd_req_t *req, char *out_id, size_t out_id_size)
{
  if (req == NULL || out_id == NULL || out_id_size < 2)
  {
    return ESP_ERR_INVALID_ARG;
  }

  size_t value_len = httpd_req_get_hdr_value_len(req, "X-Web-Session-Id");
  if (value_len == 0 || value_len >= out_id_size)
  {
    return ESP_ERR_INVALID_SIZE;
  }

  if (httpd_req_get_hdr_value_str(req, "X-Web-Session-Id", out_id, out_id_size) != ESP_OK)
  {
    return ESP_FAIL;
  }

  return ESP_OK;
}

static bool parse_json_string_field(const char *json, const char *key, char *out, size_t out_size)
{
  if (json == NULL || key == NULL || out == NULL || out_size < 2)
  {
    return false;
  }

  char needle[48];
  int needle_len = snprintf(needle, sizeof(needle), "\"%s\"", key);
  if (needle_len <= 0 || needle_len >= (int)sizeof(needle))
  {
    return false;
  }

  const char *key_pos = strstr(json, needle);
  if (key_pos == NULL)
  {
    return false;
  }

  const char *colon = strchr(key_pos, ':');
  if (colon == NULL)
  {
    return false;
  }

  const char *start_quote = strchr(colon, '"');
  if (start_quote == NULL)
  {
    return false;
  }

  start_quote++;
  const char *end_quote = strchr(start_quote, '"');
  if (end_quote == NULL)
  {
    return false;
  }

  size_t len = (size_t)(end_quote - start_quote);
  if (len == 0 || len >= out_size)
  {
    return false;
  }

  memcpy(out, start_quote, len);
  out[len] = '\0';
  return true;
}

static bool parse_json_float_field(const char *json, const char *key, float *out)
{
  if (json == NULL || key == NULL || out == NULL)
  {
    return false;
  }

  char needle[48];
  int needle_len = snprintf(needle, sizeof(needle), "\"%s\"", key);
  if (needle_len <= 0 || needle_len >= (int)sizeof(needle))
  {
    return false;
  }

  const char *key_pos = strstr(json, needle);
  if (key_pos == NULL)
  {
    return false;
  }

  const char *colon = strchr(key_pos, ':');
  if (colon == NULL)
  {
    return false;
  }

  char *end_ptr = NULL;
  float value = strtof(colon + 1, &end_ptr);
  if (end_ptr == (colon + 1))
  {
    return false;
  }

  *out = value;
  return true;
}

static esp_err_t http_server_sensor_update_handler(httpd_req_t *req)
{
  if (req->content_len <= 0 || req->content_len >= SENSOR_UPDATE_MAX_BODY_LEN)
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid sensor update payload size");
    return ESP_FAIL;
  }

  char body[SENSOR_UPDATE_MAX_BODY_LEN];
  int recv_len = httpd_req_recv(req, body, req->content_len);
  if (recv_len <= 0)
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read sensor update payload");
    return ESP_FAIL;
  }
  body[recv_len] = '\0';

  float temperature = 0.0f;
  float humidity = 0.0f;
  float soil_moisture = 0.0f;
  char device_id[64] = "unknown";

  // device_id is optional for backward compatibility with old clients.
  parse_json_string_field(body, "device_id", device_id, sizeof(device_id));

  bool ok = parse_json_float_field(body, "temperature", &temperature) &&
            parse_json_float_field(body, "humidity", &humidity) &&
            parse_json_float_field(body, "soil_moisture", &soil_moisture);
  if (!ok)
  {
    ESP_LOGW(TAG, "Invalid sensor update JSON: %s", body);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid sensor fields");
    return ESP_FAIL;
  }

  esp_err_t err = sensor_cache_update_snapshot(device_id, temperature, humidity, soil_moisture);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to update sensor cache: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update sensor cache");
    return ESP_FAIL;
  }

  app_send_message(APP_MSG_SENSOR_DATA_RECEIVED);

  ESP_LOGI(TAG, "Sensor update received device=%s temp=%.2f humidity=%.2f soil=%.2f",
           device_id, temperature, humidity, soil_moisture);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

  char json_response[112];
  int written = snprintf(json_response, sizeof(json_response),
                         "{\"status\":\"ok\",\"device_id\":\"%s\"}", device_id);
  if (written < 0 || written >= (int)sizeof(json_response))
  {
    return httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
  }
  return httpd_resp_sendstr(req, json_response);
}

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
  bool sensor_data_available = http_server_monitor_is_sensor_data_available();
  uint8_t online_node_count = http_server_monitor_online_node_count();
  uint8_t registered_node_count = http_server_monitor_registered_node_count();
  uint8_t web_connected_count = web_session_get_connected_count();

  sensor_device_snapshot_t device_snapshots[SENSOR_CACHE_MAX_DEVICES];
  size_t device_snapshot_count = 0;
  if (!sensor_cache_get_device_snapshots(device_snapshots,
                                         SENSOR_CACHE_MAX_DEVICES,
                                         &device_snapshot_count))
  {
    device_snapshot_count = 0;
  }

  char time_buf[32];
  time_sync_get_local_time(time_buf, sizeof(time_buf));

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

  char chunk[384];
  int written = snprintf(
      chunk,
      sizeof(chunk),
      "{\"time\":\"%s\",\"temp\":%.2f,\"humidity\":%.2f,\"soil-moisture\":%.2f,\"node-snapshots\":[",
      time_buf,
      snapshot.temperature,
      snapshot.humidity,
      snapshot.soilMoisture);
  if (written < 0 || written >= (int)sizeof(chunk))
  {
    ESP_LOGE(TAG, "Failed to encode status JSON prefix");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }
  if (httpd_resp_send_chunk(req, chunk, HTTPD_RESP_USE_STRLEN) != ESP_OK)
  {
    return ESP_FAIL;
  }

  for (size_t i = 0; i < device_snapshot_count; i++)
  {
    written = snprintf(
        chunk,
        sizeof(chunk),
        "%s{\"device_id\":\"%s\",\"temp\":%.2f,\"humidity\":%.2f,\"soil-moisture\":%.2f}",
        (i == 0) ? "" : ",",
        device_snapshots[i].device_id,
        device_snapshots[i].temperature,
        device_snapshots[i].humidity,
        device_snapshots[i].soilMoisture);
    if (written < 0 || written >= (int)sizeof(chunk))
    {
      ESP_LOGE(TAG, "Failed to encode node snapshot JSON");
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
      return ESP_FAIL;
    }
    if (httpd_resp_send_chunk(req, chunk, HTTPD_RESP_USE_STRLEN) != ESP_OK)
    {
      return ESP_FAIL;
    }
  }

  written = snprintf(
      chunk,
      sizeof(chunk),
      "],\"min-moiture-level\":%u,\"max-moiture-level\":%u,\"duration\":%u,\"water-status\":\"OFF\",\"wifi-connect-status\":%u,\"relay-status\":\"%s\",\"sensor-data-available\":%s,\"online-node-count\":%u,\"registered-node-count\":%u,\"web-connected\":%u}",
      (unsigned int)water_cfg.min_moisture_level,
      (unsigned int)water_cfg.max_moisture_level,
      (unsigned int)water_cfg.duration_minutes,
      (unsigned int)wifi_connect_status,
      relay_status,
      sensor_data_available ? "true" : "false",
      (unsigned int)online_node_count,
      (unsigned int)registered_node_count,
      (unsigned int)web_connected_count);
  if (written < 0 || written >= (int)sizeof(chunk))
  {
    ESP_LOGE(TAG, "Failed to encode status JSON suffix");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }
  if (httpd_resp_send_chunk(req, chunk, HTTPD_RESP_USE_STRLEN) != ESP_OK)
  {
    return ESP_FAIL;
  }

  return httpd_resp_send_chunk(req, NULL, 0);
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

static esp_err_t http_server_web_session_heartbeat_handler(httpd_req_t *req)
{
  char session_id[WEB_SESSION_ID_MAX_LEN];
  if (web_session_get_header_id(req, session_id, sizeof(session_id)) != ESP_OK)
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing X-Web-Session-Id header");
    return ESP_FAIL;
  }

  web_session_touch(session_id);

  uint8_t web_connected_count = web_session_get_connected_count();
  char json_response[96];
  int written = snprintf(json_response,
                         sizeof(json_response),
                         "{\"status\":\"ok\",\"web-connected\":%u}",
                         (unsigned int)web_connected_count);
  if (written < 0 || written >= (int)sizeof(json_response))
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_client_info_handler(httpd_req_t *req)
{
  uint8_t online_node_count = http_server_monitor_online_node_count();
  uint8_t registered_node_count = http_server_monitor_registered_node_count();
  uint8_t web_connected_count = web_session_get_connected_count();

  char json_response[128];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"online-nodes\":%u,\"registered-nodes\":%u,\"web-connected\":%u,\"web-total\":%u}",
      (unsigned int)online_node_count,
      (unsigned int)registered_node_count,
      (unsigned int)web_connected_count,
      (unsigned int)web_connected_count);

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
  err = httpd_register_uri_handler(server, &local_time);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t sensor_update = {
      .uri = "/sensor-update",
      .method = HTTP_POST,
      .handler = http_server_sensor_update_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &sensor_update);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t client_info = {
      .uri = "/clientInfo.json",
      .method = HTTP_GET,
      .handler = http_server_client_info_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &client_info);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t web_session_heartbeat = {
      .uri = "/webSession.json",
      .method = HTTP_POST,
      .handler = http_server_web_session_heartbeat_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &web_session_heartbeat);
}