#include "http_server.h"

#include "esp_log.h"
#include "http_server_monitor.h"

static const char TAG[] = "http_server";
static httpd_handle_t s_server = NULL;

esp_err_t http_server_start(void)
{
  if (s_server != NULL)
  {
    ESP_LOGI(TAG, "HTTP server already running");
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 16;

  ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
  esp_err_t err = httpd_start(&s_server, &config);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
    return err;
  }

  err = http_server_register_static_handlers(s_server);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register static handlers: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  err = http_server_register_status_handlers(s_server);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register status handlers: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  err = http_server_register_relay_handlers(s_server);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register relay handlers: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  err = http_server_register_wifi_handlers(s_server);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register WiFi handlers: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  err = http_server_monitor_start();
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to start HTTP monitor task: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}

bool http_server_is_running(void)
{
  return (s_server != NULL);
}
