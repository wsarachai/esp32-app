/**
 * bt_sensor_server.c
 *
 * NimBLE GATT server that receives sensor data from other ESP32 broom nodes.
 *
 * Protocol:
 *   - Each peer broom acts as a BLE GATT client.
 *   - It connects to this device (which advertises as "ESP32-Broom-Server")
 *     and writes a bt_sensor_payload_t struct to the sensor-data characteristic.
 *   - On each successful write the data is forwarded to sensor_cache so the
 *     rest of the application (HTTP status endpoint, irrigation control, …)
 *     sees it just like data that arrived over HTTP.
 *
 * Service layout (16-bit UUIDs):
 *   Service  0xAA00  –  Broom Sensor Data Service
 *     Char   0xAA01  –  Sensor Data  (WRITE | WRITE_NO_RSP)
 *                        payload: bt_sensor_payload_t (28 bytes)
 */

#include "bt_sensor_server.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "nvs_flash.h"

/* NimBLE */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* Application */
#include "sensor_cache.h"
#include "main.h"
#include "task_settings.h"

static const char *TAG = "bt_sensor_server";
static uint8_t s_own_addr_type = BLE_OWN_ADDR_PUBLIC;

/* ─── Advertising name ─────────────────────────────────────────────────── */
#define BT_ADV_NAME "MJU-ESP32-Broom-Server"

/* ─── Forward declarations ─────────────────────────────────────────────── */
static void bt_advertise_start(void);

/* ─── GATT characteristic access callback ──────────────────────────────── */

static int sensor_data_chr_access_cb(uint16_t conn_handle,
                                     uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt,
                                     void *arg)
{
  (void)conn_handle;
  (void)attr_handle;
  (void)arg;

  if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
  {
    return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
  }

  /* Validate payload length exactly matches the packed struct */
  uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
  if (data_len != (uint16_t)BT_SENSOR_PAYLOAD_LEN)
  {
    ESP_LOGW(TAG, "Unexpected payload length: %u (expected %d)", data_len, BT_SENSOR_PAYLOAD_LEN);
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  bt_sensor_payload_t payload;
  uint16_t bytes_copied = 0;
  int rc = ble_hs_mbuf_to_flat(ctxt->om, &payload, sizeof(payload), &bytes_copied);
  if (rc != 0 || bytes_copied != (uint16_t)BT_SENSOR_PAYLOAD_LEN)
  {
    ESP_LOGE(TAG, "Failed to copy mbuf: rc=%d copied=%u", rc, bytes_copied);
    return BLE_ATT_ERR_UNLIKELY;
  }

  /* Ensure device_id is always null-terminated (defensive) */
  payload.device_id[sizeof(payload.device_id) - 1] = '\0';

  /* Sanitise floats – reject obviously bogus readings */
  if (payload.temperature < -40.0f || payload.temperature > 85.0f ||
      payload.humidity < 0.0f || payload.humidity > 100.0f ||
      payload.soil_moisture < 0.0f || payload.soil_moisture > 100.0f)
  {
    ESP_LOGW(TAG, "Out-of-range sensor values from '%s' – discarded",
             payload.device_id);
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
  }

  ESP_LOGI(TAG, "BT data from '%s': T=%.1f°C  H=%.1f%%  S=%.1f%%",
           payload.device_id,
           (double)payload.temperature,
           (double)payload.humidity,
           (double)payload.soil_moisture);

  esp_err_t err = sensor_cache_update_snapshot(payload.device_id,
                                               payload.temperature,
                                               payload.humidity,
                                               payload.soil_moisture);
  if (err != ESP_OK)
  {
    ESP_LOGW(TAG, "sensor_cache_update_snapshot error: %s", esp_err_to_name(err));
  }
  else
  {
    /* Notify main task so the LED flashes white (same as HTTP path) */
    app_send_message(APP_MSG_SENSOR_DATA_RECEIVED);
  }

  return 0; /* BLE_ATT_ERR_SUCCESS */
}

/* ─── GATT service table ────────────────────────────────────────────────── */

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(BT_SENSOR_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(BT_SENSOR_DATA_UUID),
                .access_cb = sensor_data_chr_access_cb,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {0} /* sentinel */
        },
    },
    {0} /* sentinel */
};

