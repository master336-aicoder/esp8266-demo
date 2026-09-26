/*
 * MqttManager - MQTT客户端管理模块
 * 负责MQTT连接、状态上报、配置存储、上报记录
 */

#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <time.h>
#include "config.h"

// MQTT配置结构体
struct MqttConfig {
    char host[MQTT_HOST_MAX_LENGTH + 1];
    uint16_t port;
    char username[MQTT_USER_MAX_LENGTH + 1];
    char password[MQTT_PASS_MAX_LENGTH + 1];
    char topic[MQTT_TOPIC_MAX_LENGTH + 1];
    char clientId[MQTT_CLIENTID_MAX_LENGTH + 1];  // MQTT客户端ID
    char infoTopic[MQTT_TOPIC_MAX_LENGTH + 1];    // 设备信息上报主题
    uint32_t interval;     // 上报间隔（毫秒）
    uint32_t pinMask;      // 上报引脚掩码（32位，支持GPIO0-16）
    bool enabled;          // 是否启用上报
};

// 上报记录结构体
struct MqttLogEntry {
    uint32_t timestamp;    // 时间戳（秒）
    bool success;          // 是否成功
    char topic[64];        // 主题
    char message[96];      // 消息摘要
};

// GPIO列表（用于上报）- ESP-01S 只有 GPIO0 和 GPIO2
const uint8_t MQTT_GPIO_PINS[] = {0, 2};
const int MQTT_GPIO_COUNT = 2;

#define MQTT_LOG_MAX  20   // 最多保存20条上报记录

class MqttManager {
public:
    MqttManager() : _wifiClient(), _client(_wifiClient), _connected(false),
        _lastPublish(0), _lastReconnectAttempt(0), _logCount(0), _logIndex(0) {}

    void begin() {
        loadConfig();
        _client.setBufferSize(1024); // 增大缓冲区
        DEBUG_PRINTLN("[MQTT] MQTT管理模块初始化完成");
        DEBUG_PRINTF("[MQTT] 服务器: %s:%d\r\n", _config.host, _config.port);
        DEBUG_PRINTF("[MQTT] 用户: %s\r\n", _config.username);
        DEBUG_PRINTF("[MQTT] 主题: %s\r\n", _config.topic);
        DEBUG_PRINTF("[MQTT] 间隔: %d ms\r\n", _config.interval);
        DEBUG_PRINTF("[MQTT] 使能: %s\r\n", _config.enabled ? "是" : "否");
    }

    // 在loop中调用
    void loop() {
        if (!_config.enabled) return;

        if (!_client.connected()) {
            _connected = false;
            unsigned long now = millis();
            if (now - _lastReconnectAttempt > MQTT_RECONNECT_DELAY) {
                _lastReconnectAttempt = now;
                if (connect()) {
                    _lastReconnectAttempt = 0;
                }
            }
        } else {
            _client.loop();
            _connected = true;
        }

        // 定时上报
        if (_connected && _config.enabled) {
            unsigned long now = millis();
            if (now - _lastPublish >= _config.interval) {
                _lastPublish = now;
                publishStatus();
            }
        }
    }

    // 连接MQTT服务器
    bool connect() {
        if (!_config.enabled) return false;
        if (WiFi.status() != WL_CONNECTED) {
            DEBUG_PRINTLN("[MQTT] WiFi未连接，无法连接MQTT");
            return false;
        }

        yield(); // 让WiFi栈处理一下

        DEBUG_PRINTF("[MQTT] 正在连接 %s:%d (用户:%s)...\r\n", _config.host, _config.port, _config.username);

        _client.setServer(_config.host, _config.port);

        // 使用配置的客户端ID，为空则自动生成
        String clientId;
        if (strlen(_config.clientId) > 0) {
            clientId = String(_config.clientId);
        } else {
            clientId = "ESP8266-" + String(ESP.getChipId(), HEX);
        }
        String willTopic = String(_config.topic) + "/status";

        yield(); // 连接前yield防止看门狗

        bool success;
        if (strlen(_config.username) > 0) {
            success = _client.connect(clientId.c_str(), _config.username, _config.password,
                willTopic.c_str(), 1, true, "offline");
        } else {
            success = _client.connect(clientId.c_str(), willTopic.c_str(), 1, true, "offline");
        }

        yield(); // 连接后yield

        if (success) {
            _connected = true;
            DEBUG_PRINTF("[MQTT] 连接成功！Client ID: %s\r\n", clientId.c_str());
            publishOnline();
            addLog(true, _config.topic, "连接成功");
            // 连接成功后自动上报设备信息
            publishDeviceInfo();
        } else {
            _connected = false;
            int state = _client.state();
            const char* errStr = getErrorStr(state);
            DEBUG_PRINTF("[MQTT] 连接失败！状态码: %d - %s\r\n", state, errStr);
            addLog(false, _config.topic, String("连接失败: ") + errStr);
        }

        return success;
    }

