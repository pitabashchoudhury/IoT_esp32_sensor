#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h"

// Callback type for control commands received from backend/app
typedef void (*ControlCallback)(const String& controlId, const String& value);

class MqttHandler {
public:
    void begin(ControlCallback onControl);
    void loop();
    bool isConnected();

    void publishTelemetry(const String& jsonPayload);
    void publishStatus(bool isOnline);
    void publishRaw(const String& topic, const String& payload, bool retained = false);

private:
    WiFiClient _wifiClient;
    PubSubClient _mqttClient{_wifiClient};
    ControlCallback _controlCallback = nullptr;
    unsigned long _lastReconnectAttempt = 0;

    String _topicStatus;
    String _topicTelemetry;
    String _topicControl;
    String _clientId;

    void _buildTopics();
    bool _reconnect();
    static void _messageCallback(char* topic, byte* payload, unsigned int length);

    static MqttHandler* _instance;
};

#endif // MQTT_HANDLER_H
