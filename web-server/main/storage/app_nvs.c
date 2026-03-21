#include "app_nvs.h"
#include "esp_log.h"

#include "wifi_app.h"

#include "nvs.h"
#include "nvs_flash.h"

esp_err_t app_nvs_init(void)
{
  ESP_LOGI("app_nvs", "Initializing NVS");
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
  {
    ESP_LOGW("app_nvs", "Erasing NVS flash");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_LOGI("app_nvs", "NVS initialized");
  return ret;
}

esp_err_t app_nvs_erase(void)
{
  ESP_LOGI("app_nvs", "Erasing NVS flash");
  return nvs_flash_erase();
}

esp_err_t app_nvs_save_sta_creds(void)
{
  ESP_LOGI("app_nvs", "Saving Wi-Fi credentials to NVS");
  char ssid[MAX_SSID_LENGTH + 1] = {0};
  char password[MAX_PASSWORD_LENGTH + 1] = {0};

  esp_err_t ret = wifi_app_get_sta_creds(ssid, sizeof(ssid), password, sizeof(password));
  if (ret != ESP_OK)
  {
    return ret;
  }

  nvs_handle_t nvs_handle;
  ret = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK)
  {
    return ret;
  }

  ret = nvs_set_str(nvs_handle, "sta_ssid", ssid);
  if (ret == ESP_OK)
  {
    ret = nvs_set_str(nvs_handle, "sta_pass", password);
  }

  if (ret == ESP_OK)
  {
    ret = nvs_commit(nvs_handle);
  }

  nvs_close(nvs_handle);
  return ret;
}

esp_err_t app_nvs_load_sta_creds(void)
{
  char ssid[MAX_SSID_LENGTH + 1] = {0};
  char password[MAX_PASSWORD_LENGTH + 1] = {0};
  size_t ssid_len = sizeof(ssid);
  size_t pass_len = sizeof(password);

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open("wifi", NVS_READONLY, &nvs_handle);
  if (ret == ESP_ERR_NVS_NOT_FOUND)
  {
    return ESP_OK;
  }
  if (ret != ESP_OK)
  {
    return ret;
  }

  ret = nvs_get_str(nvs_handle, "sta_ssid", ssid, &ssid_len);
  if (ret == ESP_ERR_NVS_NOT_FOUND)
  {
    nvs_close(nvs_handle);
    return ESP_OK;
  }
  if (ret != ESP_OK)
  {
    nvs_close(nvs_handle);
    return ret;
  }

  ret = nvs_get_str(nvs_handle, "sta_pass", password, &pass_len);
  if (ret == ESP_ERR_NVS_NOT_FOUND)
  {
    nvs_close(nvs_handle);
    return ESP_OK;
  }
  if (ret != ESP_OK)
  {
    nvs_close(nvs_handle);
    return ret;
  }

  nvs_close(nvs_handle);
  return wifi_app_set_sta_creds(ssid, password);
}
