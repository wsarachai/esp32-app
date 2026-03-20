/*
 * water_adc_oneshot.h
 *
 *  Created on: May 16, 2024
 *      Author: keng
 */

#ifndef MAIN_WATER_HUMIDITY_ONESHOT_H_
#define MAIN_WATER_HUMIDITY_ONESHOT_H_

#define WATER_ADC_ATTEN			ADC_ATTEN_DB_11
#define WATER_ADC1_CHAN0		ADC_CHANNEL_4

esp_err_t water_humidity_init(void);

int water_humidity_get_raw_data(void);

int water_humidity_get_voltage(void);

void water_humidity_sync_obtain_value(void);

void water_humidity_tear_down(void);

#endif /* MAIN_WATER_HUMIDITY_ONESHOT_H_ */
