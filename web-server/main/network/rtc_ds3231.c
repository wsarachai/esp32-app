#include "rtc_ds3231.h"

#include <stdint.h>
#include <string.h>
#include <time.h>

#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#ifndef RTC_DS3231_I2C_PORT
#define RTC_DS3231_I2C_PORT I2C_NUM_0
#endif

#ifndef RTC_DS3231_SDA_IO
#define RTC_DS3231_SDA_IO 21
#endif

#ifndef RTC_DS3231_SCL_IO
#define RTC_DS3231_SCL_IO 22
#endif

#ifndef RTC_DS3231_I2C_FREQ_HZ
#define RTC_DS3231_I2C_FREQ_HZ 100000
#endif

#ifndef RTC_DS3231_ADDR
#define RTC_DS3231_ADDR 0x68
#endif

#define RTC_DS3231_REG_SECONDS 0x00
#define RTC_DS3231_REG_STATUS 0x0F

#define RTC_DS3231_STATUS_OSF 0x80

static const char *TAG = "rtc_ds3231";

typedef struct
{
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day_of_week;
    uint8_t day_of_month;
    uint8_t month;
    uint16_t year;
} rtc_ds3231_datetime_t;

static SemaphoreHandle_t s_rtc_mutex = NULL;
static bool s_i2c_ready = false;
static bool s_rtc_available = false;
static bool s_scan_logged = false;

