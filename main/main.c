#include "main.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_zigbee_core.h" // Zigbee core includes
#include "ha/esp_zigbee_ha_standard.h"
#include "ds18b20.h"
#include "ds18b20_sensor.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "driver/rtc_io.h"


#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile sensor (End Device) source code.
#endif

#define TEMPERATURE_READ_INTERVAL_MS 10000  // Read temperature every 10 seconds
#define CUSTOM_SERVER_ENDPOINT_1 0x01
#define CUSTOM_SERVER_ENDPOINT_2 0x02

static const char *TAG = "ESP_ZB_TEMP_SENSOR";
esp_err_t esp_wifi_stop();  // Disable Wi-Fi if not used
esp_err_t esp_bt_controller_disable();

// Convert float temperature to Zigbee-compatible int16
static int16_t zb_temperature_to_s16(float temp) {
    return (int16_t)(temp * 100);
}


// Callback for commissioning retries
static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask) {
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,
                        TAG, "Failed to start Zigbee commissioning");
}

// Update the temperature sensor value for a specific endpoint and report it
static void esp_app_temp_sensor_handler(float temperature1, float temperature2) {
  int16_t measured_value1 = zb_temperature_to_s16(temperature1);
  int16_t measured_value2 = zb_temperature_to_s16(temperature2);

  /* Update temperature sensor measured value */
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &measured_value1,
        false
    );
    esp_zb_zcl_set_attribute_val(
        HA_ESP_SENSOR_ENDPOINT+1, ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &measured_value2,
        false
    );
    esp_zb_lock_release();
}

RTC_DATA_ATTR float prev_tsens_value1 = 0.0;
RTC_DATA_ATTR float prev_tsens_value2 = 0.0;

static void temp_sensor_value_update(void *arg) {
    float tsens_value1 = 0.0;
    float tsens_value2 = 0.0;
    float delta_temp1 = 0.0;
    float delta_temp2 = 0.0;
    const float delta_threshold = 0.5;  // Only send if temperature change > 0.5°C
    
    // Read temperature values
    esp_err_t err1 = ds18b20_read_temperature(0, &tsens_value1);
    esp_err_t err2 = ds18b20_read_temperature(1, &tsens_value2);

    // Calculate delta for sensor 1
    if (err1 == ESP_OK) {
        delta_temp1 = tsens_value1 - prev_tsens_value1;
        ESP_LOGI("MAIN", "Temperature 1: %.2f°C, Delta 1: %.2f°C", tsens_value1, delta_temp1);

        // Only send if delta exceeds threshold
        if (fabs(delta_temp1) > delta_threshold) {
            int16_t zigbee_temp1 = zb_temperature_to_s16(tsens_value1);
            esp_zb_zcl_set_attribute_val(
                HA_ESP_SENSOR_ENDPOINT,
                ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                &zigbee_temp1,
                false
            );
            prev_tsens_value1 = tsens_value1;  // Update previous value after sending
        } else {
            ESP_LOGI("MAIN", "Temperature 1 change is too small, not sending.");
        }
    } else {
        ESP_LOGE("MAIN", "Failed to read temperature 1");
    }

    // Calculate delta for sensor 2
    if (err2 == ESP_OK) {
        delta_temp2 = tsens_value2 - prev_tsens_value2;
        ESP_LOGI("MAIN", "Temperature 2: %.2f°C, Delta 2: %.2f°C", tsens_value2, delta_temp2);

        // Only send if delta exceeds threshold
        if (fabs(delta_temp2) > delta_threshold) {
            int16_t zigbee_temp2 = zb_temperature_to_s16(tsens_value2);
            esp_zb_zcl_set_attribute_val(
                HA_ESP_SENSOR_ENDPOINT + 1,
                ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
                ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
                ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
                &zigbee_temp2,
                false
            );
            prev_tsens_value2 = tsens_value2;  // Update previous value after sending
        } else {
            ESP_LOGI("MAIN", "Temperature 2 change is too small, not sending.");
        }
    } else {
        ESP_LOGE("MAIN", "Failed to read temperature 2");
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
    // Enter deep sleep if necessary
    esp_sleep_enable_timer_wakeup(30000000);  // Sleep for 60 seconds
    esp_deep_sleep_start();
}


// Zigbee signal handler
void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct) {
    uint32_t *p_sg_p     = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        // esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION) is used to start the commissioning process in a Zigbee network using the Base Device Behavior (BDB) specification.
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;

    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            // defferred_driver_init during boot to init functions
            ESP_LOGI(TAG, "Device started up in %s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : "non");
            //xTaskCreate(temp_sensor_value_update, "temp_sensor_update", 4096, NULL, 10, NULL);


            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted, no factory reset");
            }
        } else {
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;

    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Network steering failed (status: %s), retrying...", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        break;
    }
}

