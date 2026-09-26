/*
 * DHTManager - DHT11/DHT22 温湿度传感器管理
 * D1 引脚 (GPIO5)
 */

#ifndef DHT_MANAGER_H
#define DHT_MANAGER_H

#include <Arduino.h>
#include <DHT.h>
#include "config.h"

// DHT 传感器引脚 (D1 = GPIO5)
#define DHT_PIN     5
#define DHT_TYPE    DHT11

class DHTManager {
public:
    DHTManager() : _dht(DHT_PIN, DHT_TYPE),
        _temperature(0.0f), _humidity(0.0f),
        _lastReadTime(0), _readInterval(2000),
        _initialized(false), _valid(false),
        _failedCount(0) {}

    void begin() {
        _dht.begin();
        _initialized = true;
        _lastReadTime = 0;
        DEBUG_PRINTLN("[DHT] DHT传感器初始化完成");
        DEBUG_PRINTF("[DHT] 引脚: GPIO%d (D1), 类型: DHT11\r\n", DHT_PIN);
    }

    void loop() {
        if (!_initialized) return;

        unsigned long now = millis();
        if (now - _lastReadTime >= _readInterval) {
            _lastReadTime = now;
            readSensor();
        }
    }

    float getTemperature() { return _temperature; }
    float getHumidity() { return _humidity; }
    bool isValid() { return _valid; }
    bool isInitialized() { return _initialized; }

private:
    DHT _dht;
    float _temperature;
    float _humidity;
    unsigned long _lastReadTime;
    unsigned long _readInterval;
    bool _initialized;
    bool _valid;
    int _failedCount;

    void readSensor() {
        float t = _dht.readTemperature();
        float h = _dht.readHumidity();

        if (isnan(t) || isnan(h)) {
            _failedCount++;
            if (_failedCount >= 3) {
                _valid = false;
            }
            DEBUG_PRINTLN("[DHT] 读取失败");
            return;
        }

        _temperature = t;
        _humidity = h;
        _valid = true;
        _failedCount = 0;

        DEBUG_PRINTF("[DHT] 温度: %.1f°C, 湿度: %.1f%%\r\n", t, h);
    }
};

#endif // DHT_MANAGER_H
