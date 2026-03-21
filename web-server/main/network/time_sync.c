#include "time_sync.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_sntp.h"

// POSIX TZ string for Bangkok / Indochina Time (UTC+7, no daylight saving).
// Change this to match your local timezone if needed.
#ifndef TIME_SYNC_TIMEZONE
#define TIME_SYNC_TIMEZONE "ICT-7"
#endif

#define NTP_SERVER_PRIMARY   "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.google.com"

static const char *TAG = "time_sync";
static volatile bool s_time_synced = false;

static void sntp_sync_notification_cb(struct timeval *tv)
{
    s_time_synced = true;

    char time_buf[32];
    time_t now = tv->tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(time_buf, sizeof(time_buf), "%H:%M:%S %d/%m/%Y", &timeinfo);
    ESP_LOGI(TAG, "Time synchronised: %s", time_buf);
}

esp_err_t time_sync_start(void)
{
    if (esp_sntp_enabled())
    {
        ESP_LOGD(TAG, "SNTP already running");
        return ESP_OK;
    }

    // Set local timezone before the first localtime_r() call.
    setenv("TZ", TIME_SYNC_TIMEZONE, 1);
    tzset();

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
    if (!s_time_synced)
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
