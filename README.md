# ESP32 Zigbee Temperature Sensor with DS18B20

This project implements a Zigbee-enabled temperature sensor using an ESP32, two DS18B20 temperature sensors, and the ESP Zigbee stack. The temperature data is read from the DS18B20 sensors and reported over Zigbee to a compatible Zigbee controller. 

## Features
- Zigbee-based communication using the ESP32 Zigbee stack.
- Support for two DS18B20 temperature sensors.
- Reports temperature to a Zigbee coordinator or gateway.
- Zigbee commissioning with network steering for joining the Zigbee network.
- Regular temperature updates sent every 10 seconds.

## Hardware Requirements
- ESP32 development board with Zigbee support (such as the ESP32-H2).
- 2x DS18B20 temperature sensors.
- Pull-up resistor (4.7kΩ) for the DS18B20 1-Wire bus.
- Proper wiring for DS18B20 sensors to the ESP32 GPIO.

## Software Requirements
- ESP-IDF (Espressif IoT Development Framework).
- Zigbee SDK provided by Espressif.
- FreeRTOS (integrated into ESP-IDF).
- DS18B20 1-Wire driver for reading temperature values.

## Setup

1. **Clone the Repository**:

2. **Configure Zigbee**:
Before building the project, configure the Zigbee device to operate in End Device (ED) mode:
In the Zigbee configuration, define `ZB_ED_ROLE` to indicate that the device is a Zigbee End Device.

3. **Wiring**:
- Connect the DS18B20 sensors to the ESP32 GPIO (e.g., GPIO 0).
- Use a pull-up resistor (4.7kΩ) on the data line of the DS18B20.

4. **Build and Flash the Firmware**:
Compile and flash the firmware to your ESP32 device:

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

## License
This project is open-source and available under the MIT License.

