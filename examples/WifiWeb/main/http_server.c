#include "http_server.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const char TAG[] = "http_server";
static httpd_handle_t s_server = NULL;

static esp_err_t root_get_handler(httpd_req_t *req)
{
  const char *response = "ESP32 HTTP server is running\n";
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_start(void)
{
  if (s_server != NULL)
  {
    ESP_LOGI(TAG, "HTTP server already running");
    return ESP_OK;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
  esp_err_t err = httpd_start(&s_server, &config);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(err));
    return err;
  }

  httpd_uri_t root_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = root_get_handler,
      .user_ctx = NULL,
  };

  err = httpd_register_uri_handler(s_server, &root_uri);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register root URI: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}
