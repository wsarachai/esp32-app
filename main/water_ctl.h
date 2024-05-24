/*
 * water_ctl.h
 *
 *  Created on: May 17, 2024
 *      Author: keng
 */

#ifndef MAIN_WATER_CTL_H_
#define MAIN_WATER_CTL_H_


#define ANALOG_VOLTAGE_MAX_DEFAULT		3300 // 3.3v
#define THRESHOLD_VALUE_DEFAULT			30   // 30%
#define WATER_DURATION_DEFAULT			1    // one minute

/**
 * Structure for the water parameters
 */
typedef struct water_config
{
	int16_t analog_voltage_max;
	int16_t threshold;
	int16_t duration; // minute

} water_config_t;

void water_ctl_configure(void);

void water_ctl_on(void);

void water_ctl_off(void);

uint8_t water_ctl_is_on(void);

water_config_t *water_ctl_get_config(void);

float water_ctl_get_soil_moisture(void);

#endif /* MAIN_WATER_CTL_H_ */
