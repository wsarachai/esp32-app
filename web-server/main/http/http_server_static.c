#include "http_server.h"

#include "esp_log.h"

static const char TAG[] = "http_server_static";

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

static esp_err_t http_server_app_css_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "app.css requested");

  httpd_resp_set_type(req, "text/css");
  httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start);

  return ESP_OK;
}

static esp_err_t http_server_app_js_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "app.js requested");

  httpd_resp_set_type(req, "application/javascript");
  httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start);

  return ESP_OK;
}

static esp_err_t http_server_favicon_ico_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "favicon.ico requested");

  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);

  return ESP_OK;
}

static esp_err_t http_server_index_html_handler(httpd_req_t *req)
{
  ESP_LOGI(TAG, "index.html requested");

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);

  return ESP_OK;
}

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

static esp_err_t http_server_android_generate_204_handler(httpd_req_t *req)
{
  httpd_resp_set_status(req, "204 No Content");
  return httpd_resp_send(req, NULL, 0);
}

static esp_err_t http_server_ncsi_txt_handler(httpd_req_t *req)
{
  static const char kWindowsNcsi[] = "Microsoft NCSI";
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, kWindowsNcsi, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_connecttest_txt_handler(httpd_req_t *req)
{
  static const char kWindowsConnectTest[] = "Microsoft Connect Test";
  httpd_resp_set_type(req, "text/plain");
  return httpd_resp_send(req, kWindowsConnectTest, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_hotspot_detect_handler(httpd_req_t *req)
{
  static const char kAppleHotspotDetect[] = "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, kAppleHotspotDetect, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_register_static_handlers(httpd_handle_t server)
{
  httpd_uri_t root_uri = {
      .uri = "/",
      .method = HTTP_GET,
      .handler = root_get_handler,
      .user_ctx = NULL,
  };
  esp_err_t err = httpd_register_uri_handler(server, &root_uri);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t jquery_js = {
      .uri = "/jquery-3.3.1.min.js",
      .method = HTTP_GET,
      .handler = http_server_jquery_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &jquery_js);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t app_css = {
      .uri = "/app.css",
      .method = HTTP_GET,
      .handler = http_server_app_css_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &app_css);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t app_js = {
      .uri = "/app.js",
      .method = HTTP_GET,
      .handler = http_server_app_js_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &app_js);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t favicon_ico = {
      .uri = "/favicon.ico",
      .method = HTTP_GET,
      .handler = http_server_favicon_ico_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &favicon_ico);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t android_generate_204 = {
      .uri = "/generate_204",
      .method = HTTP_GET,
      .handler = http_server_android_generate_204_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &android_generate_204);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t android_gen_204 = {
      .uri = "/gen_204",
      .method = HTTP_GET,
      .handler = http_server_android_generate_204_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &android_gen_204);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t windows_ncsi = {
      .uri = "/ncsi.txt",
      .method = HTTP_GET,
      .handler = http_server_ncsi_txt_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &windows_ncsi);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t windows_connecttest = {
      .uri = "/connecttest.txt",
      .method = HTTP_GET,
      .handler = http_server_connecttest_txt_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &windows_connecttest);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t apple_hotspot = {
      .uri = "/hotspot-detect.html",
      .method = HTTP_GET,
      .handler = http_server_hotspot_detect_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &apple_hotspot);
}