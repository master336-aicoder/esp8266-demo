/*
 * LedStatus - LED状态指示模块
 * 通过不同的闪烁频率表示不同的系统状态
 */

#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>
#include "config.h"

// 系统状态枚举
enum SystemStatus {
    STATUS_IDLE = 0,           // 空闲/初始化
    STATUS_WIFI_CONNECTING,    // 正在连接WiFi
    STATUS_WIFI_CONNECTED,     // WiFi已连接
    STATUS_WIFI_FAILED,        // WiFi连接失败
    STATUS_CONFIG_MODE,        // 配网模式
    STATUS_ERROR               // 错误状态
};

class LedStatus {
public:
    LedStatus() : _currentStatus(STATUS_IDLE), _lastBlinkTime(0), _ledState(false) {}

    void begin() {
        pinMode(LED_PIN, OUTPUT);
        setLed(false);
        DEBUG_PRINTLN("[LED] LED状态模块初始化完成");
    }

    // 设置系统状态，自动调整LED闪烁模式
    void setStatus(SystemStatus status) {
        if (_currentStatus == status) return;

        _currentStatus = status;
        _lastBlinkTime = 0;
        _ledState = false;

        switch (status) {
            case STATUS_IDLE:
                DEBUG_PRINTLN("[LED] 状态: 空闲初始化 (常灭)");
                break;
            case STATUS_WIFI_CONNECTING:
                DEBUG_PRINTLN("[LED] 状态: WiFi连接中 (中闪)");
                break;
            case STATUS_WIFI_CONNECTED:
                DEBUG_PRINTLN("[LED] 状态: WiFi已连接 (慢闪)");
                break;
            case STATUS_WIFI_FAILED:
                DEBUG_PRINTLN("[LED] 状态: WiFi连接失败 (超快闪)");
                break;
            case STATUS_CONFIG_MODE:
                DEBUG_PRINTLN("[LED] 状态: 配网模式 (快闪)");
                break;
            case STATUS_ERROR:
                DEBUG_PRINTLN("[LED] 状态: 错误 (超快闪)");
                break;
        }

        update(); // 立即更新一次
    }

    // 在loop中调用，处理LED闪烁
    void update() {
        int interval = getBlinkInterval();

        if (interval == LED_STEADY_ON) {
            setLed(true);
            return;
        }

        if (interval == LED_STEADY_OFF) {
            setLed(false);
            return;
        }

        unsigned long now = millis();
        if (now - _lastBlinkTime >= (unsigned long)interval) {
            _lastBlinkTime = now;
            _ledState = !_ledState;
            setLed(_ledState);
        }
    }

    // 获取当前状态
    SystemStatus getStatus() const { return _currentStatus; }

    // 获取当前状态的中文描述
    const char* getStatusText() const {
        switch (_currentStatus) {
            case STATUS_IDLE:           return "空闲初始化";
            case STATUS_WIFI_CONNECTING: return "WiFi连接中";
            case STATUS_WIFI_CONNECTED: return "WiFi已连接";
            case STATUS_WIFI_FAILED:    return "WiFi连接失败";
            case STATUS_CONFIG_MODE:    return "配网模式";
            case STATUS_ERROR:          return "错误";
            default:                    return "未知";
        }
    }

private:
    SystemStatus _currentStatus;
    unsigned long _lastBlinkTime;
    bool _ledState;

    // 根据状态获取闪烁间隔
    int getBlinkInterval() const {
        switch (_currentStatus) {
            case STATUS_IDLE:
                return LED_STEADY_OFF;      // 常灭
            case STATUS_WIFI_CONNECTING:
                return BLINK_MEDIUM;         // 中闪 500ms
            case STATUS_WIFI_CONNECTED:
                return BLINK_SLOW;           // 慢闪 2000ms
            case STATUS_WIFI_FAILED:
                return BLINK_VERY_FAST;      // 超快闪 100ms
            case STATUS_CONFIG_MODE:
                return BLINK_FAST;           // 快闪 200ms
            case STATUS_ERROR:
                return BLINK_VERY_FAST;      // 超快闪 100ms
            default:
                return LED_STEADY_OFF;
        }
    }

    // 设置LED状态
    void setLed(bool on) {
        if (LED_ON_LEVEL == HIGH) {
            digitalWrite(LED_PIN, on ? HIGH : LOW);
        } else {
            digitalWrite(LED_PIN, on ? LOW : HIGH);
        }
    }
};

#endif // LED_STATUS_H
