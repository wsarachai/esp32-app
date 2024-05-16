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

/**
 * Starts DHT22 sensor task
 */
void water_adc_task_start(void);

int water_humidity_get_raw_data(void);

int water_humidity_get_voltage(void);

#endif /* MAIN_WATER_HUMIDITY_ONESHOT_H_ */