    // 获取错误描述
    const char* getErrorStr(int state) {
        switch (state) {
            case -4: return "连接超时";
            case -3: return "连接丢失";
            case -2: return "连接失败";
            case -1: return "断开连接";
            case 0: return "已连接";
            case 1: return "协议版本不支持";
            case 2: return "客户端ID被拒绝";
            case 3: return "服务器不可用";
            case 4: return "用户名或密码错误";
            case 5: return "未授权";
            default: return "未知错误";
        }
    }

    // 断开连接
    void disconnect() {
        if (_client.connected()) {
            publishOffline();
            _client.disconnect();
        }
        _connected = false;
    }

    // 发布状态
    void publishStatus() {
        if (!_connected) {
            addLog(false, _config.topic, "未连接，跳过上报");
            return;
        }

        // 获取client_id
        String clientId;
        if (strlen(_config.clientId) > 0) {
            clientId = String(_config.clientId);
        } else {
            clientId = "ESP8266-" + String(ESP.getChipId(), HEX);
        }

        // 预计算时间字符串（只调用一次）
        time_t nowTs = time(nullptr);
        String timeStr;
        if ((uint32_t)nowTs > 1577836800) {
            timeStr = getTimeStr();
        } else {
            timeStr = "未同步";
        }

        // 构建JSON状态 - 使用预分配减少碎片
        String payload;
        payload.reserve(800); // 预分配足够空间
        payload = "{";
        payload += "\"client_id\":\"" + clientId + "\",";
        payload += "\"chip_id\":\"0x" + String(ESP.getChipId(), HEX) + "\",";
        payload += "\"timestamp\":" + String((uint32_t)nowTs) + ",";
        payload += "\"datetime\":\"" + timeStr + "\",";
        payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        payload += "\"heap\":" + String(ESP.getFreeHeap()) + ",";
        payload += "\"uptime\":" + String(millis() / 1000) + ",";
        payload += "\"gpio\":[";

        bool first = true;
        int pinCount = 0;
        for (int i = 0; i < MQTT_GPIO_COUNT; i++) {
            uint8_t pin = MQTT_GPIO_PINS[i];
            if (!(_config.pinMask & (1UL << pin))) continue;

            if (!first) payload += ",";
            first = false;
            pinCount++;

            int val = digitalRead(pin);
            String mode = getPinModeStr(pin);
            payload += "{";
            payload += "\"pin\":" + String(pin) + ",";
            payload += "\"name\":\"GPIO" + String(pin) + "\",";
            payload += "\"mode\":\"" + mode + "\",";
            payload += "\"value\":" + String(val);
            payload += "}";

            // 安全检查：超过缓冲区限制则停止
            if (payload.length() > 900) {
                payload += "...]"; // 截断标记
                break;
            }
        }

        if (payload.endsWith("...]")) {
            // 已截断
        } else {
            payload += "]}";
        }

        if (payload.length() > 1020) {
            DEBUG_PRINTF("[MQTT] 警告：消息过大 (%d字节)，可能上报失败\r\n", payload.length());
        }

        DEBUG_PRINTF("[MQTT] 上报主题: %s, 长度: %d 字节, 引脚: %d\r\n", _config.topic, payload.length(), pinCount);

        bool ok = _client.publish(_config.topic, payload.c_str());
        if (ok) {
            addLog(true, _config.topic, "上报成功 (" + String(pinCount) + "个引脚)");
        } else {
            DEBUG_PRINTLN("[MQTT] 上报失败");
            addLog(false, _config.topic, "上报失败");
        }
    }

