#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  WiFi Configuration
// ============================================================
#define WIFI_SSID          "YOUR_WIFI_SSID"
#define WIFI_PASSWORD      "YOUR_WIFI_PASSWORD"
#define WIFI_RETRY_DELAY   5000   // ms between WiFi reconnect attempts
#define WIFI_MAX_RETRIES   20

// ============================================================
//  MQTT Broker Configuration
//  Must match iot-backend device-service application.yml
// ============================================================
#define MQTT_BROKER        "broker.hivemq.com"
#define MQTT_PORT          1883
#define MQTT_CLIENT_PREFIX "iot_arduino_"
#define MQTT_QOS           1
#define MQTT_KEEP_ALIVE    60       // seconds
#define MQTT_RETRY_DELAY   5000     // ms between reconnect attempts

// Optional: If using authenticated broker, set these
#define MQTT_USERNAME      ""
#define MQTT_PASSWORD      ""

// ============================================================
//  Device Configuration
//  IMPORTANT: Set DEVICE_ID to the UUID of the device you
//  registered in the backend via the mobile app or REST API.
//  The topic pattern must match: devices/{deviceId}/{type}
// ============================================================
#define DEVICE_ID          "YOUR_DEVICE_UUID"

// ============================================================
//  MQTT Topic Patterns (match device-service MqttService.java)
//  Pattern: devices/{deviceId}/{messageType}
// ============================================================
#define TOPIC_PREFIX       "devices/"
#define TOPIC_STATUS       "/status"
#define TOPIC_TELEMETRY    "/telemetry"
#define TOPIC_CONTROL      "/control"

// ============================================================
//  Sensor Configuration
// ============================================================
// DHT22 Temperature & Humidity Sensor
#define DHT_PIN            4         // GPIO pin connected to DHT data
#define DHT_TYPE           DHT22     // DHT11, DHT22, or DHT21

// BMP280 Barometric Pressure Sensor (I2C)
#define BMP280_ENABLED     true      // Set false if not connected
#define BMP280_ADDRESS     0x76      // 0x76 or 0x77

// Soil Moisture Sensor (Analog)
#define SOIL_MOISTURE_ENABLED  false
#define SOIL_MOISTURE_PIN      34     // Analog GPIO pin
#define SOIL_DRY_VALUE         4095   // ADC value when dry
#define SOIL_WET_VALUE         1500   // ADC value when wet

// Light Sensor (LDR - Analog)
#define LDR_ENABLED        false
#define LDR_PIN            35        // Analog GPIO pin

// ============================================================
//  Timing Configuration
// ============================================================
#define TELEMETRY_INTERVAL     10000  // ms between telemetry publishes (10s)
#define STATUS_INTERVAL        30000  // ms between status heartbeats (30s)
#define SENSOR_READ_INTERVAL   5000   // ms between sensor reads (5s)

// ============================================================
//  LED Indicator (optional, built-in LED)
// ============================================================
#define LED_PIN            2         // Built-in LED on most ESP32 boards
#define LED_ENABLED        true

#endif // CONFIG_H
