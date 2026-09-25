/*
 * WifiManager - WiFi管理模块
 * 负责WiFi连接、凭证存储和读取
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "config.h"

struct WifiCredentials {
    char ssid[SSID_MAX_LENGTH + 1];
    char password[PASSWORD_MAX_LENGTH + 1];
    bool valid;
};

class WifiManager {
public:
    WifiManager() {}

    void begin() {
        EEPROM.begin(EEPROM_SIZE);
        DEBUG_PRINTLN("[WiFi] WiFi管理模块初始化完成");
    }

    // 从EEPROM读取WiFi凭证
    WifiCredentials readCredentials() {
        WifiCredentials creds;
        creds.valid = false;

        // 检查魔术字
        byte magic = EEPROM.read(EEPROM_MAGIC_ADDR);
        if (magic != EEPROM_MAGIC) {
            DEBUG_PRINTLN("[WiFi] EEPROM中无有效WiFi配置（魔术字不匹配）");
            return creds;
        }

        // 读取SSID
        memset(creds.ssid, 0, sizeof(creds.ssid));
        for (int i = 0; i < SSID_MAX_LENGTH; i++) {
            char c = EEPROM.read(EEPROM_SSID_ADDR + i);
            if (c == '\0') break;
            creds.ssid[i] = c;
        }

        // 读取密码
        memset(creds.password, 0, sizeof(creds.password));
        for (int i = 0; i < PASSWORD_MAX_LENGTH; i++) {
            char c = EEPROM.read(EEPROM_PASSWORD_ADDR + i);
            if (c == '\0') break;
            creds.password[i] = c;
        }

        creds.valid = (strlen(creds.ssid) > 0);
        if (creds.valid) {
            DEBUG_PRINTF("[WiFi] 读取到WiFi配置: SSID=%s\r\n", creds.ssid);
        }

        return creds;
    }

    // 保存WiFi凭证到EEPROM
    bool saveCredentials(const char* ssid, const char* password) {
        if (!ssid || strlen(ssid) == 0) {
            DEBUG_PRINTLN("[WiFi] 保存失败: SSID为空");
            return false;
        }

        DEBUG_PRINTF("[WiFi] 保存WiFi配置: SSID=%s\r\n", ssid);

        // 写入魔术字
        EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC);

        // 写入SSID
        int ssidLen = strlen(ssid);
        for (int i = 0; i < ssidLen && i < SSID_MAX_LENGTH; i++) {
            EEPROM.write(EEPROM_SSID_ADDR + i, ssid[i]);
        }
        EEPROM.write(EEPROM_SSID_ADDR + ssidLen, '\0');

        // 写入密码
        if (password) {
            int pwdLen = strlen(password);
            for (int i = 0; i < pwdLen && i < PASSWORD_MAX_LENGTH; i++) {
                EEPROM.write(EEPROM_PASSWORD_ADDR + i, password[i]);
            }
            EEPROM.write(EEPROM_PASSWORD_ADDR + pwdLen, '\0');
        } else {
            EEPROM.write(EEPROM_PASSWORD_ADDR, '\0');
        }

        bool success = EEPROM.commit();
        if (success) {
            DEBUG_PRINTLN("[WiFi] WiFi配置已保存到EEPROM");
        } else {
            DEBUG_PRINTLN("[WiFi] WiFi配置保存失败！");
        }

        return success;
    }

    // 清除WiFi凭证
    void clearCredentials() {
        EEPROM.write(EEPROM_MAGIC_ADDR, 0x00);
        EEPROM.commit();
        DEBUG_PRINTLN("[WiFi] WiFi配置已清除");
    }

    // 尝试连接WiFi，返回是否成功
    bool connect(const char* ssid, const char* password, int maxRetries = WIFI_RETRY_COUNT) {
        if (!ssid || strlen(ssid) == 0) {
            DEBUG_PRINTLN("[WiFi] 连接失败: SSID为空");
            return false;
        }

        DEBUG_PRINTF("[WiFi] 正在连接WiFi: %s (最多重试%d次)\r\n", ssid, maxRetries);

        for (int attempt = 1; attempt <= maxRetries; attempt++) {
            DEBUG_PRINTF("[WiFi] 第%d次尝试连接...\r\n", attempt);

            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid, password ? password : "");

            unsigned long startTime = millis();
            bool connected = false;

            while (millis() - startTime < WIFI_CONNECT_TIMEOUT) {
                if (WiFi.status() == WL_CONNECTED) {
                    connected = true;
                    break;
                }
                delay(200);
            }

            if (connected) {
                DEBUG_PRINTLN("");
                DEBUG_PRINTF("[WiFi] 连接成功！\r\n");
                DEBUG_PRINTF("[WiFi] SSID: %s\r\n", WiFi.SSID().c_str());
                DEBUG_PRINTF("[WiFi] IP地址: %s\r\n", WiFi.localIP().toString().c_str());
                DEBUG_PRINTF("[WiFi] 信号强度: %d dBm\r\n", WiFi.RSSI());
                return true;
            }

            DEBUG_PRINTF("[WiFi] 第%d次连接失败 (状态码: %d)\r\n", attempt, WiFi.status());
            WiFi.disconnect();
            delay(WIFI_RECONNECT_DELAY);
        }

        DEBUG_PRINTF("[WiFi] 连接失败，已重试%d次\r\n", maxRetries);
        return false;
    }

    // 断开WiFi连接
    void disconnect() {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        DEBUG_PRINTLN("[WiFi] 已断开WiFi连接");
    }

    // 检查是否已连接
    bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }

    // 获取当前IP地址
    String getIP() {
        return WiFi.localIP().toString();
    }

    // 启动异步扫描（立即返回，不阻塞）
    bool startScan() {
        DEBUG_PRINTLN("[WiFi] 启动异步扫描...");
        
        // 确保STA模式启用
        WiFiMode_t currentMode = WiFi.getMode();
        if (currentMode != WIFI_STA && currentMode != WIFI_AP_STA) {
            DEBUG_PRINTLN("[WiFi] 切换到AP+STA模式以进行扫描...");
            WiFi.mode(WIFI_AP_STA);
            delay(50);
        }
        
        // 先清理之前的扫描结果
        WiFi.scanDelete();
        delay(10);
        
        // 启动异步扫描 (true = 异步, false = 显示隐藏网络)
        bool started = WiFi.scanNetworks(true, false);
        
        if (started) {
            DEBUG_PRINTLN("[WiFi] 异步扫描已启动，等待结果...");
        } else {
            DEBUG_PRINTLN("[WiFi] 扫描启动失败！");
        }
        
        return started;
    }
    
    // 检查扫描是否完成
    // 返回: -1=扫描中，-2=未启动，>=0=扫描完成的网络数量
    int getScanStatus() {
        int status = WiFi.scanComplete();
        return status;
    }
    
    // 同步扫描（阻塞方式，备用）
    int scanNetworks() {
        DEBUG_PRINTLN("[WiFi] 正在同步扫描附近的WiFi网络...");
        
        WiFiMode_t currentMode = WiFi.getMode();
        if (currentMode != WIFI_STA && currentMode != WIFI_AP_STA) {
            WiFi.mode(WIFI_AP_STA);
            delay(100);
        }
        
        WiFi.scanDelete();
        delay(50);
        
        int n = WiFi.scanNetworks();
        
        DEBUG_PRINTF("[WiFi] 扫描完成，共发现%d个网络\r\n", n);
        
        for (int i = 0; i < n; i++) {
            DEBUG_PRINTF("[WiFi]   %d. %s (%d dBm) %s\r\n", 
                i + 1, 
                WiFi.SSID(i).c_str(), 
                WiFi.RSSI(i),
                (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " [开放]" : " [加密]");
        }
        
        return n;
    }

    // 获取扫描到的WiFi列表（HTML格式）
    // 注意：需要先调用 startScan() 启动扫描
    String getWifiListHTML() {
        String html = "";
        int n = WiFi.scanComplete();

        if (n < 0) {
            // 扫描还在进行中
            html = "<p style='color:#999;padding:20px;text-align:center;'>扫描中，请稍候...</p>";
            return html;
        }

        if (n == 0) {
            html = "<p style='color:#999;padding:20px;text-align:center;'>未扫描到WiFi网络</p>";
            return html;
        }

        DEBUG_PRINTF("[WiFi] 获取WiFi列表，共%d个网络\r\n", n);
        
        // 打印扫描结果到串口
        for (int i = 0; i < n; i++) {
            DEBUG_PRINTF("[WiFi]   %d. %s (%d dBm)\r\n", 
                i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }

        // 按信号强度排序
        int indices[n];
        for (int i = 0; i < n; i++) indices[i] = i;

        // 简单的冒泡排序（按RSSI降序）
        for (int i = 0; i < n - 1; i++) {
            for (int j = 0; j < n - i - 1; j++) {
                if (WiFi.RSSI(indices[j]) < WiFi.RSSI(indices[j + 1])) {
                    int tmp = indices[j];
                    indices[j] = indices[j + 1];
                    indices[j + 1] = tmp;
                }
            }
        }

        for (int i = 0; i < n; i++) {
            int idx = indices[i];
            String ssid = WiFi.SSID(idx);
            int rssi = WiFi.RSSI(idx);
            bool encrypted = (WiFi.encryptionType(idx) != ENC_TYPE_NONE);

            // 信号强度条
            int bars = 1;
            if (rssi > -50) bars = 4;
            else if (rssi > -60) bars = 3;
            else if (rssi > -70) bars = 2;

            String lockIcon = encrypted ? "&#128274;" : "";

            html += "<div class='wifi-item' onclick='selectWifi(\"" + ssid + "\", " + (encrypted ? "true" : "false") + ")'>";
            html += "<div class='wifi-name'>" + ssid + " " + lockIcon + "</div>";
            html += "<div class='wifi-signal'>";
            for (int b = 0; b < 4; b++) {
                html += "<span class='signal-bar " + String(b < bars ? "active" : "") + "'></span>";
            }
            html += "<span class='rssi-text'>" + String(rssi) + "dBm</span>";
            html += "</div>";
            html += "</div>";
        }

        return html;
    }
};

#endif // WIFI_MANAGER_H
