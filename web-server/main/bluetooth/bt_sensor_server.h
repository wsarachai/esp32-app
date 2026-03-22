#ifndef BT_SENSOR_SERVER_H
#define BT_SENSOR_SERVER_H

#include "esp_err.h"
#include <stdint.h>

/**
 * Packed binary payload sent by each client ESP32 broom over BLE.
 * The client writes this struct to the sensor-data characteristic.
 */
#pragma pack(push, 1)
typedef struct
{
    char  device_id[16];   /**< Null-terminated node identifier, e.g. "broom-02" */
    float temperature;     /**< °C */
    float humidity;        /**< % */
    float soil_moisture;   /**< % */
} bt_sensor_payload_t;
#pragma pack(pop)

#define BT_SENSOR_PAYLOAD_LEN  ((int)sizeof(bt_sensor_payload_t))  /* 28 bytes */

/**
 * BLE service / characteristic UUIDs (16-bit, custom vendor range).
 * Both the server (this file) and the client broom must use the same values.
 */
#define BT_SENSOR_SVC_UUID   0xAA00   /**< Sensor data service */
#define BT_SENSOR_DATA_UUID  0xAA01   /**< Write-only characteristic: bt_sensor_payload_t */

/**
 * Start the NimBLE GATT server and begin advertising.
 *
 * Must be called AFTER sensor_cache_start() and preferably from app_main()
 * before the main task loop.  Safe to call only once.
 *
 * @return ESP_OK on success, ESP_FAIL otherwise.
 */
esp_err_t bt_sensor_server_start(void);

#endif /* BT_SENSOR_SERVER_H */
