#ifndef SENSOR_READER_H
#define SENSOR_READER_H

#include <Arduino.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "config.h"

#if BMP280_ENABLED
#include <Adafruit_BMP280.h>
#endif

struct SensorData {
    float temperature;    // Celsius
    float humidity;       // Percentage
    float pressure;       // hPa (from BMP280)
    float altitude;       // meters (from BMP280)
    int   soilMoisture;   // Percentage (0-100)
    int   lightLevel;     // Raw analog value (0-4095)
    bool  dhtValid;
    bool  bmpValid;
    bool  soilValid;
    bool  ldrValid;
};

class SensorReader {
public:
    void begin();
    SensorData read();
    String toJson(const SensorData& data);

private:
    DHT _dht{DHT_PIN, DHT_TYPE};

#if BMP280_ENABLED
    Adafruit_BMP280 _bmp;
    bool _bmpInitialized = false;
#endif
};

#endif // SENSOR_READER_H
