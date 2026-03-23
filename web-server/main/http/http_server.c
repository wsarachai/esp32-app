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
  config.max_uri_handlers = 24;
  config.max_open_sockets = 7;
  config.backlog_conn = 8;
  config.lru_purge_enable = true;
  config.recv_wait_timeout = 3;
  config.send_wait_timeout = 8;
  config.keep_alive_enable = false;

  ESP_LOGI(TAG,
           "Starting HTTP server on port %d (max_sockets=%u backlog=%u lru_purge=%s recv_timeout=%us send_timeout=%us keep_alive=%s)",
           config.server_port,
           (unsigned int)config.max_open_sockets,
           (unsigned int)config.backlog_conn,
           config.lru_purge_enable ? "true" : "false",
           (unsigned int)config.recv_wait_timeout,
           (unsigned int)config.send_wait_timeout,
           config.keep_alive_enable ? "true" : "false");
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

  err = http_server_register_ota_handlers(s_server);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register OTA handlers: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  err = http_server_monitor_start();
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "HTTP monitor task failed to start: %s", esp_err_to_name(err));
    ESP_LOGW(TAG, "Continuing with HTTP server running; monitor is optional");
  }

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}

bool http_server_is_running(void)
{
  return (s_server != NULL);
}