static esp_zb_cluster_list_t *custom_temperature_sensor_clusters_create(esp_zb_temperature_sensor_cfg_t *temperature_sensor) {
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();

    // Basic Cluster
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(&(temperature_sensor->basic_cfg));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, (void *)MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, (void *)MODEL_IDENTIFIER));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));

    // Identify Cluster
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(
        cluster_list, 
        esp_zb_identify_cluster_create(&(temperature_sensor->identify_cfg)), 
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE
    ));

    // Temperature Measurement Cluster
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_temperature_meas_cluster(
        cluster_list,
        esp_zb_temperature_meas_cluster_create(&(temperature_sensor->temp_meas_cfg)),
        ESP_ZB_ZCL_CLUSTER_SERVER_ROLE
    ));

    return cluster_list;
}

static esp_zb_ep_list_t *custom_temperature_sensor_ep_create(uint8_t endpoint_id, 
                                                              esp_zb_temperature_sensor_cfg_t *temperature_sensor_1, 
                                                              esp_zb_temperature_sensor_cfg_t *temperature_sensor_2) {
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();

    // Configure the first endpoint (for sensor 1)
    esp_zb_endpoint_config_t endpoint_config_1 = {
        .endpoint = endpoint_id,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, custom_temperature_sensor_clusters_create(temperature_sensor_1), endpoint_config_1);

    // Configure the second endpoint (for sensor 2)
    esp_zb_endpoint_config_t endpoint_config_2 = {
        .endpoint = endpoint_id + 1,  // Use a different endpoint ID for the second sensor
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_TEMPERATURE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, custom_temperature_sensor_clusters_create(temperature_sensor_2), endpoint_config_2);

    return ep_list;
}


static void esp_zb_task(void *pvParameters) {
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    
    // Initialize Zigbee stack
    esp_zb_init(&zb_nwk_cfg);
    vTaskDelay(pdMS_TO_TICKS(1000));
    // Create customized temperature sensor configurations for sensor 1 and sensor 2
    esp_zb_temperature_sensor_cfg_t sensor_cfg_1 = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();
    esp_zb_temperature_sensor_cfg_t sensor_cfg_2 = ESP_ZB_DEFAULT_TEMPERATURE_SENSOR_CONFIG();

    // Set (Min|Max)MeasuredValue for sensor 1
    sensor_cfg_1.temp_meas_cfg.min_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MIN_VALUE);
    sensor_cfg_1.temp_meas_cfg.max_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MAX_VALUE);

    // Set (Min|Max)MeasuredValue for sensor 2
    sensor_cfg_2.temp_meas_cfg.min_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MIN_VALUE);
    sensor_cfg_2.temp_meas_cfg.max_value = zb_temperature_to_s16(ESP_TEMP_SENSOR_MAX_VALUE);

    // Create and register both endpoints with different configurations
    esp_zb_ep_list_t *esp_zb_sensor_ep = custom_temperature_sensor_ep_create(HA_ESP_SENSOR_ENDPOINT, &sensor_cfg_1, &sensor_cfg_2);
    esp_zb_device_register(esp_zb_sensor_ep);  // This registers both endpoints

    // Set reporting information for both endpoints (using endpoint 1 here as an example)
    esp_zb_zcl_reporting_info_t reporting_info1 = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_SENSOR_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 0,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 0,
        .u.send_info.delta.u16 = 100,  // Report changes greater than 1°C
        .attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };

        esp_zb_zcl_reporting_info_t reporting_info2 = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_SRV,
        .ep = HA_ESP_SENSOR_ENDPOINT+1,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = 1,
        .u.send_info.max_interval = 60,
        .u.send_info.def_min_interval = 1,
        .u.send_info.def_max_interval = 60,
        .u.send_info.delta.u16 = 100,  // Report changes greater than 1°C
        .attr_id = ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };

    // Update reporting information
    esp_zb_zcl_update_reporting_info(&reporting_info1);

    esp_zb_zcl_update_reporting_info(&reporting_info2);

    // Start the Zigbee stack and let it handle the main loop
    ESP_ERROR_CHECK(esp_zb_start(false));

    // This will keep the task alive
    esp_zb_stack_main_loop();
}

// Application entry point
void app_main(void) {
    
    // Initialize NVS flash
    ESP_ERROR_CHECK(nvs_flash_init());

    // Zigbee platform configuration
    esp_zb_platform_config_t config = {
            .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
            .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
        };

    // Configure Zigbee platform
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    if (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER) {
        ESP_LOGI(TAG, "Woke up from deep sleep by timer.");
    } else {
        ESP_LOGI(TAG, "Normal startup, initializing sensor.");
    }

    esp_err_t ds18b20_init_ret = ds18b20_init();
    if (ds18b20_init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize DS18B20 sensor(s)");
        // Optionally handle this failure (e.g., retry, exit, etc.)
        return;  // Prevent further execution if the sensor initialization failed
    }

    
    // Start Zigbee task
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);

    xTaskCreate(temp_sensor_value_update, "temp_sensor_update", 4096, NULL, 10, NULL);
    
}

//export IDF_PATH='/Users/lukaspicker/esp/esp-idf'


//in the 2ds18b20 folder run . $HOME/esp/esp-idf/export.sh

//idf.py -p /dev/tty.usbmodem101 erase-flash 