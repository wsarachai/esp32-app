#include "http_server.h"

#include <stdio.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "main.h"
#include "wifi_app.h"

static const char TAG[] = "http_server_wifi";

static esp_err_t http_server_ap_ssid_handler(httpd_req_t *req)
{
  char json_response[96];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"ssid\":\"%s\"}",
      WIFI_AP_SSID);

  if (written < 0 || written >= (int)sizeof(json_response))
  {
    ESP_LOGE(TAG, "Failed to create AP SSID JSON response");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_wifi_connect_handler(httpd_req_t *req)
{
  char ssid[MAX_SSID_LENGTH + 1] = {0};
  char password[MAX_PASSWORD_LENGTH + 1] = {0};

  size_t ssid_len = httpd_req_get_hdr_value_len(req, "my-connect-ssid");
  size_t pwd_len = httpd_req_get_hdr_value_len(req, "my-connect-pwd");
  if (ssid_len == 0 || pwd_len == 0 || ssid_len > MAX_SSID_LENGTH || pwd_len > MAX_PASSWORD_LENGTH)
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing or invalid WiFi credentials");
    return ESP_FAIL;
  }

  if (httpd_req_get_hdr_value_str(req, "my-connect-ssid", ssid, sizeof(ssid)) != ESP_OK ||
      httpd_req_get_hdr_value_str(req, "my-connect-pwd", password, sizeof(password)) != ESP_OK)
  {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read WiFi credentials");
    return ESP_FAIL;
  }

  esp_err_t err = wifi_app_set_sta_creds(ssid, password);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "Invalid WiFi credentials: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid WiFi credentials");
    return ESP_FAIL;
  }

  if (app_send_message(WIFI_APP_MSG_CONNECTING_FROM_HTTP_SERVER) != pdPASS)
  {
    ESP_LOGE(TAG, "Failed to queue connect event");
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to queue WiFi connection task");
    return ESP_FAIL;
  }

  char json_response[96];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"status\":\"connecting\",\"ssid\":\"%s\"}",
      ssid);
  if (written < 0 || written >= (int)sizeof(json_response))
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_wifi_connect_status_handler(httpd_req_t *req)
{
  uint8_t status = wifi_app_get_sta_connect_status();

  char json_response[64];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"wifi_connect_status\":%u}",
      (unsigned int)status);
  if (written < 0 || written >= (int)sizeof(json_response))
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t http_server_wifi_connect_info_handler(httpd_req_t *req)
{
  char ap_ssid[MAX_SSID_LENGTH + 1] = "Not Connected";
  char ip_str[16] = "0.0.0.0";
  char netmask_str[16] = "0.0.0.0";
  char gw_str[16] = "0.0.0.0";

  wifi_ap_record_t ap_record;
  if (esp_wifi_sta_get_ap_info(&ap_record) == ESP_OK)
  {
    snprintf(ap_ssid, sizeof(ap_ssid), "%s", (const char *)ap_record.ssid);
  }

  esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (sta_netif != NULL)
  {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK)
    {
      snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
      snprintf(netmask_str, sizeof(netmask_str), IPSTR, IP2STR(&ip_info.netmask));
      snprintf(gw_str, sizeof(gw_str), IPSTR, IP2STR(&ip_info.gw));
    }
  }

  char json_response[192];
  int written = snprintf(
      json_response,
      sizeof(json_response),
      "{\"ap\":\"%s\",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\"}",
      ap_ssid,
      ip_str,
      netmask_str,
      gw_str);
  if (written < 0 || written >= (int)sizeof(json_response))
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_register_wifi_handlers(httpd_handle_t server)
{
  httpd_uri_t ap_ssid = {
      .uri = "/apSSID.json",
      .method = HTTP_GET,
      .handler = http_server_ap_ssid_handler,
      .user_ctx = NULL,
  };
  esp_err_t err = httpd_register_uri_handler(server, &ap_ssid);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t wifi_connect = {
      .uri = "/wifiConnect.json",
      .method = HTTP_POST,
      .handler = http_server_wifi_connect_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &wifi_connect);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t wifi_connect_status = {
      .uri = "/wifiConnectStatus",
      .method = HTTP_POST,
      .handler = http_server_wifi_connect_status_handler,
      .user_ctx = NULL,
  };
  err = httpd_register_uri_handler(server, &wifi_connect_status);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t wifi_connect_info = {
      .uri = "/wifiConnectInfo.json",
      .method = HTTP_GET,
      .handler = http_server_wifi_connect_info_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &wifi_connect_info);
}