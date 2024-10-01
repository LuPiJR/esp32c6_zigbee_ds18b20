// ds18b20_sensor.c

#include "ds18b20_sensor.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"
#include "onewire_bus.h"
#include "ds18b20.h"

// Define the GPIO pin for the 1-Wire bus
#define ONEWIRE_BUS_GPIO    0  // Ensure this is the correct GPIO pin for your 1-Wire bus
#define MAX_DS18B20_SENSORS 2  // Adjust this if you have more sensors

static const char *TAG = "DS18B20_SENSOR";
static ds18b20_device_handle_t ds18b20s[MAX_DS18B20_SENSORS];
static int ds18b20_device_num = 0;
static onewire_bus_handle_t bus = NULL;

esp_err_t ds18b20_init(void) {
    // Configure the 1-Wire bus
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = ONEWIRE_BUS_GPIO,
    };
    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10, // 1 byte ROM command + 8 bytes ROM number + 1 byte device command
    };

    // Initialize the 1-Wire bus
    esp_err_t ret = onewire_new_bus_rmt(&bus_config, &rmt_config, &bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize 1-Wire bus on GPIO %d, error: %s", ONEWIRE_BUS_GPIO, esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "1-Wire bus initialized on GPIO %d", ONEWIRE_BUS_GPIO);

    // Create an iterator to search for devices on the 1-Wire bus
    onewire_device_iter_handle_t iter = NULL;
    ret = onewire_new_device_iter(bus, &iter);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create 1-Wire device iterator, error: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Searching for devices on the 1-Wire bus...");
    ds18b20_device_num = 0;  // Reset the device count
    onewire_device_t next_onewire_device;

    // Search for devices
    while (onewire_device_iter_get_next(iter, &next_onewire_device) == ESP_OK) {
        ESP_LOGI(TAG, "Found device with address: %016llX", next_onewire_device.address);
        
        ds18b20_config_t ds_cfg = {};
        if (ds18b20_new_device(&next_onewire_device, &ds_cfg, &ds18b20s[ds18b20_device_num]) == ESP_OK) {
            ESP_LOGI(TAG, "DS18B20 sensor initialized at index %d", ds18b20_device_num);
            ds18b20_device_num++;
        } else {
            ESP_LOGW(TAG, "Device found at address %016llX is not a DS18B20", next_onewire_device.address);
        }

        if (ds18b20_device_num >= MAX_DS18B20_SENSORS) {
            ESP_LOGI(TAG, "Max number of DS18B20 sensors (%d) reached", MAX_DS18B20_SENSORS);
            break;
        }
    }

    // Delete the iterator
    ESP_ERROR_CHECK(onewire_del_device_iter(iter));
    ESP_LOGI(TAG, "Device search completed, %d DS18B20 sensor(s) found", ds18b20_device_num);

    if (ds18b20_device_num == 0) {
        ESP_LOGW(TAG, "No DS18B20 sensors found on the bus");
        return ESP_FAIL;
    }

    return ESP_OK;
}


// Read temperature from DS18B20 sensors
esp_err_t ds18b20_read_temperature(int index, float *temperature) {
    if (index >= ds18b20_device_num || index < 0) {
        ESP_LOGE(TAG, "Invalid sensor index: %d", index);
        return ESP_ERR_INVALID_ARG;
    }

    // Trigger a temperature conversion
    ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20s[index]));

    // Read the temperature value
    ESP_ERROR_CHECK(ds18b20_get_temperature(ds18b20s[index], temperature));

    ESP_LOGI(TAG, "Temperature read from DS18B20[%d]: %.2f°C", index, *temperature);
    return ESP_OK;
}
