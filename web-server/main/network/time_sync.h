#ifndef TIME_SYNC_H_
#define TIME_SYNC_H_

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

/**
 * @brief Configure timezone support and restore system time from DS3231 if available.
 *
 * Call this once during boot before modules that need a valid wall clock.
 */
esp_err_t time_sync_init(void);

/**
 * @brief Initialize SNTP and start polling for the current time.
 *
 * Should be called once after the STA interface obtains an IP address.
 * Idempotent — safe to call again on reconnect.
 *
 * Uses NTP pool (pool.ntp.org) and sets the system timezone to
 * Bangkok / Indochina Time (UTC+7).  Change TIME_SYNC_TIMEZONE in
 * time_sync.c if a different timezone is needed.
 */
esp_err_t time_sync_start(void);

/**
 * @brief Returns true if time has been successfully synchronised at least once.
 */
bool time_sync_is_synced(void);

/**
 * @brief Write the current local time into buf as "HH:MM:SS DD/MM/YYYY".
 *
 * If internet time is not yet available, this falls back to DS3231-backed
 * system time when possible. If no valid time source exists, writes "--:--:--"
 * and returns false.
 *
 * @param buf       Destination buffer (at least 24 bytes recommended).
 * @param buf_size  Size of buf.
 * @return true if time is valid, false if no valid time source exists.
 */
bool time_sync_get_local_time(char *buf, size_t buf_size);

#endif // TIME_SYNC_H_
