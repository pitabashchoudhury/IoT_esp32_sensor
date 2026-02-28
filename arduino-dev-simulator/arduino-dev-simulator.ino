// ============================================================
//  IoT Dev Simulator - Arduino UNO WiFi R4
//  Generates smooth random-walk sensor data and publishes
//  to the same MQTT topics as the real ESP32 firmware.
//  Use this for backend / mobile-app development without
//  needing actual sensors.
//
//  Compatible topic pattern:
//    devices/{deviceId}/status
//    devices/{deviceId}/telemetry
//    devices/{deviceId}/control
//
//  Board: Arduino UNO R4 WiFi
//  Libraries needed (install via Library Manager):
//    - PubSubClient by Nick O'Leary
// ============================================================

#include <WiFiS3.h>
#include <PubSubClient.h>
#include "config.h"

// ── Networking ──────────────────────────────────────────────
// WiFiSSLClient provides TLS encryption (required by HiveMQ Cloud)

WiFiSSLClient wifiClient;
PubSubClient mqttClient(wifiClient);

// ── MQTT Topics ─────────────────────────────────────────────

String topicStatus;
String topicTelemetry;
String topicControl;
String clientId;

// ── Timing ──────────────────────────────────────────────────

unsigned long lastTelemetry = 0;
unsigned long lastStatus    = 0;
unsigned long lastReconnect = 0;

// ── Simulated Sensor State ──────────────────────────────────
// Values "walk" gradually so charts look realistic

float simTemp     = 27.0;
float simHumidity = 60.0;
float simPressure = 1013.0;
float simAltitude = 300.0;
int   simSoil     = 50;
int   simLight    = 2000;

// ── Helpers ─────────────────────────────────────────────────

float randomFloat(float minVal, float maxVal) {
    return minVal + (random(10001) / 10000.0) * (maxVal - minVal);
}

// Smooth random walk: value drifts by at most +/-step each tick
float walkFloat(float current, float minVal, float maxVal, float maxStep) {
    float step = randomFloat(-maxStep, maxStep);
    float next = current + step;
    if (next < minVal) next = minVal;
    if (next > maxVal) next = maxVal;
    return next;
}

int walkInt(int current, int minVal, int maxVal, int maxStep) {
    int step = random(-maxStep, maxStep + 1);
    int next = current + step;
    if (next < minVal) next = minVal;
    if (next > maxVal) next = maxVal;
    return next;
}

// ── WiFi ────────────────────────────────────────────────────

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.print("[WiFi] Connecting to ");
    Serial.print(WIFI_SSID);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println(" Connected!");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println(" Failed!");
        Serial.println("[WiFi] Could not connect. Check SSID and password.");
    }
}

// ── MQTT ────────────────────────────────────────────────────

void buildTopics() {
    topicStatus    = String(TOPIC_PREFIX) + DEVICE_ID + TOPIC_STATUS;
    topicTelemetry = String(TOPIC_PREFIX) + DEVICE_ID + TOPIC_TELEMETRY;
    topicControl   = String(TOPIC_PREFIX) + DEVICE_ID + TOPIC_CONTROL;
    clientId       = String(MQTT_CLIENT_PREFIX) + String(random(0xFFFF), HEX);
}

