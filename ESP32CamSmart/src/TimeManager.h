/*
 * TimeManager - NTP 时间同步
 */

#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <time.h>
#include <EEPROM.h>
#include "config.h"

class TimeManager {
public:
    TimeManager() : _synced(false), _timestamp(0),
        _lastSync(0), _syncInterval(1800), _timezone(8) {}

    void begin() {
        loadConfig();
        configTime(_timezone * 3600, 0,
            _ntpServer1.c_str(),
            _ntpServer2.c_str(),
            "time.google.com");
        DEBUG_PRINTLN_TAG("TIME", "NTP 初始化完成");
        sync();
    }

    void loop() {
        if (!_synced) {
            // 未同步时每 30 秒尝试一次
            if (millis() - _lastSync > 30000) {
                sync();
            }
            return;
        }

        // 已同步，按间隔更新
        if (millis() - _lastSync > (uint32_t)_syncInterval * 1000UL) {
            sync();
        }
    }

    void sync() {
        _lastSync = millis();

        time_t now;
        time(&now);

        // 如果时间大于 2020 年，说明同步成功
        if (now > 1577836800) {
            _timestamp = now;
            _synced = true;
            struct tm t;
            localtime_r(&now, &t);
            DEBUG_PRINTF("[TIME] 同步成功: %04d-%02d-%02d %02d:%02d:%02d\r\n",
                t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                t.tm_hour, t.tm_min, t.tm_sec);
        } else {
            DEBUG_PRINTLN_TAG("TIME", "同步中...");
        }
    }

    bool isSynced() { return _synced; }
    time_t getTimestamp() {
        if (_synced) {
            time_t now;
            time(&now);
            return now;
        }
        return 0;
    }

    // 写入调用方缓冲，避免返回 String 造成堆碎片
    void getDateTimeChars(char* buf, size_t n) {
        if (!buf || n == 0) return;
        if (!_synced) {
            snprintf(buf, n, "未同步");
            return;
        }
        time_t now = getTimestamp();
        struct tm t;
        localtime_r(&now, &t);
        snprintf(buf, n, "%04d-%02d-%02d %02d:%02d:%02d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);
    }

    String getDateTimeStr() {
        if (!_synced) return "未同步";
        time_t now = getTimestamp();
        struct tm t;
        localtime_r(&now, &t);
        char buf[24];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }

    String getTimeStr() {
        if (!_synced) return "--:--:--";
        time_t now = getTimestamp();
        struct tm t;
        localtime_r(&now, &t);
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
            t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }

    String getDateStr() {
        if (!_synced) return "----/--/--";
        time_t now = getTimestamp();
        struct tm t;
        localtime_r(&now, &t);
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
        return String(buf);
    }

    // 生成文件名用的时间戳
    String getFileTimestamp() {
        if (!_synced) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%08lu", millis() / 1000);
            return String(buf);
        }
        time_t now = getTimestamp();
        struct tm t;
        localtime_r(&now, &t);
        char buf[32];
        snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }

    int getTimezone() { return _timezone; }
    String getNtpServer1() { return _ntpServer1; }
    String getNtpServer2() { return _ntpServer2; }
    int getSyncInterval() { return _syncInterval; }

    void setTimezone(int tz) {
        if (tz >= -12 && tz <= 12) {
            _timezone = tz;
            saveConfig();
            configTime(_timezone * 3600, 0,
                _ntpServer1.c_str(), _ntpServer2.c_str());
            sync();
        }
    }

    void setNtpServers(String s1, String s2) {
        _ntpServer1 = s1;
        _ntpServer2 = s2;
        saveConfig();
        configTime(_timezone * 3600, 0,
            _ntpServer1.c_str(), _ntpServer2.c_str());
        sync();
    }

    void setSyncInterval(int sec) {
        if (sec >= 60 && sec <= 86400) {
            _syncInterval = sec;
            saveConfig();
        }
    }

private:
    bool _synced;
    time_t _timestamp;
    unsigned long _lastSync;
    int _syncInterval;  // 秒
    int _timezone;      // 时区偏移
    String _ntpServer1;
    String _ntpServer2;

    void loadConfig() {
        char buf[65];

        // NTP 服务器1
        for (int i = 0; i < 64; i++) {
            char c = (char)EEPROM.read(EEP_ADDR_NTP_SVR1 + i);
            buf[i] = (c == 0xFF || c < 32 || c > 126) ? 0 : c;
        }
        buf[64] = '\0';
        _ntpServer1 = String(buf);
        if (_ntpServer1.length() == 0) _ntpServer1 = "ntp.aliyun.com";

        // NTP 服务器2
        for (int i = 0; i < 64; i++) {
            char c = (char)EEPROM.read(EEP_ADDR_NTP_SVR2 + i);
            buf[i] = (c == 0xFF || c < 32 || c > 126) ? 0 : c;
        }
        buf[64] = '\0';
        _ntpServer2 = String(buf);
        if (_ntpServer2.length() == 0) _ntpServer2 = "ntp.ntsc.ac.cn";

        // 时区
        int8_t tz = (int8_t)EEPROM.read(EEP_ADDR_NTP_TZ);
        if (tz < -12 || tz > 12) tz = 8;
        _timezone = tz;
    }

    void saveConfig() {
        // NTP 服务器1
        for (int i = 0; i < 64; i++) {
            EEPROM.write(EEP_ADDR_NTP_SVR1 + i,
                i < _ntpServer1.length() ? _ntpServer1[i] : 0);
        }
        // NTP 服务器2
        for (int i = 0; i < 64; i++) {
            EEPROM.write(EEP_ADDR_NTP_SVR2 + i,
                i < _ntpServer2.length() ? _ntpServer2[i] : 0);
        }
        // 时区
        EEPROM.write(EEP_ADDR_NTP_TZ, (uint8_t)_timezone);
        EEPROM.commit();
    }
};

#endif // TIME_MANAGER_H
