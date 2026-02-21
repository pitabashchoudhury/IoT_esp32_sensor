// ============================================================
//  IoT Dev Simulator - Arduino Mega + Ethernet Shield
//  Generates smooth random-walk sensor data and publishes
//  to the same MQTT topics as the real ESP32 firmware.
//  Use this for backend / mobile-app development without
//  needing actual sensors.
//
//  Compatible topic pattern:
//    devices/{deviceId}/status
//    devices/{deviceId}/telemetry
//    devices/{deviceId}/control
// ============================================================

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include "config.h"

// ── Networking ──────────────────────────────────────────────

byte mac[] = MAC_ADDRESS;

#if !USE_DHCP
IPAddress ip(STATIC_IP);
IPAddress gateway(GATEWAY_IP);
IPAddress subnet(SUBNET_MASK);
IPAddress dns_server(DNS_SERVER);
#endif

EthernetClient ethClient;
PubSubClient   mqttClient(ethClient);

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

// Smooth random walk: value drifts by at most ±step each tick
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

// ── MQTT ────────────────────────────────────────────────────

void buildTopics() {
    topicStatus    = String(TOPIC_PREFIX) + DEVICE_ID + TOPIC_STATUS;
    topicTelemetry = String(TOPIC_PREFIX) + DEVICE_ID + TOPIC_TELEMETRY;
    topicControl   = String(TOPIC_PREFIX) + DEVICE_ID + TOPIC_CONTROL;
    clientId       = String(MQTT_CLIENT_PREFIX) + String(random(0xFFFF), HEX);
}

void onMessage(char* topic, byte* payload, unsigned int length) {
    Serial.print(F("[MQTT] Received on "));
    Serial.print(topic);
    Serial.print(F(": "));
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
        Serial.print(F("[Control] ID="));
        Serial.print(controlId);
        Serial.print(F("  Value="));
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

    Serial.print(F("[MQTT] Connecting as "));
    Serial.print(clientId);
    Serial.println(F("..."));

    bool ok;
    if (strlen(MQTT_USERNAME) > 0) {
        ok = mqttClient.connect(
            clientId.c_str(),
            MQTT_USERNAME, MQTT_PASSWORD,
            topicStatus.c_str(), MQTT_QOS, true,
            "{\"is_online\":false}");
    } else {
        ok = mqttClient.connect(
            clientId.c_str(),
            nullptr, nullptr,
            topicStatus.c_str(), MQTT_QOS, true,
            "{\"is_online\":false}");
    }

    if (ok) {
        Serial.println(F("[MQTT] Connected!"));
        mqttClient.publish(topicStatus.c_str(), "{\"is_online\":true}", true);
        mqttClient.subscribe(topicControl.c_str(), MQTT_QOS);
        Serial.print(F("[MQTT] Subscribed: "));
        Serial.println(topicControl);
    } else {
        Serial.print(F("[MQTT] Failed, rc="));
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
    // dtostrf for AVR float-to-string (sprintf %f not supported on AVR)
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
    Serial.println(F("========================================"));
    Serial.println(F("  IoT Dev Simulator - Arduino"));
    Serial.print(F("  Device: "));
    Serial.println(F(DEVICE_ID));
    Serial.println(F("  Mode: Random sensor data"));
    Serial.println(F("========================================"));

#if LED_ENABLED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
#endif

    // Seed RNG from floating analog pin
    randomSeed(analogRead(A0));

    // ── Ethernet ────────────────────────────────────────────
    Serial.print(F("[Ethernet] Initializing... "));

#if USE_DHCP
    if (Ethernet.begin(mac) == 0) {
        Serial.println(F("DHCP failed!"));
        // Check for hardware
        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            Serial.println(F("[Ethernet] Shield not found!"));
        }
        if (Ethernet.linkStatus() == LinkOFF) {
            Serial.println(F("[Ethernet] Cable not connected!"));
        }
        Serial.println(F("Halting."));
        while (true) { delay(1000); }
    }
#else
    Ethernet.begin(mac, ip, dns_server, gateway, subnet);
#endif

    Serial.print(F("IP: "));
    Serial.println(Ethernet.localIP());

    // ── MQTT ────────────────────────────────────────────────
    buildTopics();
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(onMessage);
    mqttClient.setBufferSize(512);

    Serial.println(F("[MQTT] Topics:"));
    Serial.print(F("  Status:    ")); Serial.println(topicStatus);
    Serial.print(F("  Telemetry: ")); Serial.println(topicTelemetry);
    Serial.print(F("  Control:   ")); Serial.println(topicControl);

    mqttConnect();

    Serial.print(F("[Setup] Ready! Telemetry every "));
    Serial.print(TELEMETRY_INTERVAL / 1000);
    Serial.println(F("s"));
}

// ── Loop ────────────────────────────────────────────────────

void loop() {
    // Maintain DHCP lease
    Ethernet.maintain();

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
            Serial.print(F("[Telemetry] "));
            Serial.println(json);

#if LED_ENABLED
            digitalWrite(LED_PIN, HIGH);
            delay(50);
            digitalWrite(LED_PIN, LOW);
#endif
        } else {
            Serial.println(F("[Telemetry] Publish failed"));
        }
    }

    // ── Publish status heartbeat ────────────────────────────
    if (now - lastStatus >= STATUS_INTERVAL) {
        lastStatus = now;
        mqttClient.publish(topicStatus.c_str(), "{\"is_online\":true}", true);
        Serial.println(F("[Status] Heartbeat sent"));
    }
}
