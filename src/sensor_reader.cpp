#include "sensor_reader.h"

void SensorReader::begin() {
    _dht.begin();
    Serial.println("[Sensor] DHT22 initialized on GPIO " + String(DHT_PIN));

#if BMP280_ENABLED
    if (_bmp.begin(BMP280_ADDRESS)) {
        _bmpInitialized = true;
        _bmp.setSampling(
            Adafruit_BMP280::MODE_NORMAL,
            Adafruit_BMP280::SAMPLING_X2,   // temperature
            Adafruit_BMP280::SAMPLING_X16,  // pressure
            Adafruit_BMP280::FILTER_X16,
            Adafruit_BMP280::STANDBY_MS_500
        );
        Serial.println("[Sensor] BMP280 initialized at 0x" + String(BMP280_ADDRESS, HEX));
    } else {
        Serial.println("[Sensor] BMP280 not found at 0x" + String(BMP280_ADDRESS, HEX));
    }
#endif

#if SOIL_MOISTURE_ENABLED
    pinMode(SOIL_MOISTURE_PIN, INPUT);
    Serial.println("[Sensor] Soil moisture on GPIO " + String(SOIL_MOISTURE_PIN));
#endif

#if LDR_ENABLED
    pinMode(LDR_PIN, INPUT);
    Serial.println("[Sensor] LDR on GPIO " + String(LDR_PIN));
#endif
}

SensorData SensorReader::read() {
    SensorData data = {};

    // DHT22: Temperature & Humidity
    float t = _dht.readTemperature();
    float h = _dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
        data.temperature = t;
        data.humidity = h;
        data.dhtValid = true;
    } else {
        Serial.println("[Sensor] DHT read failed");
        data.dhtValid = false;
    }

#if BMP280_ENABLED
    if (_bmpInitialized) {
        data.pressure = _bmp.readPressure() / 100.0F;  // Pa → hPa
        data.altitude = _bmp.readAltitude(1013.25);     // sea-level pressure
        data.bmpValid = true;
    }
#endif

#if SOIL_MOISTURE_ENABLED
    int rawSoil = analogRead(SOIL_MOISTURE_PIN);
    data.soilMoisture = map(rawSoil, SOIL_DRY_VALUE, SOIL_WET_VALUE, 0, 100);
    data.soilMoisture = constrain(data.soilMoisture, 0, 100);
    data.soilValid = true;
#endif

#if LDR_ENABLED
    data.lightLevel = analogRead(LDR_PIN);
    data.ldrValid = true;
#endif

    return data;
}

String SensorReader::toJson(const SensorData& data) {
    JsonDocument doc;

    if (data.dhtValid) {
        doc["temperature"] = serialized(String(data.temperature, 2));
        doc["humidity"] = serialized(String(data.humidity, 2));
    }

    if (data.bmpValid) {
        doc["pressure"] = serialized(String(data.pressure, 2));
        doc["altitude"] = serialized(String(data.altitude, 2));
    }

    if (data.soilValid) {
        doc["soilMoisture"] = data.soilMoisture;
    }

    if (data.ldrValid) {
        doc["lightLevel"] = data.lightLevel;
    }

    doc["uptimeMs"] = millis();
    doc["timestamp"] = millis() / 1000;  // seconds since boot
    doc["freeHeap"] = ESP.getFreeHeap();

    String output;
    serializeJson(doc, output);
    return output;
}