/* ─── GAP event handler ─────────────────────────────────────────────────── */

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
  (void)arg;

  switch (event->type)
  {
  case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0)
    {
      ESP_LOGI(TAG, "BLE client connected; conn_handle=%u",
               event->connect.conn_handle);
    }
    else
    {
      ESP_LOGW(TAG, "BLE connect failed (status %d) – restarting advertising",
               event->connect.status);
      bt_advertise_start();
    }
    break;

  case BLE_GAP_EVENT_DISCONNECT:
    ESP_LOGI(TAG, "BLE client disconnected; reason=%d – restarting advertising",
             event->disconnect.reason);
    bt_advertise_start();
    break;

  case BLE_GAP_EVENT_ADV_COMPLETE:
    ESP_LOGI(TAG, "Advertising complete – restarting");
    bt_advertise_start();
    break;

  default:
    break;
  }

  return 0;
}

/* ─── Advertising ───────────────────────────────────────────────────────── */

static void bt_advertise_start(void)
{
  struct ble_gap_adv_params adv_params = {0};
  struct ble_hs_adv_fields fields = {0};
  static const ble_uuid16_t advertised_uuids[] = {
      BLE_UUID16_INIT(BT_SENSOR_SVC_UUID),
  };

  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.uuids16 = advertised_uuids;
  fields.num_uuids16 = 1;
  fields.uuids16_is_complete = 1;
  fields.name = (const uint8_t *)BT_ADV_NAME;
  fields.name_len = (uint8_t)strlen(BT_ADV_NAME);
  fields.name_is_complete = 1;

  int rc = ble_gap_adv_set_fields(&fields);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "ble_gap_adv_set_fields error: %d", rc);
    return;
  }

  /* Undirected, connectable, general discoverable */
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                         &adv_params, gap_event_handler, NULL);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "ble_gap_adv_start error: %d", rc);
    return;
  }

  ESP_LOGI(TAG, "BLE advertising started as '%s'", BT_ADV_NAME);
}

/* ─── Host sync / reset callbacks ──────────────────────────────────────── */

static void bt_host_on_sync(void)
{
  /* Infer own address type (0 = prefer public, fall back to random) */
  int rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
    return;
  }

  bt_advertise_start();
}

static void bt_host_on_reset(int reason)
{
  ESP_LOGW(TAG, "BLE host reset; reason=%d", reason);
}

/* ─── NimBLE host task ──────────────────────────────────────────────────── */

static void nimble_host_task(void *param)
{
  (void)param;
  ESP_LOGI(TAG, "NimBLE host task started");
  nimble_port_run(); /* blocks until nimble_port_stop() */
  nimble_port_freertos_deinit();
}

/* ─── Public API ────────────────────────────────────────────────────────── */

esp_err_t bt_sensor_server_start(void)
{
  /* NimBLE requires NVS flash to be initialised */
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

  /* Register host callbacks */
  ble_hs_cfg.sync_cb = bt_host_on_sync;
  ble_hs_cfg.reset_cb = bt_host_on_reset;

  /* Set device name visible in generic access service */
  int rc = ble_svc_gap_device_name_set(BT_ADV_NAME);
  if (rc != 0)
  {
    ESP_LOGW(TAG, "ble_svc_gap_device_name_set error: %d", rc);
  }

  /* Initialise standard services */
  ble_svc_gap_init();
  ble_svc_gatt_init();

  /* Register custom GATT service */
  rc = ble_gatts_count_cfg(s_gatt_svcs);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "ble_gatts_count_cfg error: %d", rc);
    return ESP_FAIL;
  }

  rc = ble_gatts_add_svcs(s_gatt_svcs);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "ble_gatts_add_svcs error: %d", rc);
    return ESP_FAIL;
  }

  /* Start NimBLE host task (pinned to core 0, same as WiFi) */
  nimble_port_freertos_init(nimble_host_task);

  ESP_LOGI(TAG, "BLE sensor server initialised (service 0x%04X, char 0x%04X)",
           BT_SENSOR_SVC_UUID, BT_SENSOR_DATA_UUID);

  return ESP_OK;
}
