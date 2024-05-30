/*
 * water_ctl.h
 *
 *  Created on: May 17, 2024
 *      Author: keng
 */

#ifndef MAIN_WATER_CTL_H_
#define MAIN_WATER_CTL_H_


#define ANALOG_VOLTAGE_MAX_DEFAULT		3300 	// 3.3v
#define MIN_MOI_LEVEL_DEFAULT			30		// 10%
#define MAX_MOI_LEVEL_DEFAULT			80		// 10%
#define WATER_DURATION_DEFAULT			20   	// one minute
#define MANUAL_ON_OFF_DEFAULT			false

/**
 * Structure for the water parameters
 */
typedef struct water_config
{
	int16_t analog_voltage_max;
	int16_t min_moisture_level;
	int16_t max_moisture_level;
	int16_t duration; // minute
	bool manual_on_off;

} water_config_t;

void water_ctl_configure(void);

void water_ctl_on(void);

void water_ctl_off(void);

uint8_t water_ctl_is_on(void);

water_config_t *water_ctl_get_config(void);

float water_ctl_get_soil_moisture(void);

#endif /* MAIN_WATER_CTL_H_ */
