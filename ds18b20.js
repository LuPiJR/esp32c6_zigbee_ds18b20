const {deviceEndpoints, temperature} = require('zigbee-herdsman-converters/lib/modernExtend');
const reporting = require('zigbee-herdsman-converters/lib/reporting');

const definition = {
    zigbeeModel: ['esp32c6'],
    model: 'esp32c6',
    vendor: 'ESPRESSIF',
    description: 'Automatically generated definition',
    extend: [
        deviceEndpoints({"endpoints": {"10": 10, "11": 11}}),
        temperature({"endpointNames": ["10", "11"]})
    ],
    meta: {"multiEndpoint": true},
    
    configure: async (device, coordinatorEndpoint, logger) => {
        // Get both endpoints for the two sensors
        const endpoint10 = device.getEndpoint(10); // Endpoint for sensor 1
        const endpoint11 = device.getEndpoint(11); // Endpoint for sensor 2

        // Bind both endpoints to the coordinator and configure temperature reporting
        await reporting.bind(endpoint10, coordinatorEndpoint, ['msTemperatureMeasurement']);
        await reporting.temperature(endpoint10, {min: 30, max: 600, change: 5});   // Set up reporting for sensor 1 (every 30s to 10min, 0.5°C change)

        await reporting.bind(endpoint11, coordinatorEndpoint, ['msTemperatureMeasurement']);
        await reporting.temperature(endpoint11, {min: 30, max: 600, change: 5});  // Set up reporting for sensor 2 (every 30s to 10min, 0.5°C change)
    },
};

module.exports = definition;