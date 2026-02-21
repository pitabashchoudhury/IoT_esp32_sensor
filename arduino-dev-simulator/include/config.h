#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
//  Ethernet Configuration
// ============================================================
// MAC address – must be unique on your LAN
// Change last byte if running multiple simulators
#define MAC_ADDRESS        { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0x01 }

// true = get IP from router via DHCP (recommended)
// false = use the static IP below
#define USE_DHCP           true

// Static IP (only used when USE_DHCP is false)
#define STATIC_IP          192, 168, 1, 100
#define GATEWAY_IP         192, 168, 1, 1
#define SUBNET_MASK        255, 255, 255, 0
#define DNS_SERVER         8, 8, 8, 8

// ============================================================
//  MQTT Broker Configuration
//  Must match iot-backend device-service application.yml
// ============================================================
#define MQTT_BROKER        "broker.hivemq.com"
#define MQTT_PORT          1883
#define MQTT_CLIENT_PREFIX "iot_dev_sim_"
#define MQTT_QOS           1
#define MQTT_RETRY_DELAY   5000     // ms between reconnect attempts

// Optional: If using authenticated broker
#define MQTT_USERNAME      ""
#define MQTT_PASSWORD      ""

// ============================================================
//  Device Configuration
//  Set DEVICE_ID to the UUID registered in your backend
// ============================================================
#define DEVICE_ID          "YOUR_DEVICE_UUID"

// ============================================================
//  MQTT Topic Patterns (match device-service)
//  Pattern: devices/{deviceId}/{messageType}
// ============================================================
#define TOPIC_PREFIX       "devices/"
#define TOPIC_STATUS       "/status"
#define TOPIC_TELEMETRY    "/telemetry"
#define TOPIC_CONTROL      "/control"

// ============================================================
//  Timing
// ============================================================
#define TELEMETRY_INTERVAL 10000    // ms between telemetry publishes (10s)
#define STATUS_INTERVAL    30000    // ms between status heartbeats  (30s)

// ============================================================
//  Simulated Sensor Data Ranges
//  Values will random-walk within these bounds
// ============================================================
#define TEMP_MIN           20.0     // Celsius
#define TEMP_MAX           35.0
#define TEMP_STEP          0.5      // max change per reading

#define HUM_MIN            40.0     // Percentage
#define HUM_MAX            80.0
#define HUM_STEP           1.0

#define PRES_MIN           1000.0   // hPa
#define PRES_MAX           1025.0
#define PRES_STEP          0.3

#define ALT_MIN            100.0    // meters
#define ALT_MAX            500.0
#define ALT_STEP           2.0

#define SOIL_MIN           20       // Percentage (0-100)
#define SOIL_MAX           80
#define SOIL_STEP          3

#define LIGHT_MIN          500      // Raw analog (0-4095)
#define LIGHT_MAX          3500
#define LIGHT_STEP         100

// ============================================================
//  LED Indicator (built-in LED on Arduino pin 13)
// ============================================================
#define LED_PIN            13
#define LED_ENABLED        true

#endif // CONFIG_H
