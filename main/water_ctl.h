/*
 * water_ctl.h
 *
 *  Created on: May 17, 2024
 *      Author: keng
 */

#ifndef MAIN_WATER_CTL_H_
#define MAIN_WATER_CTL_H_

/**
 * Structure for the water parameters
 */
typedef struct water_config
{
	float analog_voltage_max;
	int low_bound;

} water_config_t;

void water_ctl_configure(void);

void water_ctl_on(void);

void water_ctl_off(void);

uint8_t water_ctl_is_on(void);

uint8_t water_ctl_is_off(void);

water_config_t *water_ctl_get_config(void);

float water_ctl_get_soil_humidity(void);

#endif /* MAIN_WATER_CTL_H_ */
