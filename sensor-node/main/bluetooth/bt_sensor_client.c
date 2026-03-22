#include "bt_sensor_client.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_mac.h"
#include "main.h"
#include "nvs_flash.h"

#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "bt_sensor_client";

#define BT_INVALID_CONN_HANDLE 0xFFFF
#define BT_CONNECT_TIMEOUT_MS 30000

static uint8_t s_own_addr_type;
static uint16_t s_conn_handle = BT_INVALID_CONN_HANDLE;
static uint16_t s_sensor_data_handle = 0;
static uint16_t s_service_start_handle = 0;
static uint16_t s_service_end_handle = 0;
static bool s_connected = false;
static bool s_connecting = false;
static bool s_started = false;
static bool s_connect_after_scan = false;
static ble_addr_t s_pending_addr;
static char s_device_id[sizeof(((bt_sensor_payload_t *)0)->device_id)] = {0};
static bool s_device_id_initialized = false;

static int bt_gap_event_handler(struct ble_gap_event *event, void *arg);
static void bt_scan_start(void);

static void bt_reset_connection_state(void)
{
  s_connected = false;
  s_connecting = false;
  s_conn_handle = BT_INVALID_CONN_HANDLE;
  s_sensor_data_handle = 0;
  s_service_start_handle = 0;
  s_service_end_handle = 0;
}

static const char *bt_sensor_client_get_device_id(void)
{
  if (s_device_id_initialized)
  {
    return s_device_id;
  }

  uint8_t base_mac[6] = {0};
  esp_err_t err = esp_efuse_mac_get_default(base_mac);
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "Failed to read base MAC: %s", esp_err_to_name(err));
    snprintf(s_device_id, sizeof(s_device_id), "esp32-unknown");
  }
  else
  {
    snprintf(s_device_id,
             sizeof(s_device_id),
             "esp32-%02X%02X%02X",
             base_mac[3],
             base_mac[4],
             base_mac[5]);
  }

  s_device_id_initialized = true;
  ESP_LOGI(TAG, "Device ID: %s", s_device_id);
  return s_device_id;
}

static bool bt_adv_name_matches(const struct ble_hs_adv_fields *fields)
{
  size_t expected_len = strlen(BT_SENSOR_SERVER_NAME);

  if (fields->name == NULL || fields->name_len != expected_len)
  {
    return false;
  }

  return memcmp(fields->name, BT_SENSOR_SERVER_NAME, expected_len) == 0;
}

static bool bt_adv_service_matches(const struct ble_hs_adv_fields *fields)
{
  if (fields->uuids16 == NULL)
  {
    return false;
  }

  for (uint8_t index = 0; index < fields->num_uuids16; ++index)
  {
    if (fields->uuids16[index].value == BT_SENSOR_SVC_UUID)
    {
      return true;
    }
  }

  return false;
}