    // 上报设备详细信息
    void publishDeviceInfo() {
        if (!_connected) return;

        // 获取client_id
        String clientId;
        if (strlen(_config.clientId) > 0) {
            clientId = String(_config.clientId);
        } else {
            clientId = "ESP8266-" + String(ESP.getChipId(), HEX);
        }

        // 预计算时间
        time_t nowTs = time(nullptr);
        String timeStr;
        if ((uint32_t)nowTs > 1577836800) {
            timeStr = getTimeStr();
        } else {
            timeStr = "未同步";
        }

        // 构建设备信息JSON - 预分配减少碎片
        String payload;
        payload.reserve(1000);
        payload = "{";
        payload += "\"client_id\":\"" + clientId + "\",";
        payload += "\"chip_id\":\"0x" + String(ESP.getChipId(), HEX) + "\",";
        payload += "\"timestamp\":" + String((uint32_t)nowTs) + ",";
        payload += "\"datetime\":\"" + timeStr + "\",";
        payload += "\"chip_model\":\"ESP8266EX\",";
        payload += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz()) + ",";
        payload += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
        payload += "\"flash_speed\":" + String(ESP.getFlashChipSpeed()) + ",";
        payload += "\"heap_frag\":" + String(ESP.getHeapFragmentation()) + ",";
        payload += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
        payload += "\"sketch_size\":" + String(ESP.getSketchSize()) + ",";
        payload += "\"free_sketch_space\":" + String(ESP.getFreeSketchSpace()) + ",";
        payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        payload += "\"mac\":\"" + WiFi.macAddress() + "\",";
        payload += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
        payload += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        payload += "\"channel\":" + String(WiFi.channel()) + ",";
        payload += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
        payload += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
        payload += "\"firmware\":\"AutoWifiConfig v1.3\",";
        payload += "\"uptime\":" + String(millis() / 1000) + ",";
        payload += "\"gpio_count\":" + String(MQTT_GPIO_COUNT) + ",";
        payload += "\"report_pins\":" + String(_config.pinMask) + ",";
        payload += "\"report_interval\":" + String(_config.interval) + ",";
        payload += "\"status_topic\":\"" + String(_config.topic) + "\"";
        payload += "}";

