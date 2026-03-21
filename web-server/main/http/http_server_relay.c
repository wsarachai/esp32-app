#include "http_server.h"

#include <stdio.h>

#include "esp_log.h"
#include "relay.h"

static const char TAG[] = "http_server_relay";

static esp_err_t http_server_relay_status_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "Relay status requested");

  bool state  = relay_get_state();
  bool manual = relay_is_manual_override();

  char json_response[72];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"relay_status\":\"%s\",\"manual_override\":%s}",
      state  ? "ON"   : "OFF",
      manual ? "true" : "false");

  if (written < 0 || written >= (int)sizeof(json_response))
  {
    ESP_LOGE(TAG, "Failed to create relay status JSON response");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_relay_control_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "Relay control requested");

  char relay_control_header[16] = {0};
  size_t relay_control_len = httpd_req_get_hdr_value_len(req, "relay-control");

  if (relay_control_len == 0 || relay_control_len >= sizeof(relay_control_header))
  {
    ESP_LOGE(TAG, "Missing or invalid relay-control header");
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing relay-control header");
    return ESP_FAIL;
  }

  httpd_req_get_hdr_value_str(req, "relay-control", relay_control_header, relay_control_len + 1);

  bool relay_enabled;
  bool manual;

  if (relay_control_header[0] == 'a')
  {
    // "auto" — release manual override; let the irrigation controller take over.
    relay_set_manual_override(false);
    relay_enabled = relay_get_state();
    manual = false;
    ESP_LOGI(TAG, "Manual override released; relay remains %s", relay_enabled ? "ON" : "OFF");
  }
  else
  {
    // "1" or "0" — explicit manual control, override takes effect immediately.
    relay_enabled = (relay_control_header[0] == '1');
    relay_set_manual_override(true);
    esp_err_t err = relay_set(relay_enabled);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "Failed to set relay: %s", esp_err_to_name(err));
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to control relay");
      return ESP_FAIL;
    }
    manual = true;
  }

  char json_response[72];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"relay_status\":\"%s\",\"manual_override\":%s}",
      relay_enabled ? "ON"   : "OFF",
      manual        ? "true" : "false");

  if (written < 0 || written >= (int)sizeof(json_response))
  {
    ESP_LOGE(TAG, "Failed to create relay control JSON response");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_register_relay_handlers(httpd_handle_t server)
{
  httpd_uri_t relay_status = {
      .uri = "/relayStatus.json",
      .method = HTTP_GET,
      .handler = http_server_relay_status_handler,
      .user_ctx = NULL,
  };
  esp_err_t err = httpd_register_uri_handler(server, &relay_status);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t relay_control = {
      .uri = "/relayControl.json",
      .method = HTTP_POST,
      .handler = http_server_relay_control_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &relay_control);
}