# IoT Solution - ESP32 Sensor Firmware

Arduino firmware for ESP32 that reads sensor data and communicates with the **iot-backend** (device-service) and **IoTSolution** mobile app via MQTT.

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [System Flow Diagram](#system-flow-diagram)
- [Sequence Diagrams](#sequence-diagrams)
  - [1. Boot & Connection](#1-boot--connection)
  - [2. Telemetry Publishing](#2-telemetry-publishing)
  - [3. Control Command (App → Device)](#3-control-command-app--device)
  - [4. Device Disconnect (Last Will)](#4-device-disconnect-last-will)
- [Component Diagram](#component-diagram)
- [MQTT Topic Schema](#mqtt-topic-schema)
- [Message Formats](#message-formats)
- [Hardware Setup](#hardware-setup)
- [Configuration](#configuration)
- [Build & Flash](#build--flash)

---

## Architecture Overview

The system consists of **four components** communicating through a central MQTT broker:

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          SYSTEM ARCHITECTURE                                │
│                                                                             │
│  ┌───────────┐       ┌──────────────┐       ┌──────────────┐               │
│  │  ESP32    │       │  MQTT Broker │       │  iot-backend │               │
│  │  Board    │◀─────▶│  (HiveMQ)    │◀─────▶│device-service│               │
│  │          │  MQTT  │  Port: 1883  │  MQTT │  Port: 8082  │               │
│  └───────────┘       └──────────────┘       └──────┬───────┘               │
│   Sensors:                                         │ WebSocket              │
│   - DHT22 (temp/hum)                               │ (STOMP)               │
│   - BMP280 (pressure)                              │                        │
│   - Soil Moisture                           ┌──────▼───────┐               │
│   - LDR (light)                             │  Mobile App  │               │
│                                              │ (IoTSolution)│               │
│   Actuators:                                │  Android/    │               │
│   - LED / Relay                             │  Kotlin      │               │
│   - Motor / Fan                             └──────────────┘               │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Communication Protocols:**

| Path | Protocol | Purpose |
|------|----------|---------|
| ESP32 ↔ Broker | MQTT (TCP:1883) | Sensor data out, control commands in |
| Backend ↔ Broker | MQTT (TCP:1883) | Receives telemetry, sends controls |
| Backend → App | WebSocket (STOMP) | Real-time push to mobile UI |
| App → Backend | REST (HTTP) | Device CRUD, authentication |
| App → Broker | MQTT (TCP:1883) | Direct control commands |

---

## System Flow Diagram

### High-Level Data Flow

```mermaid
flowchart LR
    subgraph ESP32["🔌 ESP32 Board"]
        S1[DHT22<br>Temp & Humidity]
        S2[BMP280<br>Pressure]
        S3[Soil Moisture]
        S4[LDR Light]
        MCU[ESP32 MCU]
        ACT[Actuators<br>LED / Relay]
    end

    subgraph Broker["📡 MQTT Broker<br>broker.hivemq.com:1883"]
        T1["devices/{id}/telemetry"]
        T2["devices/{id}/status"]
        T3["devices/{id}/control"]
    end

    subgraph Backend["🖥️ iot-backend"]
        MS[MqttService<br>subscribes: devices/#]
        DS[DeviceService<br>updates DB]
        WS[WebSocket<br>Notification Service]
        DB[(PostgreSQL<br>device_db)]
    end

    subgraph App["📱 IoTSolution App"]
        VM[DeviceDetail<br>ViewModel]
        UI[Compose UI<br>Dashboard]
        MQTT_C[MqttManager<br>Client]
    end

    S1 & S2 & S3 & S4 --> MCU
    MCU -->|publish| T1
    MCU -->|publish| T2
    T3 -->|subscribe| MCU
    MCU --> ACT

    T1 -->|subscribe| MS
    T2 -->|subscribe| MS
    MS --> DS
    DS --> DB
    MS --> WS

    WS -->|"/topic/devices/{id}/telemetry"| VM
    WS -->|"/topic/devices/{id}/status"| VM
    VM --> UI

    UI -->|user action| MQTT_C
    MQTT_C -->|publish| T3
```

### Detailed Internal Flow (ESP32)

```mermaid
flowchart TD
    START([Power On]) --> SERIAL[Init Serial 115200]
    SERIAL --> WIFI{Connect WiFi}
    WIFI -->|Failed after 20 retries| RESTART([ESP.restart])
    WIFI -->|Connected| SENSORS[Init Sensors<br>DHT22 + BMP280]
    SENSORS --> MQTT_INIT[Init MQTT Client<br>Set Last Will Testament]
    MQTT_INIT --> MQTT_CONN{Connect to Broker}
    MQTT_CONN -->|Failed| RETRY_LATER[Retry in 5s]
    MQTT_CONN -->|Connected| PUB_ONLINE["Publish Status<br>{is_online: true}"]
    PUB_ONLINE --> SUB_CTRL["Subscribe to<br>devices/{id}/control"]
    SUB_CTRL --> LOOP([Enter Main Loop])

    LOOP --> CHECK_WIFI{WiFi OK?}
    CHECK_WIFI -->|No| RECONNECT_WIFI[Reconnect WiFi]
    RECONNECT_WIFI --> CHECK_WIFI
    CHECK_WIFI -->|Yes| MQTT_LOOP[mqtt.loop<br>Process messages]

    MQTT_LOOP --> CHECK_SENSOR{5s elapsed?}
    CHECK_SENSOR -->|Yes| READ[Read all sensors]
    CHECK_SENSOR -->|No| CHECK_TEL

    READ --> CHECK_TEL{10s elapsed?}
    CHECK_TEL -->|Yes| PUBLISH_TEL["Publish Telemetry JSON<br>to devices/{id}/telemetry"]
    CHECK_TEL -->|No| CHECK_STATUS

    PUBLISH_TEL --> CHECK_STATUS{30s elapsed?}
    CHECK_STATUS -->|Yes| PUBLISH_STAT["Publish Heartbeat<br>{is_online: true}"]
    CHECK_STATUS -->|No| LOOP
    PUBLISH_STAT --> LOOP

    RETRY_LATER --> LOOP
```

---

## Sequence Diagrams

### 1. Boot & Connection

```mermaid
sequenceDiagram
    participant ESP as ESP32 Board
    participant WiFi as WiFi Router
    participant Broker as MQTT Broker<br>(HiveMQ)
    participant Backend as device-service
    participant DB as PostgreSQL
    participant WS as WebSocket
    participant App as Mobile App

    Note over ESP: Power On / Reset
    ESP->>ESP: Serial.begin(115200)
    ESP->>ESP: Init LED pin

    rect rgb(220, 240, 255)
        Note over ESP, WiFi: Phase 1: WiFi Connection
        ESP->>WiFi: WiFi.begin(SSID, PASSWORD)
        loop Retry up to 20 times
            WiFi-->>ESP: Status check
        end
        WiFi-->>ESP: WL_CONNECTED (IP assigned)
    end

    rect rgb(220, 255, 220)
        Note over ESP, Broker: Phase 2: Sensor Init
        ESP->>ESP: DHT22.begin() on GPIO 4
        ESP->>ESP: BMP280.begin() on I2C 0x76
    end

    rect rgb(255, 240, 220)
        Note over ESP, App: Phase 3: MQTT Connection
        ESP->>Broker: CONNECT<br>clientId: iot_arduino_{ts}<br>lastWill: devices/{id}/status<br>payload: {"is_online": false}
        Broker-->>ESP: CONNACK (connected)

        ESP->>Broker: PUBLISH devices/{id}/status<br>{"is_online": true} [retained]
        Broker->>Backend: Forward status message

        Backend->>DB: UPDATE device SET is_online=true
        Backend->>WS: Broadcast to /topic/devices/{id}/status

        WS->>App: STOMP MESSAGE<br>{"is_online": true}
        App->>App: UI shows device ONLINE

        ESP->>Broker: SUBSCRIBE devices/{id}/control QoS=1
        Broker-->>ESP: SUBACK
    end

    Note over ESP: ✅ Ready - entering main loop
```

### 2. Telemetry Publishing

```mermaid
sequenceDiagram
    participant Sensor as Sensors<br>(DHT22 + BMP280)
    participant ESP as ESP32
    participant Broker as MQTT Broker
    participant Backend as device-service<br>MqttService
    participant WS as WebSocket<br>NotificationService
    participant App as Mobile App<br>ViewModel

    rect rgb(245, 245, 255)
        Note over Sensor, ESP: Every 5 seconds: Sensor Read
        ESP->>Sensor: readTemperature()
        Sensor-->>ESP: 25.30°C
        ESP->>Sensor: readHumidity()
        Sensor-->>ESP: 62.10%
        ESP->>Sensor: readPressure()
        Sensor-->>ESP: 1013.25 hPa
        ESP->>ESP: Store in SensorData struct
    end

    rect rgb(255, 250, 230)
        Note over ESP, App: Every 10 seconds: Telemetry Publish
        ESP->>ESP: SensorReader.toJson(data)
        Note right of ESP: {"temperature":25.30,<br>"humidity":62.10,<br>"pressure":1013.25,<br>"altitude":45.20,<br>"uptimeMs":120000,<br>"timestamp":120,<br>"freeHeap":245000}

        ESP->>Broker: PUBLISH devices/{id}/telemetry<br>(JSON payload)

        Broker->>Backend: Forward (subscribed to devices/#)

        Note over Backend: Parse topic:<br>devices/{id}/telemetry<br>→ messageType = "telemetry"<br>→ pass-through (no processing)

        Backend->>WS: sendDeviceTelemetry(deviceId, payload)
        WS->>App: STOMP MESSAGE<br>/topic/devices/{id}/telemetry<br>(raw JSON)

        App->>App: Parse JSON, update UI state
        App->>App: Display: 25.3°C  62.1%  1013 hPa
    end

    rect rgb(230, 255, 230)
        Note over ESP, App: Every 30 seconds: Status Heartbeat
        ESP->>Broker: PUBLISH devices/{id}/status<br>{"is_online": true} [retained]
        Broker->>Backend: Forward status
        Backend->>Backend: DeviceEntity.isOnline = true
        Backend->>WS: sendDeviceStatus(deviceId, payload)
        WS->>App: Status confirmed
    end
```

### 3. Control Command (App → Device)

```mermaid
sequenceDiagram
    participant User as 👤 User
    participant App as Mobile App
    participant MQTT_App as App MqttManager
    participant Broker as MQTT Broker
    participant ESP as ESP32
    participant Act as Actuator<br>(LED/Relay)
    participant Backend as device-service
    participant WS as WebSocket

    User->>App: Taps toggle switch ON
    App->>App: Optimistic UI update<br>(switch shows ON immediately)

    App->>MQTT_App: publish(topic, "controlId:true")
    MQTT_App->>Broker: PUBLISH devices/{id}/control<br>"led-uuid:true"

    par Delivered to ESP32
        Broker->>ESP: MESSAGE devices/{id}/control<br>"led-uuid:true"
        ESP->>ESP: _messageCallback() fires
        ESP->>ESP: Parse "led-uuid:true"<br>controlId = "led-uuid"<br>value = "true"
        ESP->>ESP: onControlCommand(controlId, value)
        ESP->>Act: digitalWrite(LED_PIN, HIGH)
        Note over Act: 💡 LED turns ON
    and Delivered to Backend
        Broker->>Backend: MESSAGE devices/{id}/control
        Backend->>WS: sendDeviceControl(deviceId, payload)
        WS->>App: Confirm via WebSocket
    end
```

### 4. Device Disconnect (Last Will)

```mermaid
sequenceDiagram
    participant ESP as ESP32
    participant Broker as MQTT Broker
    participant Backend as device-service
    participant DB as PostgreSQL
    participant WS as WebSocket
    participant App as Mobile App

    Note over ESP: ⚡ Power lost / WiFi dies /<br>unexpected disconnect

    ESP--xBroker: Connection lost<br>(no DISCONNECT packet)

    Note over Broker: Keep-alive timeout (60s)<br>No PINGREQ received<br>→ Client considered dead

    rect rgb(255, 230, 230)
        Note over Broker, App: Broker triggers Last Will Testament
        Broker->>Broker: Retrieve stored Last Will:<br>topic: devices/{id}/status<br>payload: {"is_online": false}<br>retained: true

        Broker->>Backend: PUBLISH devices/{id}/status<br>{"is_online": false}

        Backend->>Backend: Parse: messageType = "status"<br>Extract is_online = false
        Backend->>DB: UPDATE device<br>SET is_online = false

        Backend->>WS: sendDeviceStatus(deviceId,<br>{"is_online": false})
        WS->>App: STOMP MESSAGE<br>{"is_online": false}

        App->>App: UI shows device OFFLINE 🔴
    end

    Note over ESP: Later... power restored

    rect rgb(230, 255, 230)
        Note over ESP, App: ESP32 reconnects
        ESP->>Broker: CONNECT (new session)
        Broker-->>ESP: CONNACK
        ESP->>Broker: PUBLISH devices/{id}/status<br>{"is_online": true} [retained]
        Broker->>Backend: Forward
        Backend->>DB: SET is_online = true
        Backend->>WS: Broadcast
        WS->>App: Device back ONLINE 🟢
    end
```

---

## Component Diagram

```mermaid
graph TB
    subgraph ESP32_Firmware["ESP32 Firmware (iot-arduino)"]
        MAIN["main.cpp<br>─────────<br>setup()<br>loop()<br>onControlCommand()"]
        SR["sensor_reader.cpp<br>─────────<br>begin()<br>read() → SensorData<br>toJson() → String"]
        MH["mqtt_handler.cpp<br>─────────<br>begin()<br>loop()<br>publishTelemetry()<br>publishStatus()<br>_messageCallback()"]
        CFG["config.h<br>─────────<br>WiFi credentials<br>MQTT broker<br>Device UUID<br>GPIO pins<br>Timing intervals"]
    end

    subgraph Hardware["Hardware Layer"]
        DHT[DHT22<br>GPIO 4]
        BMP[BMP280<br>I2C 0x76]
        SOIL[Soil Moisture<br>GPIO 34]
        LDR_S[LDR<br>GPIO 35]
        LED_S[Built-in LED<br>GPIO 2]
        RELAY[Relay/Motor<br>Custom GPIO]
    end

    subgraph Libraries["Libraries (PlatformIO)"]
        PAHO[PubSubClient<br>MQTT Client]
        AJSON[ArduinoJson v7<br>JSON Serialization]
        ADHT[Adafruit DHT<br>Sensor Driver]
        ABMP[Adafruit BMP280<br>Sensor Driver]
    end

    MAIN --> SR
    MAIN --> MH
    MAIN --> CFG
    SR --> CFG
    MH --> CFG

    SR --> DHT
    SR --> BMP
    SR --> SOIL
    SR --> LDR_S
    MAIN --> LED_S
    MAIN --> RELAY

    MH --> PAHO
    SR --> AJSON
    SR --> ADHT
    SR --> ABMP
```

---

## MQTT Topic Schema

All topics follow the pattern expected by `device-service/MqttService.java`:

```
devices/{deviceId}/{messageType}
   │         │           │
   │         │           ├── status      ESP32 → Backend  (online/offline)
   │         │           ├── telemetry   ESP32 → Backend  (sensor readings)
   │         │           └── control     App   → ESP32    (commands)
   │         │
   │         └── UUID from backend (e.g., "a1b2c3d4-e5f6-...")
   │
   └── Fixed prefix
```

| Topic | Direction | Publisher | Subscriber | QoS | Retained |
|-------|-----------|-----------|------------|-----|----------|
| `devices/{id}/status` | ESP32 → Backend | ESP32 | device-service | 1 | Yes |
| `devices/{id}/telemetry` | ESP32 → Backend | ESP32 | device-service | 1 | No |
| `devices/{id}/control` | App → ESP32 | Mobile App | ESP32 | 1 | No |

---

## Message Formats

### Status Message
```json
{
  "is_online": true
}
```
Published on connect, every 30s as heartbeat, and `false` via Last Will on disconnect.

### Telemetry Message
```json
{
  "temperature": 25.30,
  "humidity": 62.10,
  "pressure": 1013.25,
  "altitude": 45.20,
  "soilMoisture": 72,
  "lightLevel": 3200,
  "uptimeMs": 120000,
  "timestamp": 120,
  "freeHeap": 245000
}
```
Fields are conditionally included based on which sensors are enabled and returning valid data.

### Control Command (received by ESP32)
```
controlId:value
```
Examples: `led-uuid:true`, `fan-speed-uuid:75`, `mode-uuid:auto`

---

## Hardware Setup

### Wiring Diagram

```
                    ┌─────────────────────┐
                    │      ESP32 Board     │
                    │                      │
    DHT22 ─────────┤ GPIO 4         GPIO 2├──── Built-in LED
    (Data)          │                      │
                    │ GPIO 21 (SDA)        │
    BMP280 ─────────┤ GPIO 22 (SCL)       │
    (I2C)           │                      │
                    │ GPIO 34 (ADC)        │
    Soil Sensor ────┤ (Analog In)          │
                    │                      │
                    │ GPIO 35 (ADC)        │
    LDR ────────────┤ (Analog In)          │
                    │                      │
                    │ 3.3V ────── VCC (sensors)
                    │ GND  ────── GND (sensors)
                    └─────────────────────┘
```

### Pin Mapping

| Component | ESP32 Pin | Type | Notes |
|-----------|-----------|------|-------|
| DHT22 Data | GPIO 4 | Digital | 10K pull-up to 3.3V |
| BMP280 SDA | GPIO 21 | I2C | Built-in I2C bus |
| BMP280 SCL | GPIO 22 | I2C | Built-in I2C bus |
| Soil Moisture | GPIO 34 | Analog | ADC input only |
| LDR | GPIO 35 | Analog | ADC input only |
| LED (built-in) | GPIO 2 | Digital | Active HIGH |

---

## Configuration

Edit `include/config.h` before flashing:

### Required Settings

```cpp
// Your WiFi network
#define WIFI_SSID       "MyWiFi"
#define WIFI_PASSWORD   "MyPassword"

// Device UUID from backend (create device via mobile app first)
#define DEVICE_ID       "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
```

### Optional: Enable/Disable Sensors

```cpp
#define BMP280_ENABLED         true    // I2C pressure sensor
#define SOIL_MOISTURE_ENABLED  false   // Analog soil sensor
#define LDR_ENABLED            false   // Analog light sensor
```

### Optional: Adjust Timing

```cpp
#define TELEMETRY_INTERVAL   10000  // Publish sensor data every 10s
#define STATUS_INTERVAL      30000  // Heartbeat every 30s
#define SENSOR_READ_INTERVAL  5000  // Read sensors every 5s
```

---

## Build & Flash

### Prerequisites

1. Install [PlatformIO CLI](https://platformio.org/install/cli) or [VS Code Extension](https://platformio.org/install/ide?install=vscode)
2. Connect ESP32 board via USB

### Steps

```bash
# Navigate to project
cd iot-arduino

# Build firmware
pio run

# Flash to ESP32
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

### Expected Serial Output

```
========================================
  IoT Solution - ESP32 Sensor Board
  Device: a1b2c3d4-e5f6-7890-abcd-ef1234567890
========================================
[WiFi] Connecting to MyWiFi.....
[WiFi] Connected! IP: 192.168.1.105
[Sensor] DHT22 initialized on GPIO 4
[Sensor] BMP280 initialized at 0x76
[MQTT] Broker: broker.hivemq.com:1883
[MQTT] Connecting as iot_arduino_12345...
[MQTT] Connected!
[MQTT] Status: {"is_online":true}
[MQTT] Subscribed to: devices/a1b2c3d4-.../control
[Setup] Ready! Telemetry every 10s
[MQTT] Telemetry sent: {"temperature":25.30,"humidity":62.10,...}
```

---

## Setup Checklist

1. **Backend**: Register a device via REST API or mobile app → get the device UUID
2. **Config**: Set `DEVICE_ID` in `config.h` to that UUID
3. **Config**: Set `WIFI_SSID` and `WIFI_PASSWORD`
4. **Wire**: Connect sensors to ESP32 per pin mapping above
5. **Flash**: `pio run --target upload`
6. **Verify**: Open serial monitor, confirm MQTT connected
7. **App**: Open IoTSolution → device should show ONLINE with live telemetry
# IoT_esp32_sensor
