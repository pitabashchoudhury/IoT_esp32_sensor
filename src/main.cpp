// ============================================================
//  IoT Solution - ESP32 Sensor Firmware
//  Sends sensor data to iot-backend via MQTT
//  Compatible with device-service topic pattern:
//    devices/{deviceId}/status
//    devices/{deviceId}/telemetry
//    devices/{deviceId}/control
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "sensor_reader.h"
#include "mqtt_handler.h"

SensorReader sensors;
MqttHandler  mqtt;

unsigned long lastTelemetry = 0;
unsigned long lastStatus = 0;
unsigned long lastSensorRead = 0;
SensorData latestData = {};

// ── WiFi ────────────────────────────────────────────────────

void connectWiFi() {
    Serial.print("[WiFi] Connecting to " + String(WIFI_SSID));
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < WIFI_MAX_RETRIES) {
        delay(WIFI_RETRY_DELAY);
        Serial.print(".");
        retries++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected! IP: " + WiFi.localIP().toString());
    } else {
        Serial.println("\n[WiFi] Connection failed! Restarting...");
        ESP.restart();
    }
}

// ── Control Command Handler ─────────────────────────────────

void onControlCommand(const String& controlId, const String& value) {
    Serial.println("[Control] ID: " + controlId + ", Value: " + value);

    // ──────────────────────────────────────────────────────
    //  Handle control commands from the mobile app here.
    //  Map controlId to physical actuators on your board.
    //
    //  Example:
    //    if (controlId == "led-control-uuid") {
    //        digitalWrite(RELAY_PIN, value == "true" ? HIGH : LOW);
    //    }
    //    if (controlId == "fan-speed-uuid") {
    //        analogWrite(FAN_PIN, value.toInt());
    //    }
    // ──────────────────────────────────────────────────────

#if LED_ENABLED
    // Demo: toggle built-in LED on any control command
    if (value == "true" || value == "1" || value == "ON") {
        digitalWrite(LED_PIN, HIGH);
    } else if (value == "false" || value == "0" || value == "OFF") {
        digitalWrite(LED_PIN, LOW);
    }
#endif
}

// ── Setup ───────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  IoT Solution - ESP32 Sensor Board");
    Serial.println("  Device: " + String(DEVICE_ID));
    Serial.println("========================================");

#if LED_ENABLED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif

    // 1. Connect to WiFi
    connectWiFi();

    // 2. Initialize sensors
    sensors.begin();

    // 3. Connect to MQTT broker and subscribe to control topic
    mqtt.begin(onControlCommand);

    Serial.println("[Setup] Ready! Telemetry every " + String(TELEMETRY_INTERVAL / 1000) + "s");
}

// ── Main Loop ───────────────────────────────────────────────

void loop() {
    // Reconnect WiFi if dropped
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Disconnected, reconnecting...");
        connectWiFi();
    }

    // Process MQTT messages
    mqtt.loop();

    unsigned long now = millis();

    // Read sensors at configured interval
    if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = now;
        latestData = sensors.read();

#if LED_ENABLED
        // Brief blink to indicate sensor read
        if (mqtt.isConnected()) {
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
        }
#endif
    }

    // Publish telemetry at configured interval
    if (now - lastTelemetry >= TELEMETRY_INTERVAL) {
        lastTelemetry = now;

        if (latestData.dhtValid || latestData.bmpValid) {
            String json = sensors.toJson(latestData);
            mqtt.publishTelemetry(json);
        } else {
            Serial.println("[Main] No valid sensor data to publish");
        }
    }

    // Publish status heartbeat at configured interval
    if (now - lastStatus >= STATUS_INTERVAL) {
        lastStatus = now;
        mqtt.publishStatus(true);
    }
}
