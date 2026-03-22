#ifndef BT_SENSOR_CLIENT_H
#define BT_SENSOR_CLIENT_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define BT_SENSOR_SERVER_NAME "MJU-ESP32-Broom-Server"
#define BT_SENSOR_SVC_UUID 0xAA00
#define BT_SENSOR_DATA_UUID 0xAA01

#pragma pack(push, 1)
typedef struct
{
  char device_id[16];
  float temperature;
  float humidity;
  float soil_moisture;
} bt_sensor_payload_t;
#pragma pack(pop)

#define BT_SENSOR_PAYLOAD_LEN ((int)sizeof(bt_sensor_payload_t))

esp_err_t bt_sensor_client_start(void);
bool bt_sensor_client_is_connected(void);
void bt_sensor_client_maintain(void);
esp_err_t bt_sensor_client_send(float temperature, float humidity, float soil_moisture);

#endif // BT_SENSOR_CLIENT_H