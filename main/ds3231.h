/*
 * ds3231.h
 *
 *  Created on: May 15, 2024
 *      Author: keng
 */

#ifndef MAIN_DS3231_H_
#define MAIN_DS3231_H_

#include <time.h>
#include <stdbool.h>
#include "driver/i2c.h"

#include "i2cdev.h"

#define DS3231_ADDR 0x68 //!< I2C address

#define DS3231_STAT_OSCILLATOR 		0x80
#define DS3231_STAT_32KHZ      		0x08
#define DS3231_STAT_BUSY       		0x04
#define DS3231_STAT_ALARM_2    		0x02
#define DS3231_STAT_ALARM_1    		0x01

#define DS3231_CTRL_OSCILLATOR    	0x80
#define DS3231_CTRL_SQUAREWAVE_BB 	0x40
#define DS3231_CTRL_TEMPCONV      	0x20
#define DS3231_CTRL_ALARM_INTS    	0x04
#define DS3231_CTRL_ALARM2_INT    	0x02
#define DS3231_CTRL_ALARM1_INT    	0x01

#define DS3231_ALARM_WDAY   		0x40
#define DS3231_ALARM_NOTSET 		0x80

#define DS3231_ADDR_TIME    		0x00
#define DS3231_ADDR_ALARM1  		0x07
#define DS3231_ADDR_ALARM2  		0x0b
#define DS3231_ADDR_CONTROL 		0x0e
#define DS3231_ADDR_STATUS  		0x0f
#define DS3231_ADDR_AGING   		0x10
#define DS3231_ADDR_TEMP    		0x11

#define DS3231_12HOUR_FLAG  		0x40
#define DS3231_12HOUR_MASK  		0x1f
#define DS3231_PM_FLAG      		0x20
#define DS3231_MONTH_MASK   		0x1f

/**
 * Starts DHT22 sensor task
 */
void DS3231_task_start(void);

/**
 * Returns local time if set.
 * @return local time buffer.
 */
char* DS3231_time_sync_get_time(void);

uint8_t bcd2dec(uint8_t val);
uint8_t dec2bcd(uint8_t val);

esp_err_t ds3231_set_time(struct tm *time);
esp_err_t ds3231_get_raw_temp(int16_t *temp);
esp_err_t ds3231_get_temp_integer(int8_t *temp);
esp_err_t ds3231_get_temp_float(float *temp);
esp_err_t ds3231_get_time(struct tm *time);

#endif /* MAIN_DS3231_H_ */
