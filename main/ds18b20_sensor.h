// ds18b20_sensor.h

#ifndef DS18B20_SENSOR_H
#define DS18B20_SENSOR_H

#include "esp_err.h"

// Initialize the DS18B20 sensors
esp_err_t ds18b20_init(void);

// Read temperature from a DS18B20 sensor
esp_err_t ds18b20_read_temperature(int index, float *temperature);

#endif // DS18B20_SENSOR_H
