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

void configure_water(void);

void water_on(void);

void water_off(void);

uint8_t get_water_status(void);

water_config_t *get_water_config(void);

float get_soil_humidity(void);

#endif /* MAIN_WATER_CTL_H_ */
