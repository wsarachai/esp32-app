#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define HUMIDITY_ADC_UNIT ADC_UNIT_1
#define HUMIDITY_ADC_CHANNEL ADC_CHANNEL_6
#define HUMIDITY_ADC_ATTEN ADC_ATTEN_DB_12
#define HUMIDITY_SAMPLES_PER_READING 16
#define HUMIDITY_READ_PERIOD_MS 1000

/*
 * Update these two values after measuring your real sensor output.
 * Example: dry air voltage -> HUMIDITY_DRY_MV, wet condition voltage -> HUMIDITY_WET_MV.
 */
#define HUMIDITY_DRY_MV 2800
#define HUMIDITY_WET_MV 1200

static const char *TAG = "humidity_adc";

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle;
static bool adc_calibration_enabled;

static int clamp_int(int value, int min_value, int max_value)
{
  if (value < min_value)
  {
    return min_value;
  }
  if (value > max_value)
  {
    return max_value;
  }
  return value;
}

static int map_voltage_to_humidity_percent(int millivolts)
{
  const int input_span = HUMIDITY_WET_MV - HUMIDITY_DRY_MV;

  if (input_span == 0)
  {
    return 0;
  }

  const int humidity_percent = ((millivolts - HUMIDITY_DRY_MV) * 100) / input_span;
  return clamp_int(humidity_percent, 0, 100);
}

static esp_err_t init_adc_calibration(void)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t cali_config = {
      .unit_id = HUMIDITY_ADC_UNIT,
      .chan = HUMIDITY_ADC_CHANNEL,
      .atten = HUMIDITY_ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  ESP_RETURN_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali_handle), TAG,
                      "Failed to create curve fitting calibration");
  adc_calibration_enabled = true;
  return ESP_OK;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  adc_cali_line_fitting_config_t cali_config = {
      .unit_id = HUMIDITY_ADC_UNIT,
      .atten = HUMIDITY_ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  ESP_RETURN_ON_ERROR(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle), TAG,
                      "Failed to create line fitting calibration");
  adc_calibration_enabled = true;
  return ESP_OK;
#else
  ESP_LOGW(TAG, "ADC calibration scheme is not supported on this target");
  adc_calibration_enabled = false;
  return ESP_OK;
#endif
}

static void deinit_adc_calibration(void)
{
  if (!adc_calibration_enabled)
  {
    return;
  }

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(adc_cali_handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(adc_cali_handle));
#endif

  adc_calibration_enabled = false;
}

static void init_humidity_adc(void)
{
  adc_oneshot_unit_init_cfg_t unit_config = {
      .unit_id = HUMIDITY_ADC_UNIT,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

  adc_oneshot_chan_cfg_t channel_config = {
      .atten = HUMIDITY_ADC_ATTEN,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle));
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, HUMIDITY_ADC_CHANNEL, &channel_config));

  if (init_adc_calibration() != ESP_OK)
  {
    ESP_LOGW(TAG, "Continuing without ADC calibration; millivolt output will be unavailable");
  }
}

void app_main(void)
{
  init_humidity_adc();

  ESP_LOGI(TAG, "Humidity ADC test started");
  ESP_LOGI(TAG, "ESP32 ADC1 channel 6 maps to GPIO34");
  ESP_LOGI(TAG, "Adjust HUMIDITY_DRY_MV and HUMIDITY_WET_MV in main.c to match your sensor");

  while (true)
  {
    int raw_sum = 0;

    for (int sample = 0; sample < HUMIDITY_SAMPLES_PER_READING; ++sample)
    {
      int raw_value = 0;
      ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, HUMIDITY_ADC_CHANNEL, &raw_value));
      raw_sum += raw_value;
      vTaskDelay(pdMS_TO_TICKS(10));
    }

    const int raw_average = raw_sum / HUMIDITY_SAMPLES_PER_READING;

    if (adc_calibration_enabled)
    {
      int millivolts = 0;
      ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw_average, &millivolts));

      const int humidity_percent = map_voltage_to_humidity_percent(millivolts);
      ESP_LOGI(TAG, "raw=%d voltage=%d mV humidity=%d%%", raw_average, millivolts, humidity_percent);
    }
    else
    {
      ESP_LOGI(TAG, "raw=%d calibration=disabled", raw_average);
    }

    vTaskDelay(pdMS_TO_TICKS(HUMIDITY_READ_PERIOD_MS));
  }

  deinit_adc_calibration();
}