static uint8_t dec_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static uint8_t bcd_to_dec(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static esp_err_t rtc_ds3231_lock(void)
{
    if (s_rtc_mutex == NULL)
    {
        s_rtc_mutex = xSemaphoreCreateMutex();
        if (s_rtc_mutex == NULL)
        {
            return ESP_ERR_NO_MEM;
        }
    }

    if (xSemaphoreTake(s_rtc_mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static void rtc_ds3231_unlock(void)
{
    if (s_rtc_mutex != NULL)
    {
        xSemaphoreGive(s_rtc_mutex);
    }
}

static esp_err_t rtc_ds3231_i2c_init_locked(void)
{
    if (s_i2c_ready)
    {
        return ESP_OK;
    }

    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = RTC_DS3231_SDA_IO,
        .scl_io_num = RTC_DS3231_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = RTC_DS3231_I2C_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(RTC_DS3231_I2C_PORT, &config);
    if (err != ESP_OK)
    {
        return err;
    }

    err = i2c_driver_install(RTC_DS3231_I2C_PORT, config.mode, 0, 0, 0);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE)
    {
        s_i2c_ready = true;
        return ESP_OK;
    }

    return err;
}

static esp_err_t rtc_ds3231_read_bytes_locked(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(RTC_DS3231_I2C_PORT,
                                        RTC_DS3231_ADDR,
                                        &reg,
                                        1,
                                        data,
                                        len,
                                        pdMS_TO_TICKS(100));
}

static esp_err_t rtc_ds3231_write_bytes_locked(uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t buffer[8];
    if (len > (sizeof(buffer) - 1U))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    buffer[0] = reg;
    memcpy(&buffer[1], data, len);
    return i2c_master_write_to_device(RTC_DS3231_I2C_PORT,
                                      RTC_DS3231_ADDR,
                                      buffer,
                                      len + 1U,
                                      pdMS_TO_TICKS(100));
}

static void rtc_ds3231_log_scan_locked(void)
{
    if (s_scan_logged)
    {
        return;
    }

    s_scan_logged = true;
    ESP_LOGW(TAG,
             "Scanning I2C bus on port=%d SDA=%d SCL=%d for DS3231 addr=0x%02X",
             RTC_DS3231_I2C_PORT,
             RTC_DS3231_SDA_IO,
             RTC_DS3231_SCL_IO,
             RTC_DS3231_ADDR);

    int found_count = 0;
    for (uint8_t addr = 0x08; addr <= 0x77; addr++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        if (cmd == NULL)
        {
            ESP_LOGW(TAG, "I2C scan skipped: failed to allocate command link");
            return;
        }

        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (uint8_t)(addr << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(RTC_DS3231_I2C_PORT, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (err == ESP_OK)
        {
            found_count++;
            ESP_LOGW(TAG,
                     "I2C device found at 0x%02X%s",
                     addr,
                     (addr == RTC_DS3231_ADDR) ? " <- expected DS3231" : "");
        }
    }

    if (found_count == 0)
    {
        ESP_LOGW(TAG, "No I2C devices responded on the configured bus");
    }
}

static esp_err_t rtc_ds3231_read_datetime_locked(rtc_ds3231_datetime_t *dt)
{
    if (dt == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[7] = {0};
    ESP_RETURN_ON_ERROR(rtc_ds3231_read_bytes_locked(RTC_DS3231_REG_SECONDS, raw, sizeof(raw)),
                        TAG,
                        "failed to read datetime registers");

    dt->second = bcd_to_dec((uint8_t)(raw[0] & 0x7FU));
    dt->minute = bcd_to_dec((uint8_t)(raw[1] & 0x7FU));
    dt->hour = bcd_to_dec((uint8_t)(raw[2] & 0x3FU));
    dt->day_of_week = bcd_to_dec((uint8_t)(raw[3] & 0x07U));
    dt->day_of_month = bcd_to_dec((uint8_t)(raw[4] & 0x3FU));
    dt->month = bcd_to_dec((uint8_t)(raw[5] & 0x1FU));
    dt->year = (uint16_t)(2000U + bcd_to_dec(raw[6]));

    return ESP_OK;
}

static esp_err_t rtc_ds3231_write_datetime_locked(const rtc_ds3231_datetime_t *dt)
{
    if (dt == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t raw[7] = {
        dec_to_bcd(dt->second),
        dec_to_bcd(dt->minute),
        dec_to_bcd(dt->hour),
        dec_to_bcd(dt->day_of_week),
        dec_to_bcd(dt->day_of_month),
        dec_to_bcd(dt->month),
        dec_to_bcd((uint8_t)(dt->year % 100U)),
    };

    return rtc_ds3231_write_bytes_locked(RTC_DS3231_REG_SECONDS, raw, sizeof(raw));
}

static esp_err_t rtc_ds3231_read_status_locked(uint8_t *status)
{
    return rtc_ds3231_read_bytes_locked(RTC_DS3231_REG_STATUS, status, 1);
}

static esp_err_t rtc_ds3231_clear_osf_locked(void)
{
    uint8_t status = 0;
    ESP_RETURN_ON_ERROR(rtc_ds3231_read_status_locked(&status), TAG, "failed to read status");
    status = (uint8_t)(status & (uint8_t)(~RTC_DS3231_STATUS_OSF));
    return rtc_ds3231_write_bytes_locked(RTC_DS3231_REG_STATUS, &status, 1);
}

esp_err_t rtc_ds3231_init(void)
{
    esp_err_t err = rtc_ds3231_lock();
    if (err != ESP_OK)
    {
        return err;
    }

    err = rtc_ds3231_i2c_init_locked();
    if (err != ESP_OK)
    {
        s_rtc_available = false;
        rtc_ds3231_unlock();
        ESP_LOGW(TAG, "I2C init failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG,
             "Probing DS3231 on port=%d SDA=%d SCL=%d addr=0x%02X freq=%d",
             RTC_DS3231_I2C_PORT,
             RTC_DS3231_SDA_IO,
             RTC_DS3231_SCL_IO,
             RTC_DS3231_ADDR,
             RTC_DS3231_I2C_FREQ_HZ);

    uint8_t status = 0;
    err = rtc_ds3231_read_status_locked(&status);
    s_rtc_available = (err == ESP_OK);
    if (err != ESP_OK)
    {
        rtc_ds3231_log_scan_locked();
    }
    rtc_ds3231_unlock();

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "DS3231 not detected: %s", esp_err_to_name(err));
        return err;
    }

    if ((status & RTC_DS3231_STATUS_OSF) != 0U)
    {
        ESP_LOGW(TAG, "DS3231 detected but oscillator stop flag is set; RTC time is not trustworthy yet");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "DS3231 ready on I2C port %d (SDA=%d SCL=%d)",
             RTC_DS3231_I2C_PORT,
             RTC_DS3231_SDA_IO,
             RTC_DS3231_SCL_IO);
    return ESP_OK;
}

bool rtc_ds3231_is_available(void)
{
    return s_rtc_available;
}

esp_err_t rtc_ds3231_get_timeval(struct timeval *tv)
{
    if (tv == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = rtc_ds3231_lock();
    if (err != ESP_OK)
    {
        return err;
    }

    err = rtc_ds3231_i2c_init_locked();
    if (err != ESP_OK)
    {
        rtc_ds3231_unlock();
        return err;
    }

    uint8_t status = 0;
    err = rtc_ds3231_read_status_locked(&status);
    if (err != ESP_OK)
    {
        s_rtc_available = false;
        rtc_ds3231_unlock();
        return err;
    }

    if ((status & RTC_DS3231_STATUS_OSF) != 0U)
    {
        rtc_ds3231_unlock();
        return ESP_ERR_INVALID_STATE;
    }

    rtc_ds3231_datetime_t dt = {0};
    err = rtc_ds3231_read_datetime_locked(&dt);
    rtc_ds3231_unlock();
    if (err != ESP_OK)
    {
        s_rtc_available = false;
        return err;
    }

    struct tm timeinfo = {
        .tm_sec = dt.second,
        .tm_min = dt.minute,
        .tm_hour = dt.hour,
        .tm_mday = dt.day_of_month,
        .tm_mon = (int)dt.month - 1,
        .tm_year = (int)dt.year - 1900,
        .tm_isdst = -1,
    };

    time_t timestamp = mktime(&timeinfo);
    if (timestamp == (time_t)-1)
    {
        return ESP_ERR_INVALID_RESPONSE;
    }

    tv->tv_sec = timestamp;
    tv->tv_usec = 0;
    s_rtc_available = true;
    return ESP_OK;
}

esp_err_t rtc_ds3231_set_timeval(const struct timeval *tv)
{
    if (tv == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    time_t timestamp = tv->tv_sec;
    struct tm timeinfo;
    if (localtime_r(&timestamp, &timeinfo) == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    rtc_ds3231_datetime_t dt = {
        .second = (uint8_t)timeinfo.tm_sec,
        .minute = (uint8_t)timeinfo.tm_min,
        .hour = (uint8_t)timeinfo.tm_hour,
        .day_of_week = (uint8_t)((timeinfo.tm_wday == 0) ? 7 : timeinfo.tm_wday),
        .day_of_month = (uint8_t)timeinfo.tm_mday,
        .month = (uint8_t)(timeinfo.tm_mon + 1),
        .year = (uint16_t)(timeinfo.tm_year + 1900),
    };

    esp_err_t err = rtc_ds3231_lock();
    if (err != ESP_OK)
    {
        return err;
    }

    err = rtc_ds3231_i2c_init_locked();
    if (err == ESP_OK)
    {
        err = rtc_ds3231_write_datetime_locked(&dt);
    }
    if (err == ESP_OK)
    {
        err = rtc_ds3231_clear_osf_locked();
    }

    if (err == ESP_OK)
    {
        s_rtc_available = true;
    }
    else
    {
        s_rtc_available = false;
    }

    rtc_ds3231_unlock();
    return err;
}