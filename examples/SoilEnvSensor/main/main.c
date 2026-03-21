#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* ── DHT22 (air temperature + relative humidity) ──────────────────────────── */
#define DHT_PIN GPIO_NUM_32
#define DHT_TIMEOUT_US 100
#define DHT_DATA_BITS 40

/* ── Soil moisture (capacitive/resistive sensor on ADC) ──────────────────── */
#define SOIL_ADC_UNIT ADC_UNIT_1
#define SOIL_ADC_CHANNEL ADC_CHANNEL_7 /* GPIO35 */
#define SOIL_ADC_ATTEN ADC_ATTEN_DB_12
#define SOIL_SAMPLES 16

/*
 * Calibrate these voltages with your sensor in open-air (dry) and fully
 * submerged (wet) conditions.
 */
#define SOIL_DRY_MV 2800
#define SOIL_WET_MV 1100

/* ── Timing ──────────────────────────────────────────────────────────────── */
#define READ_PERIOD_MS 2000

static const char *TAG = "soil_env_sensor";

/* ── ADC handles ─────────────────────────────────────────────────────────── */
static adc_oneshot_unit_handle_t s_adc_handle;
static adc_cali_handle_t s_adc_cali_handle;
static bool s_adc_cali_enabled;

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility
 * ═══════════════════════════════════════════════════════════════════════════ */

static int clamp_int(int value, int lo, int hi)
{
    if (value < lo)
        return lo;
    if (value > hi)
        return hi;
    return value;
}

static int voltage_to_soil_percent(int mv)
{
    const int span = SOIL_WET_MV - SOIL_DRY_MV;
    if (span == 0)
        return 0;
    return clamp_int(((mv - SOIL_DRY_MV) * 100) / span, 0, 100);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Soil moisture – ADC
 * ═══════════════════════════════════════════════════════════════════════════ */

static esp_err_t soil_adc_cali_init(void)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id = SOIL_ADC_UNIT,
        .chan = SOIL_ADC_CHANNEL,
        .atten = SOIL_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_curve_fitting(&cfg, &s_adc_cali_handle),
                        TAG, "curve-fitting calibration failed");
    s_adc_cali_enabled = true;
    return ESP_OK;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cfg = {
        .unit_id = SOIL_ADC_UNIT,
        .atten = SOIL_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(adc_cali_create_scheme_line_fitting(&cfg, &s_adc_cali_handle),
                        TAG, "line-fitting calibration failed");
    s_adc_cali_enabled = true;
    return ESP_OK;
#else
    ESP_LOGW(TAG, "ADC calibration not supported on this target");
    s_adc_cali_enabled = false;
    return ESP_OK;
#endif
}

static void soil_adc_cali_deinit(void)
{
    if (!s_adc_cali_enabled)
        return;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(s_adc_cali_handle));
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(s_adc_cali_handle));
#endif
    s_adc_cali_enabled = false;
}

static void soil_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = SOIL_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = SOIL_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc_handle, SOIL_ADC_CHANNEL, &chan_cfg));

    if (soil_adc_cali_init() != ESP_OK)
    {
        ESP_LOGW(TAG, "Soil ADC continuing without calibration");
    }
}

/* Read soil moisture, returning percent (0-100) or -1 on error. */
static int soil_read_percent(void)
{
    int raw_sum = 0;

    for (int i = 0; i < SOIL_SAMPLES; ++i)
    {
        int raw = 0;
        if (adc_oneshot_read(s_adc_handle, SOIL_ADC_CHANNEL, &raw) != ESP_OK)
        {
            ESP_LOGE(TAG, "ADC read failed");
            return -1;
        }
        raw_sum += raw;
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    const int raw_avg = raw_sum / SOIL_SAMPLES;

    if (s_adc_cali_enabled)
    {
        int mv = 0;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_adc_cali_handle, raw_avg, &mv));
        return voltage_to_soil_percent(mv);
    }

    /* Fallback: linear raw mapping (0-4095 → 0-100 %) */
    return clamp_int((raw_avg * 100) / 4095, 0, 100);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DHT22 – air temperature & humidity
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool dht_read_bits(uint8_t *buf, int n)
{
    for (int i = 0; i < n; ++i)
    {
        int t = DHT_TIMEOUT_US;
        while (gpio_get_level(DHT_PIN) == 0 && t--)
            esp_rom_delay_us(1);
        if (t <= 0)
            return false;

        esp_rom_delay_us(30);
        uint8_t bit = gpio_get_level(DHT_PIN);
        buf[i / 8] = (uint8_t)((buf[i / 8] << 1) | bit);

        t = DHT_TIMEOUT_US;
        while (gpio_get_level(DHT_PIN) == 1 && t--)
            esp_rom_delay_us(1);
        if (t <= 0)
            return false;
    }
    return true;
}