        String infoTopic = strlen(_config.infoTopic) > 0 ? String(_config.infoTopic) : String(MQTT_DEFAULT_INFO_TOPIC);
        bool ok = _client.publish(infoTopic.c_str(), payload.c_str(), true); // retain=true
        if (ok) {
            DEBUG_PRINTF("[MQTT] 设备信息已上报到 %s\r\n", infoTopic.c_str());
            addLog(true, infoTopic.c_str(), "设备信息上报成功");
        } else {
            DEBUG_PRINTLN("[MQTT] 设备信息上报失败");
            addLog(false, infoTopic.c_str(), "设备信息上报失败");
        }
    }

    // 上线通知
    void publishOnline() {
        if (!_connected) return;
        String topic = String(_config.topic) + "/status";
        _client.publish(topic.c_str(), "online", true);
        DEBUG_PRINTLN("[MQTT] 上线通知已发送");
    }

    // 离线通知
    void publishOffline() {
        if (!_connected) return;
        String topic = String(_config.topic) + "/status";
        _client.publish(topic.c_str(), "offline", true);
        DEBUG_PRINTLN("[MQTT] 离线通知已发送");
    }

    // 获取配置
    MqttConfig getConfig() {
        return _config;
    }

    // 保存配置
    void saveConfig(MqttConfig& config) {
        _config = config;

        // 写入EEPROM
        EEPROM.write(EEPROM_MQTT_MAGIC_ADDR, EEPROM_MQTT_MAGIC);

        for (int i = 0; i < MQTT_HOST_MAX_LENGTH; i++) {
            EEPROM.write(EEPROM_MQTT_HOST_ADDR + i, config.host[i]);
            if (config.host[i] == '\0') break;
        }

        EEPROM.write(EEPROM_MQTT_PORT_ADDR, config.port & 0xFF);
        EEPROM.write(EEPROM_MQTT_PORT_ADDR + 1, (config.port >> 8) & 0xFF);

        for (int i = 0; i < MQTT_USER_MAX_LENGTH; i++) {
            EEPROM.write(EEPROM_MQTT_USER_ADDR + i, config.username[i]);
            if (config.username[i] == '\0') break;
        }

        for (int i = 0; i < MQTT_PASS_MAX_LENGTH; i++) {
            EEPROM.write(EEPROM_MQTT_PASS_ADDR + i, config.password[i]);
            if (config.password[i] == '\0') break;
        }

        for (int i = 0; i < MQTT_TOPIC_MAX_LENGTH; i++) {
            EEPROM.write(EEPROM_MQTT_TOPIC_ADDR + i, config.topic[i]);
            if (config.topic[i] == '\0') break;
        }

        // Client ID
        for (int i = 0; i < MQTT_CLIENTID_MAX_LENGTH; i++) {
            EEPROM.write(EEPROM_MQTT_CLIENTID_ADDR + i, config.clientId[i]);
            if (config.clientId[i] == '\0') break;
        }

        EEPROM.write(EEPROM_MQTT_INTERVAL_ADDR, config.interval & 0xFF);
        EEPROM.write(EEPROM_MQTT_INTERVAL_ADDR + 1, (config.interval >> 8) & 0xFF);
        EEPROM.write(EEPROM_MQTT_INTERVAL_ADDR + 2, (config.interval >> 16) & 0xFF);
        EEPROM.write(EEPROM_MQTT_INTERVAL_ADDR + 3, (config.interval >> 24) & 0xFF);

        EEPROM.write(EEPROM_MQTT_PINS_ADDR, config.pinMask & 0xFF);
        EEPROM.write(EEPROM_MQTT_PINS_ADDR + 1, (config.pinMask >> 8) & 0xFF);
        EEPROM.write(EEPROM_MQTT_PINS_ADDR + 2, (config.pinMask >> 16) & 0xFF);
        EEPROM.write(EEPROM_MQTT_PINS_ADDR + 3, (config.pinMask >> 24) & 0xFF);

        EEPROM.write(EEPROM_MQTT_ENABLE_ADDR, config.enabled ? 1 : 0);

        // 设备信息主题
        for (int i = 0; i < MQTT_TOPIC_MAX_LENGTH; i++) {
            EEPROM.write(EEPROM_MQTT_INFO_TOPIC_ADDR + i, config.infoTopic[i]);
            if (config.infoTopic[i] == '\0') break;
        }

        EEPROM.commit();
        DEBUG_PRINTLN("[MQTT] 配置已保存到EEPROM");

        // 断开重新连接
        if (_client.connected()) {
            disconnect();
        }
        _lastReconnectAttempt = 0;
    }

    // 加载配置
    void loadConfig() {
        if (EEPROM.read(EEPROM_MQTT_MAGIC_ADDR) == EEPROM_MQTT_MAGIC) {
            memset(_config.host, 0, sizeof(_config.host));
            for (int i = 0; i < MQTT_HOST_MAX_LENGTH; i++) {
                _config.host[i] = EEPROM.read(EEPROM_MQTT_HOST_ADDR + i);
                if (_config.host[i] == '\0') break;
            }

            _config.port = EEPROM.read(EEPROM_MQTT_PORT_ADDR);
            _config.port |= (uint16_t)EEPROM.read(EEPROM_MQTT_PORT_ADDR + 1) << 8;

            memset(_config.username, 0, sizeof(_config.username));
            for (int i = 0; i < MQTT_USER_MAX_LENGTH; i++) {
                _config.username[i] = EEPROM.read(EEPROM_MQTT_USER_ADDR + i);
                if (_config.username[i] == '\0') break;
            }

            memset(_config.password, 0, sizeof(_config.password));
            for (int i = 0; i < MQTT_PASS_MAX_LENGTH; i++) {
                _config.password[i] = EEPROM.read(EEPROM_MQTT_PASS_ADDR + i);
                if (_config.password[i] == '\0') break;
            }

            memset(_config.topic, 0, sizeof(_config.topic));
            for (int i = 0; i < MQTT_TOPIC_MAX_LENGTH; i++) {
                _config.topic[i] = EEPROM.read(EEPROM_MQTT_TOPIC_ADDR + i);
                if (_config.topic[i] == '\0') break;
            }

            // Client ID
            memset(_config.clientId, 0, sizeof(_config.clientId));
            for (int i = 0; i < MQTT_CLIENTID_MAX_LENGTH; i++) {
                _config.clientId[i] = EEPROM.read(EEPROM_MQTT_CLIENTID_ADDR + i);
                if (_config.clientId[i] == '\0') break;
            }

            _config.interval = EEPROM.read(EEPROM_MQTT_INTERVAL_ADDR);
            _config.interval |= (uint32_t)EEPROM.read(EEPROM_MQTT_INTERVAL_ADDR + 1) << 8;
            _config.interval |= (uint32_t)EEPROM.read(EEPROM_MQTT_INTERVAL_ADDR + 2) << 16;
            _config.interval |= (uint32_t)EEPROM.read(EEPROM_MQTT_INTERVAL_ADDR + 3) << 24;

            _config.pinMask = EEPROM.read(EEPROM_MQTT_PINS_ADDR);
            _config.pinMask |= (uint32_t)EEPROM.read(EEPROM_MQTT_PINS_ADDR + 1) << 8;
            _config.pinMask |= (uint32_t)EEPROM.read(EEPROM_MQTT_PINS_ADDR + 2) << 16;
            _config.pinMask |= (uint32_t)EEPROM.read(EEPROM_MQTT_PINS_ADDR + 3) << 24;

            _config.enabled = EEPROM.read(EEPROM_MQTT_ENABLE_ADDR) == 1;

            // 设备信息主题
            memset(_config.infoTopic, 0, sizeof(_config.infoTopic));
            for (int i = 0; i < MQTT_TOPIC_MAX_LENGTH; i++) {
                _config.infoTopic[i] = EEPROM.read(EEPROM_MQTT_INFO_TOPIC_ADDR + i);
                if (_config.infoTopic[i] == '\0') break;
            }

            DEBUG_PRINTLN("[MQTT] 已从EEPROM加载配置");
        } else {
            strncpy(_config.host, MQTT_DEFAULT_HOST, MQTT_HOST_MAX_LENGTH);
            _config.port = MQTT_DEFAULT_PORT;
            strncpy(_config.username, MQTT_DEFAULT_USER, MQTT_USER_MAX_LENGTH);
            strncpy(_config.password, MQTT_DEFAULT_PASS, MQTT_PASS_MAX_LENGTH);
            strncpy(_config.topic, MQTT_DEFAULT_TOPIC, MQTT_TOPIC_MAX_LENGTH);
            memset(_config.clientId, 0, sizeof(_config.clientId)); // 空表示自动生成
            strncpy(_config.infoTopic, MQTT_DEFAULT_INFO_TOPIC, MQTT_TOPIC_MAX_LENGTH);
            _config.interval = MQTT_DEFAULT_INTERVAL;
            _config.pinMask = 0;
            _config.enabled = false;

            DEBUG_PRINTLN("[MQTT] 使用默认配置");
        }
    }

    bool isConnected() {
        return _connected && _client.connected();
    }

    int getState() {
        return _client.state();
    }

    // 获取上报记录数量
    int getLogCount() {
        return _logCount > MQTT_LOG_MAX ? MQTT_LOG_MAX : _logCount;
    }

    // 获取指定索引的日志（0=最新）
    MqttLogEntry getLog(int index) {
        int actualIdx = (_logIndex - 1 - index + MQTT_LOG_MAX) % MQTT_LOG_MAX;
        if (index >= getLogCount()) {
            MqttLogEntry empty = {0, false, "", ""};
            return empty;
        }
        return _logs[actualIdx];
    }

    // 添加一条日志
    void addLog(bool success, const char* topic, String message) {
        MqttLogEntry& entry = _logs[_logIndex];
        entry.timestamp = millis() / 1000;
        entry.success = success;
        strncpy(entry.topic, topic, 63);
        entry.topic[63] = '\0';
        strncpy(entry.message, message.c_str(), 95);
        entry.message[95] = '\0';

        _logIndex = (_logIndex + 1) % MQTT_LOG_MAX;
        if (_logCount < MQTT_LOG_MAX) _logCount++;
    }

