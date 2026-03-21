#include "http_server.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "http_server_monitor.h"

#define OTA_RECV_BUFFER_SIZE 4096

static const char TAG[] = "http_server_ota";
static volatile int s_fw_update_status = 0;

static void http_server_ota_set_status(int status)
{
  s_fw_update_status = status;
}

static esp_err_t http_server_ota_update_handler(httpd_req_t *req)
{
  if (req->content_len <= 0)
  {
    http_server_ota_set_status(-1);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty OTA payload");
    return ESP_FAIL;
  }

  const esp_partition_t *running_partition = esp_ota_get_running_partition();
  const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
  if (update_partition == NULL || running_partition == NULL ||
      update_partition->address == running_partition->address)
  {
    ESP_LOGE(TAG, "OTA not supported by current partition table");
    http_server_ota_set_status(-1);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA partition not available");
    return ESP_FAIL;
  }

  if ((size_t)req->content_len > update_partition->size)
  {
    ESP_LOGE(TAG, "Firmware image (%d bytes) exceeds OTA partition size (%lu bytes)",
             req->content_len, (unsigned long)update_partition->size);
    http_server_ota_set_status(-1);
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Firmware too large for OTA partition");
    return ESP_FAIL;
  }

  esp_ota_handle_t ota_handle = 0;
  esp_err_t err = esp_ota_begin(update_partition, req->content_len, &ota_handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
    http_server_ota_set_status(-1);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to begin OTA");
    return ESP_FAIL;
  }

  char ota_buff[OTA_RECV_BUFFER_SIZE];
  int total_received = 0;

  while (total_received < req->content_len)
  {
    int bytes_to_read = req->content_len - total_received;
    if (bytes_to_read > (int)sizeof(ota_buff))
    {
      bytes_to_read = sizeof(ota_buff);
    }

    int recv_len = httpd_req_recv(req, ota_buff, bytes_to_read);
    if (recv_len == HTTPD_SOCK_ERR_TIMEOUT)
    {
      continue;
    }
    if (recv_len <= 0)
    {
      ESP_LOGE(TAG, "OTA recv failed: %d", recv_len);
      esp_ota_abort(ota_handle);
      http_server_ota_set_status(-1);
      http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive OTA data");
      return ESP_FAIL;
    }

    err = esp_ota_write(ota_handle, ota_buff, recv_len);
    if (err != ESP_OK)
    {
      ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
      esp_ota_abort(ota_handle);
      http_server_ota_set_status(-1);
      http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write OTA data");
      return ESP_FAIL;
    }

    total_received += recv_len;
  }

  err = esp_ota_end(ota_handle);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
    http_server_ota_set_status(-1);
    http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to finalize OTA image");
    return ESP_FAIL;
  }

  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
    http_server_ota_set_status(-1);
    http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_FAILED);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set boot partition");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "OTA image written successfully to subtype %d at offset 0x%lx",
           update_partition->subtype, update_partition->address);

  http_server_ota_set_status(1);
  http_server_monitor_send_message(HTTP_MSG_OTA_UPDATE_SUCCESSFUL);

  httpd_resp_set_type(req, "application/json");
  httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
  return ESP_OK;
}

static esp_err_t http_server_ota_status_handler(httpd_req_t *req)
{
  char ota_json[128];
  int written = snprintf(
      ota_json,
      sizeof(ota_json),
      // __TIME__ and __DATE__ are compiler-provided build timestamp macros.
      "{\"ota_update_status\":%d,\"compile_time\":\"%s\",\"compile_date\":\"%s\"}",
      s_fw_update_status,
      __TIME__,
      __DATE__);

  if (written < 0 || written >= (int)sizeof(ota_json))
  {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encoding error");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
  return httpd_resp_send(req, ota_json, HTTPD_RESP_USE_STRLEN);
}

esp_err_t http_server_register_ota_handlers(httpd_handle_t server)
{
  httpd_uri_t ota_update = {
      .uri = "/OTAupdate",
      .method = HTTP_POST,
      .handler = http_server_ota_update_handler,
      .user_ctx = NULL,
  };
  esp_err_t err = httpd_register_uri_handler(server, &ota_update);
  if (err != ESP_OK)
  {
    return err;
  }

  httpd_uri_t ota_status = {
      .uri = "/OTAstatus",
      .method = HTTP_POST,
      .handler = http_server_ota_status_handler,
      .user_ctx = NULL,
  };
  return httpd_register_uri_handler(server, &ota_status);
}