static int bt_char_disc_cb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr,
                           void *arg)
{
  (void)arg;

  if (error->status != 0 && error->status != BLE_HS_EDONE)
  {
    ESP_LOGE(TAG, "Characteristic discovery failed: status=%d", error->status);
    ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return 0;
  }

  if (error->status == BLE_HS_EDONE)
  {
    if (s_sensor_data_handle == 0)
    {
      ESP_LOGE(TAG, "Sensor data characteristic not found");
      ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    else
    {
      ESP_LOGI(TAG,
               "Bluetooth transport ready: conn_handle=%u value_handle=%u",
               conn_handle,
               s_sensor_data_handle);
    }
    return 0;
  }

  s_sensor_data_handle = chr->val_handle;
  ESP_LOGI(TAG,
           "Discovered sensor data characteristic: def_handle=%u value_handle=%u",
           chr->def_handle,
           chr->val_handle);
  return 0;
}

static int bt_service_disc_cb(uint16_t conn_handle,
                              const struct ble_gatt_error *error,
                              const struct ble_gatt_svc *service,
                              void *arg)
{
  (void)arg;

  if (error->status != 0 && error->status != BLE_HS_EDONE)
  {
    ESP_LOGE(TAG, "Service discovery failed: status=%d", error->status);
    ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    return 0;
  }

  if (error->status == BLE_HS_EDONE)
  {
    if (s_service_start_handle == 0 || s_service_end_handle == 0)
    {
      ESP_LOGE(TAG, "Sensor service 0x%04X not found", BT_SENSOR_SVC_UUID);
      ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
      return 0;
    }

    int rc = ble_gattc_disc_chrs_by_uuid(conn_handle,
                                         s_service_start_handle,
                                         s_service_end_handle,
                                         BLE_UUID16_DECLARE(BT_SENSOR_DATA_UUID),
                                         bt_char_disc_cb,
                                         NULL);
    if (rc != 0)
    {
      ESP_LOGE(TAG, "Characteristic discovery start failed: rc=%d", rc);
      ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
  }

  s_service_start_handle = service->start_handle;
  s_service_end_handle = service->end_handle;
  ESP_LOGI(TAG,
           "Discovered sensor service: start_handle=%u end_handle=%u",
           service->start_handle,
           service->end_handle);
  return 0;
}

static void bt_scan_start(void)
{
  if (!s_started || s_connected || s_connecting || s_connect_after_scan)
  {
    return;
  }

  struct ble_gap_disc_params disc_params = {0};
  disc_params.filter_duplicates = 1;
  disc_params.passive = 0;
  disc_params.itvl = 0;
  disc_params.window = 0;

  int rc = ble_gap_disc(s_own_addr_type, BLE_HS_FOREVER, &disc_params, bt_gap_event_handler, NULL);
  if (rc != 0 && rc != BLE_HS_EALREADY)
  {
    ESP_LOGE(TAG, "ble_gap_disc failed: rc=%d", rc);
    return;
  }

  if (rc == BLE_HS_EALREADY)
  {
    ESP_LOGD(TAG, "Bluetooth scan already active");
  }
  else
  {
    ESP_LOGI(TAG, "Scanning for Bluetooth server '%s'", BT_SENSOR_SERVER_NAME);
  }
}

static void bt_connect_pending_peer(void)
{
  if (!s_connect_after_scan || s_connected || s_connecting)
  {
    return;
  }

  s_connect_after_scan = false;
  s_connecting = true;

  int rc = ble_gap_connect(s_own_addr_type,
                           &s_pending_addr,
                           BT_CONNECT_TIMEOUT_MS,
                           NULL,
                           bt_gap_event_handler,
                           NULL);
  if (rc != 0)
  {
    s_connecting = false;
    ESP_LOGE(TAG, "ble_gap_connect failed: rc=%d", rc);
    bt_scan_start();
    return;
  }

  ESP_LOGI(TAG, "Connecting to Bluetooth server");
}

static int bt_gap_event_handler(struct ble_gap_event *event, void *arg)
{
  (void)arg;

  switch (event->type)
  {
  case BLE_GAP_EVENT_DISC:
  {
    if (s_connected || s_connecting || s_connect_after_scan)
    {
      return 0;
    }

    struct ble_hs_adv_fields fields = {0};
    int rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
    if (rc != 0)
    {
      ESP_LOGW(TAG, "Failed to parse advertisement fields: rc=%d", rc);
      return 0;
    }

    bool service_match = bt_adv_service_matches(&fields);
    bool name_match = bt_adv_name_matches(&fields);
    if (!service_match && !name_match)
    {
      return 0;
    }

    memcpy(&s_pending_addr, &event->disc.addr, sizeof(s_pending_addr));
    s_connect_after_scan = true;
    ESP_LOGI(TAG,
             "Found Bluetooth server advertisement (service_match=%d name_match=%d)",
             service_match,
             name_match);
    ble_gap_disc_cancel();
    return 0;
  }

  case BLE_GAP_EVENT_DISC_COMPLETE:
    if (s_connect_after_scan)
    {
      bt_connect_pending_peer();
    }
    else if (!s_connected && !s_connecting)
    {
      bt_scan_start();
    }
    return 0;

  case BLE_GAP_EVENT_CONNECT:
    s_connecting = false;

    if (event->connect.status != 0)
    {
      ESP_LOGW(TAG, "Bluetooth connect failed: status=%d", event->connect.status);
      bt_reset_connection_state();
      bt_scan_start();
      return 0;
    }

    s_connected = true;
    s_conn_handle = event->connect.conn_handle;
    s_sensor_data_handle = 0;
    s_service_start_handle = 0;
    s_service_end_handle = 0;

    app_send_message(APP_MSG_BT_CONNECTED);
    ESP_LOGI(TAG, "Bluetooth connected; conn_handle=%u", s_conn_handle);

    int rc = ble_gattc_disc_svc_by_uuid(s_conn_handle,
                                        BLE_UUID16_DECLARE(BT_SENSOR_SVC_UUID),
                                        bt_service_disc_cb,
                                        NULL);
    if (rc != 0)
    {
      ESP_LOGE(TAG, "Service discovery start failed: rc=%d", rc);
      ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGW(TAG, "Bluetooth disconnected; reason=%d", event->disconnect.reason);
    bt_reset_connection_state();
    app_send_message(APP_MSG_BT_DISCONNECTED);
    bt_scan_start();
    return 0;

  default:
    return 0;
  }
}

static void bt_host_on_sync(void)
{
  int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: rc=%d", rc);
    return;
  }

  bt_reset_connection_state();
  bt_scan_start();
}

static void bt_host_on_reset(int reason)
{
  ESP_LOGW(TAG, "Bluetooth host reset; reason=%d", reason);
  bt_reset_connection_state();
}

static void nimble_host_task(void *param)
{
  (void)param;
  ESP_LOGI(TAG, "NimBLE host task started");
  nimble_port_run();
  nimble_port_freertos_deinit();
}

esp_err_t bt_sensor_client_start(void)
{
  if (s_started)
  {
    return ESP_OK;
  }

  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = nimble_port_init();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "nimble_port_init failed: %s", esp_err_to_name(ret));
    return ret;
  }

  ble_hs_cfg.sync_cb = bt_host_on_sync;
  ble_hs_cfg.reset_cb = bt_host_on_reset;

  ble_svc_gap_init();
  ble_svc_gap_device_name_set("MJU-ESP32-Sensor-Node");

  bt_sensor_client_get_device_id();
  bt_reset_connection_state();
  s_started = true;

  nimble_port_freertos_init(nimble_host_task);
  ESP_LOGI(TAG,
           "Bluetooth sensor client initialised (service 0x%04X, char 0x%04X)",
           BT_SENSOR_SVC_UUID,
           BT_SENSOR_DATA_UUID);

  return ESP_OK;
}