void onMessage(char* topic, byte* payload, unsigned int length) {
    Serial.print("[MQTT] Received on ");
    Serial.print(topic);
    Serial.print(": ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Parse control commands (format: "controlId:value")
    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    int sep = payloadStr.indexOf(':');
    if (sep > 0) {
        String controlId = payloadStr.substring(0, sep);
        String value     = payloadStr.substring(sep + 1);
        Serial.print("[Control] ID=");
        Serial.print(controlId);
        Serial.print("  Value=");
        Serial.println(value);

#if LED_ENABLED
        if (value == "true" || value == "1" || value == "ON") {
            digitalWrite(LED_PIN, HIGH);
        } else if (value == "false" || value == "0" || value == "OFF") {
            digitalWrite(LED_PIN, LOW);
        }
#endif
    }
}

bool mqttConnect() {
    if (mqttClient.connected()) return true;

    Serial.print("[MQTT] Connecting as ");
    Serial.print(clientId);
    Serial.println("...");

    // HiveMQ Cloud requires username + password
    bool ok = mqttClient.connect(
        clientId.c_str(),
        MQTT_USERNAME, MQTT_PASSWORD_STR,
        topicStatus.c_str(), MQTT_QOS, true,
        "{\"is_online\":false}");

    if (ok) {
        Serial.println("[MQTT] Connected!");
        mqttClient.publish(topicStatus.c_str(), "{\"is_online\":true}", true);
        mqttClient.subscribe(topicControl.c_str(), MQTT_QOS);
        Serial.print("[MQTT] Subscribed: ");
        Serial.println(topicControl);
    } else {
        Serial.print("[MQTT] Failed, rc=");
        Serial.println(mqttClient.state());
    }
    return ok;
}

// ── Telemetry ───────────────────────────────────────────────

void updateSimulatedData() {
    simTemp     = walkFloat(simTemp,     TEMP_MIN,  TEMP_MAX,  TEMP_STEP);
    simHumidity = walkFloat(simHumidity, HUM_MIN,   HUM_MAX,   HUM_STEP);
    simPressure = walkFloat(simPressure, PRES_MIN,  PRES_MAX,  PRES_STEP);
    simAltitude = walkFloat(simAltitude, ALT_MIN,   ALT_MAX,   ALT_STEP);
    simSoil     = walkInt(simSoil,       SOIL_MIN,  SOIL_MAX,  SOIL_STEP);
    simLight    = walkInt(simLight,      LIGHT_MIN, LIGHT_MAX, LIGHT_STEP);
}

String buildTelemetryJson() {
    char tStr[8], hStr[8], pStr[8], aStr[8];
    dtostrf(simTemp,     1, 2, tStr);
    dtostrf(simHumidity, 1, 2, hStr);
    dtostrf(simPressure, 1, 2, pStr);
    dtostrf(simAltitude, 1, 2, aStr);

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"temperature\":%s,\"humidity\":%s,\"pressure\":%s,"
        "\"altitude\":%s,\"soilMoisture\":%d,\"lightLevel\":%d,"
        "\"uptimeMs\":%lu,\"simulated\":true}",
        tStr, hStr, pStr, aStr, simSoil, simLight, millis());

    return String(buf);
}

// ── Setup ───────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  IoT Dev Simulator - Arduino UNO R4 WiFi");
    Serial.print("  Device: ");
    Serial.println(DEVICE_ID);
    Serial.println("  Mode: Random sensor data over WiFi");
    Serial.println("========================================");

#if LED_ENABLED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif

    // Seed RNG from floating analog pin
    randomSeed(analogRead(A0));

    // ── WiFi ────────────────────────────────────────────────
    connectWiFi();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Setup] WiFi not connected. Halting.");
        while (true) { delay(1000); }
    }

    // ── MQTT ────────────────────────────────────────────────
    buildTopics();
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(onMessage);
    mqttClient.setBufferSize(512);

    Serial.println("[MQTT] Topics:");
    Serial.print("  Status:    "); Serial.println(topicStatus);
    Serial.print("  Telemetry: "); Serial.println(topicTelemetry);
    Serial.print("  Control:   "); Serial.println(topicControl);

    mqttConnect();

    Serial.print("[Setup] Ready! Telemetry every ");
    Serial.print(TELEMETRY_INTERVAL / 1000);
    Serial.println("s");
}

// ── Loop ────────────────────────────────────────────────────

void loop() {
    // Reconnect WiFi if dropped
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Connection lost. Reconnecting...");
        connectWiFi();
    }

    // MQTT reconnect with backoff
    if (!mqttClient.connected()) {
        unsigned long now = millis();
        if (now - lastReconnect > MQTT_RETRY_DELAY) {
            lastReconnect = now;
            mqttConnect();
        }
    }
    mqttClient.loop();

    unsigned long now = millis();

    // ── Publish telemetry ───────────────────────────────────
    if (now - lastTelemetry >= TELEMETRY_INTERVAL) {
        lastTelemetry = now;

        updateSimulatedData();
        String json = buildTelemetryJson();

        if (mqttClient.publish(topicTelemetry.c_str(), json.c_str())) {
            Serial.print("[Telemetry] ");
            Serial.println(json);

#if LED_ENABLED
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
#endif
        } else {
            Serial.println("[Telemetry] Publish failed");
        }
    }

    // ── Publish status heartbeat ────────────────────────────
    if (now - lastStatus >= STATUS_INTERVAL) {
        lastStatus = now;
        mqttClient.publish(topicStatus.c_str(), "{\"is_online\":true}", true);
        Serial.println("[Status] Heartbeat sent");
    }
}