private:
    WiFiClient _wifiClient;
    PubSubClient _client;
    MqttConfig _config;
    bool _connected;
    unsigned long _lastPublish;
    unsigned long _lastReconnectAttempt;

    // 上报日志
    MqttLogEntry _logs[MQTT_LOG_MAX];
    int _logCount;
    int _logIndex;

    // 获取引脚模式字符串
    String getPinModeStr(uint8_t pin) {
        return getGpioModeStr(pin);
    }

    // 获取当前时间字符串 YYYY-MM-DD HH:MM:SS（纯算法，不用localtime）
    String getTimeStr() {
        time_t now = time(nullptr);
        if ((uint32_t)now < 1577836800) return "未同步";
        char buf[24];
        uint32_t t = (uint32_t)now;
        uint16_t yy = 1970;
        uint8_t mo = 1, dd = 1, hh, mi, ss;

        ss = t % 60; t /= 60;
        mi = t % 60; t /= 60;
        hh = t % 24; t /= 24;

        while (true) {
            uint32_t d = ((yy % 4 == 0 && yy % 100 != 0) || (yy % 400 == 0)) ? 366 : 365;
            if (t < d) break;
            t -= d; yy++;
        }
        static const uint8_t mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        mo = 1;
        while (true) {
            uint8_t d = (mo == 2 && ((yy % 4 == 0 && yy % 100 != 0) || (yy % 400 == 0))) ? 29 : mdays[mo - 1];
            if (t < d) break;
            t -= d; mo++;
        }
        dd = t + 1;
        snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u", yy, mo, dd, hh, mi, ss);
        return String(buf);
    }
};

#endif // MQTT_MANAGER_H
