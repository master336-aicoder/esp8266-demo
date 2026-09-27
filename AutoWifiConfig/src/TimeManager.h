/*
 * TimeManager - NTP网络时间同步模块
 * 纯算法实现时间格式化，不依赖localtime（避免ESP8266上的崩溃问题）
 */

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "config.h"

// NTP配置结构体
struct NtpConfig {
    char server1[32];  // NTP服务器1
    char server2[24];  // NTP服务器2
    char server3[24];  // NTP服务器3
    int8_t timezone;   // 时区（小时）
    uint32_t interval; // 同步间隔（毫秒）
};

class TimeManager {
public:
    TimeManager() : _synced(false), _lastSync(0), _lastAttempt(0), _timestamp(0), _lastTickMs(0) {
        memset(&_config, 0, sizeof(_config));
        strncpy(_config.server1, NTP_DEFAULT_SERVER1, 31);
        strncpy(_config.server2, NTP_DEFAULT_SERVER2, 23);
        strncpy(_config.server3, NTP_DEFAULT_SERVER3, 23);
        _config.timezone = NTP_DEFAULT_TIMEZONE;
        _config.interval = NTP_DEFAULT_INTERVAL;
    }

    void begin() {
        loadConfig();
        applyNtpConfig();
        DEBUG_PRINTLN("[时间] NTP模块初始化完成");
    }

    void applyNtpConfig() {
        configTime(_config.timezone * 3600, 0, _config.server1, _config.server2, _config.server3);
    }

    void loop() {
        if (WiFi.status() != WL_CONNECTED) return;

        unsigned long now = millis();

        if (!_synced && (now - _lastAttempt > 5000)) {
            _lastAttempt = now;
            trySync();
            return;
        }

        if (_synced && (now - _lastSync > _config.interval)) {
            DEBUG_PRINTLN("[时间] 定时同步...");
            trySync();
        }

        // 已同步时用 millis 累加更新时间戳，减少 time() 系统调用
        if (_synced) {
            unsigned long ms = millis();
            if (ms - _lastTickMs >= 1000) {
                _timestamp += (ms - _lastTickMs) / 1000;
                _lastTickMs = ms - (ms - _lastTickMs) % 1000;
            }
        }
    }

    bool forceSync() {
        if (WiFi.status() != WL_CONNECTED) return false;
        _synced = false;
        _lastAttempt = 0;
        return trySync();
    }

    bool trySync() {
        if (WiFi.status() != WL_CONNECTED) return false;

        time_t now;
        time(&now);

        // 大于2020年视为有效
        if (now > 1577836800) {
            _timestamp = (uint32_t)now;
            _synced = true;
            _lastSync = millis();
            DEBUG_PRINTF("[时间] 同步成功: %s\r\n", getDateTimeStr().c_str());
            return true;
        }
        return false;
    }

    bool isSynced() { return _synced; }
    uint32_t getTimestamp() { return _timestamp; }

    // 纯算法实现时间格式化，不依赖 localtime
    // _timestamp 存储 UTC 时间戳，显示时加上时区偏移
    String getDateTimeStr() {
        if (!_synced) return String("未同步");
        char buf[24];
        uint32_t localTs = _timestamp + _config.timezone * 3600;
        formatDateTime(buf, sizeof(buf), localTs);
        return String(buf);
    }

    String getDateStr() {
        if (!_synced) return String("未同步");
        char buf[16];
        uint32_t localTs = _timestamp + _config.timezone * 3600;
        formatDate(buf, sizeof(buf), localTs);
        return String(buf);
    }

    String getTimeStr() {
        if (!_synced) return String("--:--:--");
        char buf[12];
        uint32_t localTs = _timestamp + _config.timezone * 3600;
        formatTimeOnly(buf, sizeof(buf), localTs);
        return String(buf);
    }

    String formatTime(uint32_t timestamp) {
        if (timestamp < 1577836800) return String("N/A");
        char buf[24];
        uint32_t localTs = timestamp + _config.timezone * 3600;
        formatDateTime(buf, sizeof(buf), localTs);
        return String(buf);
    }

    NtpConfig getConfig() { return _config; }

    void saveConfig(NtpConfig& config) {
        _config = config;
        EEPROM.write(EEPROM_NTP_MAGIC_ADDR, EEPROM_NTP_MAGIC);

        for (int i = 0; i < 31; i++) {
            EEPROM.write(EEPROM_NTP_SERVER1_ADDR + i, config.server1[i]);
            if (config.server1[i] == '\0') break;
        }
        for (int i = 0; i < 23; i++) {
            EEPROM.write(EEPROM_NTP_SERVER2_ADDR + i, config.server2[i]);
            if (config.server2[i] == '\0') break;
        }
        for (int i = 0; i < 23; i++) {
            EEPROM.write(EEPROM_NTP_SERVER3_ADDR + i, config.server3[i]);
            if (config.server3[i] == '\0') break;
        }

        EEPROM.write(EEPROM_NTP_TZ_ADDR, (uint8_t)(config.timezone + 12));
        EEPROM.write(EEPROM_NTP_INTERVAL_ADDR, config.interval & 0xFF);
        EEPROM.write(EEPROM_NTP_INTERVAL_ADDR + 1, (config.interval >> 8) & 0xFF);
        EEPROM.write(EEPROM_NTP_INTERVAL_ADDR + 2, (config.interval >> 16) & 0xFF);
        EEPROM.write(EEPROM_NTP_INTERVAL_ADDR + 3, (config.interval >> 24) & 0xFF);

        EEPROM.commit();
        applyNtpConfig();
        _synced = false;
        _lastAttempt = 0;
        DEBUG_PRINTLN("[时间] NTP配置已保存");
    }

