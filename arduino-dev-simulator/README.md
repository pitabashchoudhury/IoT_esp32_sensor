# Arduino Dev Simulator

Generates **simulated sensor data** and publishes it via MQTT — no real sensors needed. Use this to develop and test the **iot-backend** and **IoTSolution** mobile app without an ESP32 or physical sensors.

Publishes to the same MQTT topics and JSON format as the real ESP32 firmware.

---

## Table of Contents

- [How It Works](#how-it-works)
- [Hardware Requirements](#hardware-requirements)
- [Step 1: Install PlatformIO](#step-1-install-platformio)
- [Step 2: Hardware Setup](#step-2-hardware-setup)
- [Step 3: Find Your USB Port](#step-3-find-your-usb-port)
- [Step 4: Configure the Project](#step-4-configure-the-project)
- [Step 5: Build the Firmware](#step-5-build-the-firmware)
- [Step 6: Flash to Arduino](#step-6-flash-to-arduino)
- [Step 7: Open Serial Monitor](#step-7-open-serial-monitor)
- [Step 8: Verify Data on Broker](#step-8-verify-data-on-broker)
- [Simulated Data Details](#simulated-data-details)
- [MQTT Topics](#mqtt-topics)
- [Project Structure](#project-structure)
- [Troubleshooting](#troubleshooting)

---

## How It Works

```
┌──────────────────────┐          ┌──────────────┐          ┌──────────────┐
│  Arduino Mega 2560   │   MQTT   │  MQTT Broker │   MQTT   │  iot-backend │
│  + Ethernet Shield   │─────────▶│  (HiveMQ)    │─────────▶│ device-service│
│                      │          │  Port: 1883  │          │              │
│  Simulated data:     │          └──────────────┘          └──────┬───────┘
│  - Temperature       │                                          │ WebSocket
│  - Humidity          │                                          │
│  - Pressure          │                                   ┌──────▼───────┐
│  - Altitude          │                                   │  Mobile App  │
│  - Soil moisture     │                                   │ (IoTSolution)│
│  - Light level       │                                   └──────────────┘
│                      │
│  No real sensors!    │
│  Random-walk values  │
└──────────────────────┘
```

The simulator generates **smooth random-walk data** — values drift gradually instead of jumping randomly, so dashboards and charts look realistic during development.

---

## Hardware Requirements

| Component | Purpose | Notes |
|-----------|---------|-------|
| **Arduino Mega 2560** | Microcontroller board | 8KB SRAM — needed for MQTT + JSON buffers |
| **Ethernet Shield (W5100 or W5500)** | Network connectivity | Stacks on top of the Mega |
| **USB-B cable** | Upload firmware + serial monitor | Data cable, not charge-only |
| **LAN cable (RJ45)** | Connect to your router/switch | Standard ethernet cable |

> **Note:** Arduino Uno (2KB SRAM) is too memory-constrained for this project. Use Arduino Mega 2560.

---

## Step 1: Install PlatformIO

### Option A: VS Code + PlatformIO Extension (Recommended)

1. Install **VS Code** from https://code.visualstudio.com
2. Open VS Code, go to **Extensions** sidebar (`Ctrl+Shift+X`)
3. Search for **"PlatformIO IDE"** and click **Install**
4. Wait for the installation to complete (downloads Python, PlatformIO Core, toolchains — takes a few minutes)
5. **Restart VS Code** when prompted

### Option B: PlatformIO CLI Only

```bash
# Install pip if not present
sudo apt install python3-pip

# Install PlatformIO
pip3 install platformio

# Verify installation
pio --version
```

---

## Step 2: Hardware Setup

```
    ┌─────────────────────────────┐
    │      Ethernet Shield        │  ← Stack on top of Arduino Mega
    │    (W5100 or W5500)         │     (align all header pins)
    │                             │
    │   ┌─────────────────┐       │
    │   │     RJ45        │ ←──────── LAN cable to your router/switch
    │   └─────────────────┘       │
    ├─────────────────────────────┤
    │      Arduino Mega 2560      │
    │                             │
    │   ┌─────────────────┐       │
    │   │     USB-B       │ ←──────── USB cable to your PC
    │   └─────────────────┘       │
    └─────────────────────────────┘
```

1. **Stack** the Ethernet Shield on top of the Arduino Mega — align all pins carefully
2. **Plug the LAN cable** from the Ethernet Shield's RJ45 port to your router or switch
3. **Plug the USB cable** from the Arduino Mega to your PC

That's all the wiring needed — no sensors, no breadboard.

---

## Step 3: Find Your USB Port

### Linux

```bash
# Plug in Arduino, then run:
ls /dev/ttyACM*

# Expected output: /dev/ttyACM0
```

If you get **"Permission denied"** when uploading later:

```bash
sudo usermod -a -G dialout $USER
# Log out and log back in for this to take effect
```

### Windows

1. Open **Device Manager**
2. Expand **Ports (COM & LPT)**
3. Look for **"Arduino Mega 2560"** — note the COM port (e.g., `COM3`)

If the board doesn't appear, install the **CH340 driver** (for clone boards).

### macOS

```bash
ls /dev/cu.usbmodem*
```

---

## Step 4: Configure the Project

Edit the file `include/config.h`:

### Required: Set Your Device UUID

```cpp
// Set this to the UUID of the device you registered in the backend
// (via the mobile app or REST API)
#define DEVICE_ID    "paste-your-device-uuid-here"
```

### Required: Set MQTT Broker

```cpp
// Default: public HiveMQ broker (good for testing)
#define MQTT_BROKER    "broker.hivemq.com"
#define MQTT_PORT      1883

// If your broker requires authentication:
#define MQTT_USERNAME  "your-username"
#define MQTT_PASSWORD  "your-password"
```

### Optional: Set Upload Port in `platformio.ini`

```ini
; Uncomment and set your port (from Step 3)
upload_port = /dev/ttyACM0
monitor_port = /dev/ttyACM0
```

### Optional: Adjust Simulated Data Ranges

```cpp
#define TEMP_MIN    20.0    // Min temperature (Celsius)
#define TEMP_MAX    35.0    // Max temperature
#define TEMP_STEP   0.5     // Max change per reading (smooth walk)

#define HUM_MIN     40.0    // Min humidity (%)
#define HUM_MAX     80.0
// ... see config.h for all parameters
```

### Optional: Adjust Timing

```cpp
#define TELEMETRY_INTERVAL  10000   // Publish every 10 seconds
#define STATUS_INTERVAL     30000   // Heartbeat every 30 seconds
```

---

## Step 5: Build the Firmware

### Via VS Code

1. **File → Open Folder** → select the `arduino-dev-simulator/` folder
2. Wait for PlatformIO to finish indexing (progress shown in bottom status bar)
3. Click the **checkmark icon (Build)** in the PlatformIO toolbar at the bottom

### Via CLI

```bash
cd arduino-dev-simulator/

# Build (first run downloads the AVR platform + libraries automatically)
pio run
```

The first build takes a few minutes as it downloads:
- `atmelavr` platform and toolchain
- `Ethernet` library
- `PubSubClient` library

### Expected Output

```
Compiling .pio/build/mega_ethernet/src/main.cpp.o
Linking .pio/build/mega_ethernet/firmware.elf
Checking size .pio/build/mega_ethernet/firmware.elf
Building .pio/build/mega_ethernet/firmware.hex
========================= [SUCCESS] =========================
```

---

## Step 6: Flash to Arduino

### Via VS Code

Click the **right-arrow icon (Upload)** in the PlatformIO toolbar at the bottom.

### Via CLI

```bash
# Auto-detect port
pio run -t upload

# Or specify port explicitly
pio run -t upload --upload-port /dev/ttyACM0
```

### Expected Output

```
Uploading .pio/build/mega_ethernet/firmware.hex
avrdude: AVR device initialized and ready to accept instructions
avrdude: Device signature = 0x1e9801 (probably m2560)
avrdude: writing flash (XXXXX bytes)
avrdude: XXXXX bytes of flash verified
avrdude done.  Thank you.
========================= [SUCCESS] =========================
```

---

## Step 7: Open Serial Monitor

### Via VS Code

Click the **plug icon (Serial Monitor)** in the PlatformIO toolbar at the bottom.

### Via CLI

```bash
pio device monitor
```

### Expected Serial Output

```
========================================
  IoT Dev Simulator - Arduino
  Device: a1b2c3d4-e5f6-7890-abcd-ef1234567890
  Mode: Random sensor data
========================================
[Ethernet] Initializing... IP: 192.168.1.45
[MQTT] Topics:
  Status:    devices/a1b2c3d4-.../status
  Telemetry: devices/a1b2c3d4-.../telemetry
  Control:   devices/a1b2c3d4-.../control
[MQTT] Connecting as iot_dev_sim_a3f2...
[MQTT] Connected!
[MQTT] Subscribed: devices/a1b2c3d4-.../control
[Setup] Ready! Telemetry every 10s
[Telemetry] {"temperature":27.32,"humidity":58.14,"pressure":1013.45,"altitude":302.80,"soilMoisture":48,"lightLevel":2100,"uptimeMs":10000,"simulated":true}
[Status] Heartbeat sent
[Telemetry] {"temperature":27.85,"humidity":57.60,"pressure":1013.12,"altitude":304.20,"soilMoisture":51,"lightLevel":2200,"uptimeMs":20000,"simulated":true}
```

---

## Step 8: Verify Data on Broker

Confirm MQTT messages are flowing using any MQTT client on your PC:

```bash
# Install mosquitto client tools
sudo apt install mosquitto-clients

# Subscribe to your device's telemetry topic
mosquitto_sub -h broker.hivemq.com -t "devices/YOUR_DEVICE_UUID/telemetry"
```

You should see JSON telemetry messages appear every 10 seconds.

### Alternative: Test with MQTT Explorer (GUI)

1. Download **MQTT Explorer** from https://mqtt-explorer.com
2. Connect to `broker.hivemq.com` on port `1883`
3. Look for `devices/YOUR_DEVICE_UUID/telemetry` in the topic tree

---

## Simulated Data Details

### Telemetry JSON Format

```json
{
  "temperature": 27.32,
  "humidity": 58.14,
  "pressure": 1013.45,
  "altitude": 302.80,
  "soilMoisture": 48,
  "lightLevel": 2100,
  "uptimeMs": 10000,
  "simulated": true
}
```

The `"simulated": true` field distinguishes this from real sensor data.

### Random-Walk Behavior

Values don't jump randomly — they **drift smoothly** from a starting point, bounded within configured ranges:

| Field | Range | Max Step per Reading | Starting Value |
|-------|-------|---------------------|----------------|
| `temperature` | 20.0 – 35.0 C | 0.5 | 27.0 |
| `humidity` | 40.0 – 80.0 % | 1.0 | 60.0 |
| `pressure` | 1000.0 – 1025.0 hPa | 0.3 | 1013.0 |
| `altitude` | 100.0 – 500.0 m | 2.0 | 300.0 |
| `soilMoisture` | 20 – 80 % | 3 | 50 |
| `lightLevel` | 500 – 3500 | 100 | 2000 |

This produces realistic-looking graphs for dashboard development.

---

## MQTT Topics

Same topic pattern as the ESP32 firmware, compatible with `device-service`:

```
devices/{deviceId}/status       →  {"is_online": true}        (retained)
devices/{deviceId}/telemetry    →  {sensor JSON}              (every 10s)
devices/{deviceId}/control      ←  "controlId:value"          (from app)
```

| Topic | Direction | Purpose |
|-------|-----------|---------|
| `devices/{id}/status` | Arduino → Broker → Backend | Online/offline heartbeat |
| `devices/{id}/telemetry` | Arduino → Broker → Backend | Simulated sensor readings |
| `devices/{id}/control` | App → Broker → Arduino | Control commands (LED toggle demo) |

### Control Commands

The simulator responds to control commands on the `control` topic.
Send `controlId:value` format — the built-in LED (pin 13) toggles as a demo:

```bash
# Turn LED on
mosquitto_pub -h broker.hivemq.com -t "devices/YOUR_UUID/control" -m "led:true"

# Turn LED off
mosquitto_pub -h broker.hivemq.com -t "devices/YOUR_UUID/control" -m "led:false"
```

---

## Project Structure

```
arduino-dev-simulator/
├── platformio.ini              # Build config: Arduino Mega 2560 + Ethernet Shield
├── include/
│   └── config.h                # Network, MQTT, device, timing, and data range config
├── src/
│   └── main.cpp                # All logic: Ethernet → MQTT → random data → publish
└── README.md                   # This file
```

### Differences from ESP32 Firmware

| Aspect | ESP32 Firmware (root) | Dev Simulator (this folder) |
|--------|----------------------|----------------------------|
| Board | ESP32 | Arduino Mega 2560 |
| Network | WiFi (built-in) | Ethernet Shield (W5100/W5500) |
| Sensors | Real (DHT22, BMP280, etc.) | Simulated (random walk) |
| JSON library | ArduinoJson v7 | Manual `snprintf` + `dtostrf` (saves RAM) |
| Purpose | Production | Development & testing |
| Extra JSON field | — | `"simulated": true` |

---

## Troubleshooting

### Network Issues

| Problem | Cause | Fix |
|---------|-------|-----|
| `DHCP failed!` | No network / router DHCP disabled | Check LAN cable. Ensure router DHCP is enabled. Try static IP in `config.h`. |
| `Shield not found!` | Ethernet Shield not seated properly | Reseat the shield on the Mega. Check all pins are aligned. |
| `Cable not connected!` | LAN cable not plugged in or faulty | Plug LAN cable into Ethernet Shield RJ45 port. Try a different cable. |

### MQTT Issues

| Problem | Cause | Fix |
|---------|-------|-----|
| `MQTT Failed, rc=-2` | Broker unreachable | Check internet connection. Verify `MQTT_BROKER` and `MQTT_PORT` in `config.h`. |
| `MQTT Failed, rc=-4` | Connection timeout | Firewall may be blocking port 1883. Try a different network. |
| `MQTT Failed, rc=5` | Authentication rejected | Check `MQTT_USERNAME` and `MQTT_PASSWORD` in `config.h`. |
| No data in backend | Topic mismatch | Verify `DEVICE_ID` in `config.h` matches the UUID registered in the backend. |

### Upload Issues

| Problem | Cause | Fix |
|---------|-------|-----|
| `Permission denied` on port | Linux user not in `dialout` group | Run `sudo usermod -a -G dialout $USER` then re-login. |
| Port not found | Bad USB cable or driver missing | Try a different USB cable (must be data, not charge-only). Install CH340 driver for clone boards. |
| `avrdude: stk500v2_ReceiveMessage() timeout` | Upload timing issue | Press the **reset button** on the Mega, then immediately click upload. |
| `Error: Could not find board` | Wrong environment | Ensure you opened the `arduino-dev-simulator/` folder, not the parent `iot-arduino/` folder. |

### Memory Issues

| Problem | Cause | Fix |
|---------|-------|-----|
| Board resets randomly | Out of SRAM | Use Arduino Mega (8KB), not Uno (2KB). |
| Garbled serial output | Wrong baud rate | Set serial monitor to `115200` baud. |

---

## Quick Reference

```bash
# Build
pio run

# Flash
pio run -t upload

# Monitor
pio device monitor

# Build + Flash + Monitor (all-in-one)
pio run -t upload && pio device monitor

# Clean build artifacts
pio run -t clean
```
