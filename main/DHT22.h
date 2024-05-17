/* 

	DHT22 temperature sensor driver

*/

#ifndef DHT22_H_  
#define DHT22_H_

// == function prototypes =======================================

esp_err_t DHT22_init(void);

void DHT22_set_gpio(int gpio);

float DHT22_get_humidity();

float DHT22_get_temperature();

void DHT22_sync_obtain_value(void);

#endif
