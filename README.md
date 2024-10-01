# ESP32 Zigbee Temperature Sensor with DS18B20

This project implements a Zigbee-enabled temperature sensor using an ESP32c6, two DS18B20 temperature sensors, and the ESP Zigbee stack. The temperature data is read from the DS18B20 sensors and reported over Zigbee to Zigbee2MQTT on my HomeAssistant instance. 

## Features
- Zigbee-based communication using the ESP32 Zigbee stack.
- Support for two DS18B20 temperature sensors.
- Reports temperature to a Zigbee coordinator or gateway.
- Zigbee commissioning with network steering for joining the Zigbee network.
- Regular temperature updates sent every 10 seconds.

## Hardware Requirements
- ESP32c6 development board with Zigbee support.
- 2x DS18B20 temperature sensors.
- Don't forget the Pull-up resistor (4.7kΩ) for the DS18B20 1-Wire bus.
- Proper wiring for DS18B20 sensors to the ESP32 GPIO 0.

## Software Requirements
- ESP-IDF (Espressif IoT Development Framework).
- Zigbee SDK provided by Espressif.
- FreeRTOS (integrated into ESP-IDF).
- DS18B20 1-Wire driver for reading temperature values.
- ESP-IDF VSCode Extension 

## Setup

1. **Clone the Repository**:

2. **Configure Zigbee**:
Before building the project, configure the Zigbee device to operate in End Device (ED) mode:
In the Zigbee menu-config, define the device as end-device and set the partionstable to csv. 

3. **Wiring**:
- Connect the DS18B20 sensors to the ESP32c6 GPIO (e.g., GPIO 0).
- Use a pull-up resistor (4.7kΩ) on the data line of the DS18B20.

4. **Build and Flash the Firmware**:
Compile and flash the firmware to your ESP32c6 device:

5. **Monitor the Serial Output**:
After flashing, monitor the serial output to check if the sensors are initialized correctly and see the temperature readings:

## Code Overview

- **main.c**: The main application logic, including initializing the Zigbee stack, reading temperature from the DS18B20 sensors, and reporting it over Zigbee.
- Uses `esp_zb_bdb_start_top_level_commissioning` for Zigbee commissioning.
- Periodically reads temperature values using the `ds18b20_read_temperature` function.
- Updates Zigbee temperature measurement attributes using `esp_zb_zcl_set_attribute_val`.

- **ds18b20_sensor.c**: Manages the initialization and communication with the DS18B20 sensors.
- Initializes the 1-Wire bus and discovers connected DS18B20 devices.
- Reads the temperature values from each DS18B20 sensor and provides these readings to the main application.
- The max sensors is currently set to two, but can be increased.

## Zigbee Functionality

The ESP32 Zigbee application follows these key steps:
1. Initializes the Zigbee stack.
2. Joins a Zigbee network using network steering.
3. Configures two endpoints for two DS18B20 sensors.
4. Periodically reads the temperature from both sensors and reports the data to the Zigbee coordinator.

### Commissioning Process
The commissioning process is initiated using the Base Device Behavior (BDB) mode for network steering. This allows the device to join an existing Zigbee network or to retry if the network join fails.

### Reporting
Temperature data from both DS18B20 sensors is reported using the Zigbee "Temperature Measurement" cluster. The data is updated and transmitted to the Zigbee coordinator, with configurable reporting intervals and thresholds.

### Zigbee2MQTT
Since the Zigbee2MQTT device is not known, a custom device .js file must be added to /homeassistant/zigbee2mqtt/ds18b20.js

## License
This project is open-source and available under the MIT License.