/* Returns true on success, populates *humidity_pct and *temperature_c. */
static bool dht22_read(float *humidity_pct, float *temperature_c)
{
    uint8_t data[5] = {0};

    /* Drive start pulse */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT_PIN),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(DHT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(18));
    gpio_set_level(DHT_PIN, 1);

    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);

    /* Wait for sensor response LOW */
    int t = DHT_TIMEOUT_US;
    while (gpio_get_level(DHT_PIN) == 1 && t--)
        esp_rom_delay_us(1);
    if (t <= 0)
    {
        ESP_LOGE(TAG, "DHT22: no response");
        return false;
    }

    /* Wait for response HIGH */
    t = DHT_TIMEOUT_US;
    while (gpio_get_level(DHT_PIN) == 0 && t--)
        esp_rom_delay_us(1);
    if (t <= 0)
    {
        ESP_LOGE(TAG, "DHT22: response too short");
        return false;
    }

    /* Wait for data start */
    t = DHT_TIMEOUT_US;
    while (gpio_get_level(DHT_PIN) == 1 && t--)
        esp_rom_delay_us(1);
    if (t <= 0)
    {
        ESP_LOGE(TAG, "DHT22: data start timeout");
        return false;
    }

    if (!dht_read_bits(data, DHT_DATA_BITS))
    {
        ESP_LOGE(TAG, "DHT22: bit read error");
        return false;
    }

    /* Verify checksum */
    if (data[4] != (uint8_t)((data[0] + data[1] + data[2] + data[3]) & 0xFF))
    {
        ESP_LOGE(TAG, "DHT22: checksum mismatch");
        return false;
    }

    *humidity_pct = ((data[0] << 8) | data[1]) / 10.0f;
    *temperature_c = (((data[2] & 0x7F) << 8) | data[3]) / 10.0f;
    if (data[2] & 0x80)
        *temperature_c = -*temperature_c;

    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

void app_main(void)
{
    soil_adc_init();

    ESP_LOGI(TAG, "SoilEnvSensor started");
    ESP_LOGI(TAG, "  DHT22 data  -> GPIO%d", DHT_PIN);
    ESP_LOGI(TAG, "  Soil sensor -> ADC1 ch%d (GPIO35)", SOIL_ADC_CHANNEL);
    ESP_LOGI(TAG, "  Period      -> %d ms", READ_PERIOD_MS);

    int reading = 0;

    while (true)
    {
        reading++;

        /* --- DHT22 --- */
        float air_humidity = 0.0f;
        float air_temperature = 0.0f;
        bool dht_ok = dht22_read(&air_humidity, &air_temperature);

        /* --- Soil moisture --- */
        int soil_pct = soil_read_percent();

        /* --- Log --- */
        if (dht_ok && soil_pct >= 0)
        {
            ESP_LOGI(TAG, "[%d] temp=%.1f°C  air_hum=%.1f%%  soil_moist=%d%%",
                     reading, air_temperature, air_humidity, soil_pct);
        }
        else if (!dht_ok && soil_pct >= 0)
        {
            ESP_LOGW(TAG, "[%d] DHT22 read failed  soil_moist=%d%%", reading, soil_pct);
        }
        else if (dht_ok && soil_pct < 0)
        {
            ESP_LOGW(TAG, "[%d] temp=%.1f°C  air_hum=%.1f%%  soil ADC error",
                     reading, air_temperature, air_humidity);
        }
        else
        {
            ESP_LOGE(TAG, "[%d] all sensor reads failed", reading);
        }

        vTaskDelay(pdMS_TO_TICKS(READ_PERIOD_MS));
    }

    soil_adc_cali_deinit();
}
