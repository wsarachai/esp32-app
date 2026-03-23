# ESP-IDF IoT Projects (ESP32)

A multi-project ESP32 workspace built with ESP-IDF, focused on smart-farm style IoT scenarios:

- Sensor acquisition (DHT22, soil moisture ADC, Modbus sensors, RTC)
- Wi-Fi networking (STA and SoftAP)
- HTTP client/server communication
- Actuator control (relay, RGB LED, irrigation)
- OTA-capable web control server

This repository contains two larger applications and several focused examples.

## Repository Layout

- `sensor-node/`: ESP32 node that reads sensors and posts data to server
- `web-server/`: ESP32 SoftAP + HTTP server for control, monitoring, and OTA
- `examples/`: focused demos for individual components/features
- `tool_set_config.json`: local ESP-IDF toolchain configuration metadata

## Main Applications

### 1) sensor-node

Location: `sensor-node/`

Purpose:
- Reads environmental data (DHT22 + soil moisture ADC)
- Connects as Wi-Fi STA to the server AP
- Sends JSON updates to server endpoint (`/sensor-update`)
- Uses RGB LED signaling for status

Key modules:
- `main/sensors/`
  - `dht22.c/.h`: DHT22 timing/read logic
  - `soil_moisture_adc.c/.h`: ADC calibration and moisture mapping
  - `sensor_task.c/.h`: periodic sampling task and event/report flow
- `main/network/`
  - `wifi_sta.c/.h`: STA connection handling and retries
- `main/http/`
  - `http_client.c/.h`: HTTP POST of sensor payloads
- `main/actuators/`
  - `rgb_led.c/.h`: LED indication patterns
- `main/task_settings.h`: stack sizes, priorities, GPIO defaults

Typical data flow:
1. Boot and initialize peripherals
2. Join AP (`MJU-SmartFarm-AP` by default)
3. Sample sensors periodically
4. Convert/calibrate readings
5. POST JSON to server

### 2) web-server

Location: `web-server/`

Purpose:
- Runs as Wi-Fi SoftAP and serves control/status UI
- Receives sensor-node updates and caches latest values
- Controls relay/irrigation logic
- Supports OTA endpoint for firmware update
- Stores configuration in NVS

Key modules:
- `main/http/`
  - `http_server.c/.h`: server bootstrap and route registration
  - `http_server_static.c`: static asset handlers
  - `http_server_status.c`: status endpoint(s)
  - `http_server_relay.c`: relay control endpoint(s)
  - `http_server_wifi.c`: Wi-Fi scan/connect/config handlers
  - `http_server_ota.c`: OTA upload/update handler
  - `http_server_monitor.c/.h`: monitoring endpoints/helpers
- `main/network/`
  - `wifi_app.c/.h`: SoftAP setup and Wi-Fi event handling
  - `time_sync.c/.h`: network time sync utilities
  - `rtc_ds3231.c/.h`: DS3231 access/integration
- `main/sensors/`
  - `sensor_cache.c/.h`: in-memory latest sensor values
- `main/actuators/`
  - `relay.c/.h`: relay driver/control
  - `rgb-led.c/.h`: RGB status LED driver
  - `irrigation_ctrl.c/.h`: irrigation logic/orchestration
- `main/storage/`
  - `app_nvs.c/.h`: NVS read/write helpers
  - `water_config.c/.h`: watering configuration persistence
- `main/webpage/`: static web assets (HTML/CSS/JS)
- `partitions.csv`: custom partition layout for web assets/OTA

## Examples

Location: `examples/`

- `DHT22/`: DHT22 sensor reading demo
- `DS3231/`: RTC (and I2C EEPROM interactions) demo
- `Humidity/`: ADC humidity/soil-style percentage conversion demo
- `RGBLed/`: LEDC/PWM RGB LED control demo
- `RelayCtl/`: relay on/off control demo
- `modbus-xy-md03/`: Modbus RTU reading (temperature/humidity registers)
- `SoilEnvSensor/`: combined DHT22 + soil moisture example
- `WifiClent/`: Wi-Fi STA + HTTP client example (folder name has typo)
- `WifiWeb/`: simplified Wi-Fi web server example

## Build and Flash

Prerequisites:
- ESP-IDF installed and exported in your shell
- Supported ESP32 board connected over USB

From any project directory (`sensor-node`, `web-server`, or one example):

```bash
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

Examples:

```bash
cd web-server
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

```bash
cd sensor-node
idf.py set-target esp32
idf.py build
idf.py flash monitor
```

## Suggested Bring-up Order

1. Flash and run `web-server/` first (acts as AP + control endpoint).
2. Flash and run `sensor-node/` next.
3. Confirm sensor updates arrive at server endpoints/UI.
4. Validate relay/irrigation behavior via API/UI.

## Default Behaviors and Important Notes

- Several Wi-Fi values are hardcoded by default (for example AP SSID/password).
- Some GPIO assignments are fixed in headers/source and may need board-specific changes.
- Soil moisture conversion thresholds are calibration-dependent and should be tuned for your sensor/probe.
- Relay logic is active-low in multiple modules/examples.
- `examples/WifiClent` appears to be a naming typo (likely intended as `WifiClient`).
- HTTP control endpoints in demos are generally designed for local network use and may not include strong auth.

## Useful Files to Start Reading

- `sensor-node/main/main.c`
- `sensor-node/main/task_settings.h`
- `web-server/main/main.c`
- `web-server/main/http/http_server.c`
- `web-server/main/network/wifi_app.c`
- `web-server/main/actuators/irrigation_ctrl.c`
- `web-server/partitions.csv`

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**.

- You can use, modify, and distribute the code.
- If you distribute modified versions, you must also provide the source code under the same GPL-3.0 license terms.

See the full license text in the `LICENSE` file.