    void loadConfig() {
        if (EEPROM.read(EEPROM_NTP_MAGIC_ADDR) == EEPROM_NTP_MAGIC) {
            memset(_config.server1, 0, sizeof(_config.server1));
            for (int i = 0; i < 31; i++) {
                _config.server1[i] = EEPROM.read(EEPROM_NTP_SERVER1_ADDR + i);
                if (_config.server1[i] == '\0') break;
            }
            memset(_config.server2, 0, sizeof(_config.server2));
            for (int i = 0; i < 23; i++) {
                _config.server2[i] = EEPROM.read(EEPROM_NTP_SERVER2_ADDR + i);
                if (_config.server2[i] == '\0') break;
            }
            memset(_config.server3, 0, sizeof(_config.server3));
            for (int i = 0; i < 23; i++) {
                _config.server3[i] = EEPROM.read(EEPROM_NTP_SERVER3_ADDR + i);
                if (_config.server3[i] == '\0') break;
            }
            _config.timezone = (int8_t)EEPROM.read(EEPROM_NTP_TZ_ADDR) - 12;
            _config.interval = EEPROM.read(EEPROM_NTP_INTERVAL_ADDR);
            _config.interval |= (uint32_t)EEPROM.read(EEPROM_NTP_INTERVAL_ADDR + 1) << 8;
            _config.interval |= (uint32_t)EEPROM.read(EEPROM_NTP_INTERVAL_ADDR + 2) << 16;
            _config.interval |= (uint32_t)EEPROM.read(EEPROM_NTP_INTERVAL_ADDR + 3) << 24;
            DEBUG_PRINTLN("[时间] NTP配置已加载");
        }
    }

private:
    NtpConfig _config;
    bool _synced;
    unsigned long _lastSync;
    unsigned long _lastAttempt;
    unsigned long _lastTickMs; // millis累加用的上次tick时间
    uint32_t _timestamp; // 缓存的Unix时间戳，用millis累加更新

    // ========= 纯算法时间格式化（避免localtime崩溃） =========

    // 判断闰年
    static bool isLeapYear(uint16_t year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    // 获取某年某月的天数
    static uint8_t daysInMonth(uint16_t year, uint8_t month) {
        static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        if (month == 2 && isLeapYear(year)) return 29;
        if (month >= 1 && month <= 12) return days[month - 1];
        return 31;
    }

    // Unix时间戳转年月日时分秒（基于1970-01-01 00:00:00 UTC）
    static void unixToYmdhms(uint32_t t, uint16_t& yy, uint8_t& mo, uint8_t& dd,
                             uint8_t& hh, uint8_t& mi, uint8_t& ss) {
        ss = t % 60;
        t /= 60;
        mi = t % 60;
        t /= 60;
        hh = t % 24;
        t /= 24;

        // 计算年份
        uint16_t year = 1970;
        while (true) {
            uint32_t days = isLeapYear(year) ? 366 : 365;
            if (t < days) break;
            t -= days;
            year++;
        }
        yy = year;

        // 计算月份
        uint8_t month = 1;
        while (true) {
            uint8_t d = daysInMonth(year, month);
            if (t < d) break;
            t -= d;
            month++;
        }
        mo = month;
        dd = t + 1;
    }

    static void formatDateTime(char* buf, size_t bufSize, uint32_t t) {
        if (!buf || bufSize < 20) { if (buf && bufSize > 0) buf[0] = '\0'; return; }
        uint16_t yy; uint8_t mo, dd, hh, mi, ss;
        unixToYmdhms(t, yy, mo, dd, hh, mi, ss);
        snprintf(buf, bufSize, "%04u-%02u-%02u %02u:%02u:%02u",
            yy, mo, dd, hh, mi, ss);
    }

    static void formatDate(char* buf, size_t bufSize, uint32_t t) {
        if (!buf || bufSize < 11) { if (buf && bufSize > 0) buf[0] = '\0'; return; }
        uint16_t yy; uint8_t mo, dd, hh, mi, ss;
        unixToYmdhms(t, yy, mo, dd, hh, mi, ss);
        snprintf(buf, bufSize, "%04u-%02u-%02u", yy, mo, dd);
    }

    static void formatTimeOnly(char* buf, size_t bufSize, uint32_t t) {
        if (!buf || bufSize < 9) { if (buf && bufSize > 0) buf[0] = '\0'; return; }
        uint16_t yy; uint8_t mo, dd, hh, mi, ss;
        unixToYmdhms(t, yy, mo, dd, hh, mi, ss);
        snprintf(buf, bufSize, "%02u:%02u:%02u", hh, mi, ss);
    }
};

#endif // TIME_MANAGER_H
