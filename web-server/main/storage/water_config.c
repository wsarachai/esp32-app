#include "water_config.h"

#include <stdbool.h>
#include <string.h>

#include "nvs.h"

static const water_config_t DEFAULT_CFG = {
  .min_moisture_level = 35,
  .max_moisture_level = 60,
  .duration_minutes = 10,
};

static water_config_t s_cfg = {0};
static bool s_cfg_loaded = false;

static void water_config_load_once(void)
{
  if (s_cfg_loaded)
  {
    return;
  }

  s_cfg = DEFAULT_CFG;

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("water", NVS_READONLY, &nvs_handle);
  if (err != ESP_OK)
  {
    s_cfg_loaded = true;
    return;
  }

  uint32_t value = 0;
  if (nvs_get_u32(nvs_handle, "min", &value) == ESP_OK && value <= 100)
  {
    s_cfg.min_moisture_level = (uint16_t)value;
  }

  if (nvs_get_u32(nvs_handle, "max", &value) == ESP_OK && value <= 100)
  {
    s_cfg.max_moisture_level = (uint16_t)value;
  }

  if (nvs_get_u32(nvs_handle, "dur", &value) == ESP_OK && value <= 65535)
  {
    s_cfg.duration_minutes = (uint16_t)value;
  }

  nvs_close(nvs_handle);
  s_cfg_loaded = true;
}

water_config_t water_config_get(void)
{
  water_config_load_once();
  return s_cfg;
}

esp_err_t water_config_save(water_config_t cfg)
{
  s_cfg = cfg;
  s_cfg_loaded = true;

  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("water", NVS_READWRITE, &nvs_handle);
  if (err != ESP_OK)
  {
    return err;
  }

  err = nvs_set_u32(nvs_handle, "min", (uint32_t)cfg.min_moisture_level);
  if (err == ESP_OK)
  {
    err = nvs_set_u32(nvs_handle, "max", (uint32_t)cfg.max_moisture_level);
  }
  if (err == ESP_OK)
  {
    err = nvs_set_u32(nvs_handle, "dur", (uint32_t)cfg.duration_minutes);
  }
  if (err == ESP_OK)
  {
    err = nvs_commit(nvs_handle);
  }

  nvs_close(nvs_handle);
  return err;
}
