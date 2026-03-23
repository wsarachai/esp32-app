#include "time_sync.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_sntp.h"

#include "rtc_ds3231.h"

// POSIX TZ string for Bangkok / Indochina Time (UTC+7, no daylight saving).
// Change this to match your local timezone if needed.
#ifndef TIME_SYNC_TIMEZONE
#define TIME_SYNC_TIMEZONE "ICT-7"
#endif

#define NTP_SERVER_PRIMARY   "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.google.com"

static const char *TAG = "time_sync";
static volatile bool s_time_synced = false;
static bool s_timezone_configured = false;

#define TIME_SYNC_VALID_UNIX_EPOCH 1704067200

static void time_sync_configure_timezone(void)
{
    if (s_timezone_configured)
    {
        return;
    }

    setenv("TZ", TIME_SYNC_TIMEZONE, 1);
    tzset();
    s_timezone_configured = true;
}

static bool time_sync_system_time_is_valid(void)
{
    time_t now = time(NULL);
    return now >= (time_t)TIME_SYNC_VALID_UNIX_EPOCH;
}

static esp_err_t time_sync_restore_from_rtc(void)
{
    struct timeval rtc_time = {0};
    esp_err_t err = rtc_ds3231_get_timeval(&rtc_time);
    if (err != ESP_OK)
    {
        return err;
    }

    err = settimeofday(&rtc_time, NULL);
    if (err != 0)
    {
        return ESP_FAIL;
    }

    char time_buf[32];
    time_t now = rtc_time.tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S %d/%m/%Y", &timeinfo);
    ESP_LOGI(TAG, "System time restored from DS3231: %s", time_buf);
    return ESP_OK;
}

static void sntp_sync_notification_cb(struct timeval *tv)
{
    s_time_synced = true;

    esp_err_t rtc_err = rtc_ds3231_set_timeval(tv);
    if (rtc_err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to write SNTP time to DS3231: %s", esp_err_to_name(rtc_err));
    }

    char time_buf[32];
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S %d/%m/%Y", &timeinfo);
    ESP_LOGI(TAG, "Time synchronised: %s", time_buf);
}

esp_err_t time_sync_init(void)
{
    time_sync_configure_timezone();

    esp_err_t rtc_init_err = rtc_ds3231_init();
    if (rtc_init_err == ESP_OK)
    {
        esp_err_t restore_err = time_sync_restore_from_rtc();
        if (restore_err != ESP_OK)
        {
            ESP_LOGW(TAG, "DS3231 is present but time restore failed: %s", esp_err_to_name(restore_err));
            return restore_err;
        }

        return ESP_OK;
    }

    if (time_sync_system_time_is_valid())
    {
        return ESP_OK;
    }

    ESP_LOGW(TAG, "No valid offline RTC time available yet: %s", esp_err_to_name(rtc_init_err));
    return rtc_init_err;
}

esp_err_t time_sync_start(void)
{
    time_sync_configure_timezone();

    if (esp_sntp_enabled())
    {
        ESP_LOGD(TAG, "SNTP already running");
        return ESP_OK;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, NTP_SERVER_PRIMARY);
    esp_sntp_setservername(1, NTP_SERVER_SECONDARY);
    sntp_set_time_sync_notification_cb(sntp_sync_notification_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP started — servers: %s, %s  TZ: %s",
             NTP_SERVER_PRIMARY, NTP_SERVER_SECONDARY, TIME_SYNC_TIMEZONE);
    return ESP_OK;
}

bool time_sync_is_synced(void)
{
    return s_time_synced;
}

bool time_sync_get_local_time(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0)
    {
        return false;
    }

    time_sync_configure_timezone();

    if (!time_sync_system_time_is_valid())
    {
        if (rtc_ds3231_is_available())
        {
            esp_err_t restore_err = time_sync_restore_from_rtc();
            if (restore_err != ESP_OK)
            {
                ESP_LOGW(TAG, "Failed to refresh system time from DS3231: %s", esp_err_to_name(restore_err));
            }
        }
    }

    if (!time_sync_system_time_is_valid())
    {
        snprintf(buf, buf_size, "--:--:--");
        return false;
    }

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, buf_size, "%H:%M:%S %d/%m/%Y", &timeinfo);
    return true;
}
