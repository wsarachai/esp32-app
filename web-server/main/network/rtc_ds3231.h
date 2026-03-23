#ifndef RTC_DS3231_H_
#define RTC_DS3231_H_

#include <stdbool.h>
#include <sys/time.h>

#include "esp_err.h"

esp_err_t rtc_ds3231_init(void);
bool rtc_ds3231_is_available(void);
esp_err_t rtc_ds3231_get_timeval(struct timeval *tv);
esp_err_t rtc_ds3231_set_timeval(const struct timeval *tv);

#endif // RTC_DS3231_H_