bool bt_sensor_client_is_connected(void)
{
  return s_connected && s_sensor_data_handle != 0 && s_conn_handle != BT_INVALID_CONN_HANDLE;
}

void bt_sensor_client_maintain(void)
{
  if (!s_started || bt_sensor_client_is_connected() || s_connecting || s_connect_after_scan)
  {
    return;
  }

  bt_scan_start();
}

esp_err_t bt_sensor_client_send(float temperature, float humidity, float soil_moisture)
{
  if (!bt_sensor_client_is_connected())
  {
    return ESP_ERR_INVALID_STATE;
  }

  bt_sensor_payload_t payload = {0};
  snprintf(payload.device_id, sizeof(payload.device_id), "%s", bt_sensor_client_get_device_id());
  payload.temperature = temperature;
  payload.humidity = humidity;
  payload.soil_moisture = soil_moisture;

  int rc = ble_gattc_write_no_rsp_flat(s_conn_handle,
                                       s_sensor_data_handle,
                                       &payload,
                                       sizeof(payload));
  if (rc != 0)
  {
    ESP_LOGE(TAG, "ble_gattc_write_no_rsp_flat failed: rc=%d", rc);
    return ESP_FAIL;
  }

  ESP_LOGI(TAG,
           "Bluetooth send: id=%s temp=%.2f humidity=%.2f soil=%.2f",
           payload.device_id,
           (double)payload.temperature,
           (double)payload.humidity,
           (double)payload.soil_moisture);
  return ESP_OK;
}