#include "mqtt_handler.h"

MqttHandler* MqttHandler::_instance = nullptr;

void MqttHandler::begin(ControlCallback onControl) {
    _instance = this;
    _controlCallback = onControl;

    _buildTopics();

    _mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    _mqttClient.setCallback(_messageCallback);
    _mqttClient.setKeepAlive(MQTT_KEEP_ALIVE);
    _mqttClient.setBufferSize(512);

    Serial.println("[MQTT] Broker: " + String(MQTT_BROKER) + ":" + String(MQTT_PORT));
    Serial.println("[MQTT] Topics:");
    Serial.println("  Status:    " + _topicStatus);
    Serial.println("  Telemetry: " + _topicTelemetry);
    Serial.println("  Control:   " + _topicControl);

    _reconnect();
}

void MqttHandler::_buildTopics() {
    String deviceId = String(DEVICE_ID);
    _topicStatus    = String(TOPIC_PREFIX) + deviceId + String(TOPIC_STATUS);
    _topicTelemetry = String(TOPIC_PREFIX) + deviceId + String(TOPIC_TELEMETRY);
    _topicControl   = String(TOPIC_PREFIX) + deviceId + String(TOPIC_CONTROL);

    // Generate unique client ID
    _clientId = String(MQTT_CLIENT_PREFIX) + String(millis());
}

bool MqttHandler::_reconnect() {
    if (_mqttClient.connected()) return true;

    Serial.println("[MQTT] Connecting as " + _clientId + "...");

    // Publish last will: device goes offline if connection drops
    bool connected;
    if (strlen(MQTT_USERNAME) > 0) {
        connected = _mqttClient.connect(
            _clientId.c_str(),
            MQTT_USERNAME,
            MQTT_PASSWORD,
            _topicStatus.c_str(),
            MQTT_QOS,
            true,                          // retained
            "{\"is_online\":false}"        // last will payload
        );
    } else {
        connected = _mqttClient.connect(
            _clientId.c_str(),
            nullptr,
            nullptr,
            _topicStatus.c_str(),
            MQTT_QOS,
            true,
            "{\"is_online\":false}"
        );
    }

    if (connected) {
        Serial.println("[MQTT] Connected!");

        // Publish online status
        publishStatus(true);

        // Subscribe to control commands from backend/mobile app
        _mqttClient.subscribe(_topicControl.c_str(), MQTT_QOS);
        Serial.println("[MQTT] Subscribed to: " + _topicControl);
    } else {
        Serial.print("[MQTT] Connection failed, rc=");
        Serial.println(_mqttClient.state());
    }

    return connected;
}

void MqttHandler::loop() {
    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt > MQTT_RETRY_DELAY) {
            _lastReconnectAttempt = now;
            _reconnect();
        }
    }
    _mqttClient.loop();
}

bool MqttHandler::isConnected() {
    return _mqttClient.connected();
}

void MqttHandler::publishTelemetry(const String& jsonPayload) {
    if (!_mqttClient.connected()) return;

    bool ok = _mqttClient.publish(_topicTelemetry.c_str(), jsonPayload.c_str(), false);
    if (ok) {
        Serial.println("[MQTT] Telemetry sent: " + jsonPayload);
    } else {
        Serial.println("[MQTT] Telemetry publish failed");
    }
}

void MqttHandler::publishStatus(bool isOnline) {
    if (!_mqttClient.connected()) return;

    String payload = isOnline ? "{\"is_online\":true}" : "{\"is_online\":false}";
    _mqttClient.publish(_topicStatus.c_str(), payload.c_str(), true);  // retained
    Serial.println("[MQTT] Status: " + payload);
}

void MqttHandler::publishRaw(const String& topic, const String& payload, bool retained) {
    if (!_mqttClient.connected()) return;
    _mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

void MqttHandler::_messageCallback(char* topic, byte* payload, unsigned int length) {
    if (!_instance) return;

    String topicStr(topic);
    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        payloadStr += (char)payload[i];
    }

    Serial.println("[MQTT] Received on " + topicStr + ": " + payloadStr);

    // Handle control commands from backend/mobile app
    // Expected format from mobile app: "controlId:value"
    // Or JSON: {"controlId":"uuid","command":"set","value":"75"}
    if (topicStr == _instance->_topicControl && _instance->_controlCallback) {
        // Try parsing as "controlId:value" (mobile app format)
        int separatorIdx = payloadStr.indexOf(':');
        if (separatorIdx > 0) {
            String controlId = payloadStr.substring(0, separatorIdx);
            String value = payloadStr.substring(separatorIdx + 1);
            _instance->_controlCallback(controlId, value);
        }
    }
}
