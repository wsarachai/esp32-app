#include "http_server.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const char TAG[] = "http_server";
static httpd_handle_t s_server = NULL;

// Embedded files: JQuery, index.html, app.css, app.js and favicon.ico files
extern const uint8_t app_css_start[] asm("_binary_app_css_start");
extern const uint8_t app_css_end[] asm("_binary_app_css_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const uint8_t favicon_ico_end[] asm("_binary_favicon_ico_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t jquery_3_3_1_min_js_start[] asm("_binary_jquery_3_3_1_min_js_start");
extern const uint8_t jquery_3_3_1_min_js_end[] asm("_binary_jquery_3_3_1_min_js_end");

/**
 * app.css get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "app.css requested");

  httpd_resp_set_type(req, "text/css");
  httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

  return ESP_OK;
}

/**
 * app.js get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "app.js requested");

  httpd_resp_set_type(req, "application/javascript");
  httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

  return ESP_OK;
}

/**
 * Sends the .ico (icon) file when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "favicon.ico requested");

  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

  return ESP_OK;
}

/**
 * Sends the index.html page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "index.html requested");

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

  return ESP_OK;
}

/**
 * Jquery get handler is requested when accessing the web page.
 * @param req HTTP request for which the uri needs to be handled.
 * @return ESP_OK
 */
static esp_err_t http_server_jquery_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "Jquery requested");

  httpd_resp_set_type(req, "application/javascript");
  httpd_resp_send(req, (const char *)jquery_3_3_1_min_js_start, jquery_3_3_1_min_js_end - jquery_3_3_1_min_js_start);

  return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
  return http_server_index_html_handler(req);
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

  // register query handler
  httpd_uri_t jquery_js = {
      .uri = "/jquery-3.3.1.min.js",
      .method = HTTP_GET,
      .handler = http_server_jquery_handler,
      .user_ctx = NULL};
  err = httpd_register_uri_handler(s_server, &jquery_js);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register jQuery URI: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  // register app.css handler
  httpd_uri_t app_css = {
      .uri = "/app.css",
      .method = HTTP_GET,
      .handler = http_server_app_css_handler,
      .user_ctx = NULL};
  err = httpd_register_uri_handler(s_server, &app_css);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register app.css URI: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  // register app.js handler
  httpd_uri_t app_js = {
      .uri = "/app.js",
      .method = HTTP_GET,
      .handler = http_server_app_js_handler,
      .user_ctx = NULL};
  err = httpd_register_uri_handler(s_server, &app_js);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register app.js URI: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  // register favicon.ico handler
  httpd_uri_t favicon_ico = {
      .uri = "/favicon.ico",
      .method = HTTP_GET,
      .handler = http_server_favicon_ico_handler,
      .user_ctx = NULL};
  err = httpd_register_uri_handler(s_server, &favicon_ico);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Failed to register favicon.ico URI: %s", esp_err_to_name(err));
    httpd_stop(s_server);
    s_server = NULL;
    return err;
  }

  ESP_LOGI(TAG, "HTTP server started");
  return ESP_OK;
}
