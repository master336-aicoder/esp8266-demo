/*
 * WiFiManager - WiFi 连接管理 + 配网门户
 * 参考 ESP8266 版本的配网逻辑
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include "config.h"

class WifiManager {
public:
    WifiManager() : _server(80), _state(STATE_STARTUP),
        _retryCount(0), _lastRetryTime(0),
        _apMode(false), _downSince(0), _lastReconnect(0),
        _reconnectFails(0) {}

    void begin() {
        EEPROM.begin(EEPROM_SIZE);
        loadConfig();
        DEBUG_PRINTLN_TAG("WIFI", "初始化完成");
    }

    // 尝试连接 WiFi，返回 true 表示连接成功
    bool connect() {
        if (_ssid.isEmpty()) {
            DEBUG_PRINTLN_TAG("WIFI", "无WiFi配置，进入配网模式");
            return false;
        }

        DEBUG_PRINTF("[WIFI] 正在连接: %s ...\r\n", _ssid.c_str());
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        WiFi.setTxPower(WIFI_POWER_19_5dBm);
        WiFi.begin(_ssid.c_str(), _password.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(500);
            DEBUG_PRINT(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            DEBUG_PRINTLN();
            DEBUG_PRINTF("[WIFI] 连接成功！IP: %s\r\n", WiFi.localIP().toString().c_str());
            DEBUG_PRINTF("[WIFI] 信号强度: %d dBm\r\n", WiFi.RSSI());
            return true;
        } else {
            DEBUG_PRINTLN();
            DEBUG_PRINTLN_TAG("WIFI", "连接失败");
            return false;
        }
    }

    // 启动配网门户（AP 模式）
    void startPortal() {
        _apMode = true;
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : NULL, 6, 0, 4);
        WiFi.setSleep(false);

        DEBUG_PRINTF("[PORTAL] 配网热点已启动: %s\r\n", AP_SSID);
        DEBUG_PRINTF("[PORTAL] 访问地址: http://192.168.4.1/\r\n");

        _server.on("/", std::bind(&WifiManager::handlePortal, this));
        _server.on("/scan", std::bind(&WifiManager::handleScan, this));
        _server.on("/save", std::bind(&WifiManager::handleSave, this));
        _server.on("/status", std::bind(&WifiManager::handleStatus, this));
        _server.onNotFound(std::bind(&WifiManager::handleNotFound, this));
        _server.begin();
    }

    // 仅开启热点（不占用 80 端口），让设备在没有路由器时也能直接使用全部功能
    void startApOnly() {
        WiFi.mode(WIFI_AP);
        WiFi.softAP(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : NULL, 6, 0, 4);
        WiFi.setSleep(false);
        _apMode = false;
        DEBUG_PRINTF("[WIFI] 已开启热点: %s，访问 http://%s/\r\n",
            AP_SSID, WiFi.softAPIP().toString().c_str());
    }

    // 运行期 WiFi 维护：STA 掉线后自动重连（完全非阻塞，不卡 Web 与录像）
    void maintainWifi() {
        if (WiFi.getMode() != WIFI_STA) return;   // 热点模式下不处理
        if (_ssid.isEmpty()) return;              // 无凭据不处理

        if (WiFi.status() == WL_CONNECTED) {
            if (_downSince != 0) {
                DEBUG_PRINTF("[WIFI] 已恢复连接，IP: %s\r\n",
                    WiFi.localIP().toString().c_str());
            }
            _downSince = 0;
            _reconnectFails = 0;
            return;
        }

        unsigned long now = millis();
        if (_downSince == 0) {
            _downSince = now;
            DEBUG_PRINTLN_TAG("WIFI", "检测到掉线，等待自动重连");
            return;
        }

        // 掉线超过 15 秒、且距上次重连超过 30 秒才重试
        if (now - _downSince < 15000) return;
        if (_lastReconnect != 0 && now - _lastReconnect < 30000) return;

        _lastReconnect = now;
        _reconnectFails++;
        DEBUG_PRINTF("[WIFI] 第 %d 次自动重连: %s\r\n", _reconnectFails, _ssid.c_str());
        WiFi.disconnect();
        WiFi.begin(_ssid.c_str(), _password.c_str());
    }

    void loop() {
        if (_apMode) {
            _server.handleClient();
        }
    }

    bool isApMode() { return _apMode; }
    String getSsid() { return _ssid; }
    String getPassword() { return _password; }

    // 保存凭据（运行模式下修改 WiFi 用）
    bool saveCredentials(const String& ssid, const String& password) {
        if (ssid.isEmpty()) return false;
        saveConfig(ssid, password);
        return true;
    }

    // 清除已保存的 WiFi 凭据：下次启动会自动进入配网门户
    void clearCredentials() {
        _ssid = "";
        _password = "";
        for (int i = 0; i < 32; i++) EEPROM.write(EEP_ADDR_SSID + i, 0);
        for (int i = 0; i < 64; i++) EEPROM.write(EEP_ADDR_PASS + i, 0);
        EEPROM.write(EEP_ADDR_MAGIC, 0);
        EEPROM.commit();
        DEBUG_PRINTLN_TAG("WIFI", "WiFi 凭据已清除");
    }

    bool hasConfig() {
        return EEPROM.read(EEP_ADDR_MAGIC) == EEPROM_MAGIC && !_ssid.isEmpty();
    }

private:
    WebServer _server;
    PortalState _state;
    int _retryCount;
    unsigned long _lastRetryTime;
    bool _apMode;
    unsigned long _downSince;      // 掉线起始时刻（0 表示在线）
    unsigned long _lastReconnect;  // 上次自动重连时刻
    int _reconnectFails;

    String _ssid;
    String _password;

    void loadConfig() {
        if (EEPROM.read(EEP_ADDR_MAGIC) != EEPROM_MAGIC) {
            DEBUG_PRINTLN_TAG("WIFI", "无保存的WiFi配置");
            return;
        }

        char ssid_buf[33];
        for (int i = 0; i < 32; i++) {
            ssid_buf[i] = EEPROM.read(EEP_ADDR_SSID + i);
        }
        ssid_buf[32] = '\0';
        _ssid = String(ssid_buf);

        char pass_buf[65];
        for (int i = 0; i < 64; i++) {
            pass_buf[i] = EEPROM.read(EEP_ADDR_PASS + i);
        }
        pass_buf[64] = '\0';
        _password = String(pass_buf);

        DEBUG_PRINTF("[WIFI] 已加载配置: SSID=%s\r\n", _ssid.c_str());
    }

    void saveConfig(String ssid, String password) {
        _ssid = ssid;
        _password = password;

        EEPROM.write(EEP_ADDR_MAGIC, EEPROM_MAGIC);

        // 保存 SSID
        for (int i = 0; i < 32; i++) {
            EEPROM.write(EEP_ADDR_SSID + i,
                i < ssid.length() ? ssid[i] : 0);
        }

        // 保存密码
        for (int i = 0; i < 64; i++) {
            EEPROM.write(EEP_ADDR_PASS + i,
                i < password.length() ? password[i] : 0);
        }

        EEPROM.commit();
        DEBUG_PRINTLN_TAG("WIFI", "配置已保存");
    }

    // ========== Web 处理函数 ==========

    void handlePortal() {
        String html = "<!DOCTYPE html><html><head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        html += "<title>ESP32-CAM 配网</title>";
        html += "<style>";
        html += "*{box-sizing:border-box;margin:0;padding:0}";
        html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Microsoft YaHei',sans-serif;background:#f5f6fa;padding:20px}";
        html += ".container{max-width:500px;margin:0 auto}";
        html += ".header{background:linear-gradient(135deg,#6366f1,#8b5cf6);color:white;padding:24px;border-radius:12px;margin-bottom:20px;text-align:center}";
        html += ".header h1{font-size:22px;margin-bottom:8px}";
        html += ".header p{opacity:0.9;font-size:14px}";
        html += ".card{background:white;border-radius:12px;padding:20px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,0.05)}";
        html += ".card h2{font-size:16px;margin-bottom:16px;color:#1e2230;border-bottom:2px solid #6366f1;padding-bottom:8px;display:inline-block}";
        html += ".form-group{margin-bottom:16px}";
        html += ".form-group label{display:block;margin-bottom:6px;font-size:13px;color:#6b7280;font-weight:500}";
        html += ".form-group input,.form-group select{width:100%;padding:10px 12px;border:1px solid #d0d5e0;border-radius:6px;font-size:14px}";
        html += ".btn{width:100%;padding:12px;border:none;border-radius:6px;font-size:15px;font-weight:500;cursor:pointer;background:#6366f1;color:white}";
        html += ".btn:hover{background:#4f46e5}";
        html += ".btn-scan{background:#10b981}";
        html += ".wifi-list{max-height:300px;overflow-y:auto}";
        html += ".wifi-item{padding:10px;border-bottom:1px solid #e4e7ef;cursor:pointer;display:flex;justify-content:space-between;align-items:center}";
        html += ".wifi-item:hover{background:#f5f6fa}";
        html += ".wifi-name{font-size:14px;font-weight:500}";
        html += ".wifi-signal{font-size:12px;color:#6b7280}";
        html += ".status{text-align:center;padding:20px;font-size:14px}";
        html += ".loading{color:#6366f1}";
        html += ".success{color:#10b981}";
        html += ".error{color:#ef4444}";
        html += "</style></head><body>";

        html += "<div class='container'>";
        html += "<div class='header'>";
        html += "<h1>📷 ESP32-CAM 配网</h1>";
        html += "<p>请选择 WiFi 网络并输入密码</p>";
        html += "</div>";

        html += "<div class='card'>";
        html += "<h2>WiFi 配置</h2>";
        html += "<div class='form-group'>";
        html += "<label>WiFi 名称</label>";
        html += "<input type='text' id='ssid' placeholder='点击下方选择或手动输入'>";
        html += "</div>";
        html += "<div class='form-group'>";
        html += "<label>WiFi 密码</label>";
        html += "<input type='password' id='password' placeholder='输入WiFi密码'>";
        html += "</div>";
        html += "<button class='btn' onclick='saveWifi()'>保存并连接</button>";
        html += "</div>";

        html += "<div class='card'>";
        html += "<h2>可用网络</h2>";
        html += "<button class='btn btn-scan' id='scanBtn' onclick='scanWifi()'>🔍 扫描 WiFi</button>";
        html += "<div id='wifiList' class='wifi-list' style='margin-top:12px'><div class='status'>点击上方按钮扫描</div></div>";
        html += "</div>";

        html += "<div id='statusDiv' style='display:none' class='card'>";
        html += "<div class='status' id='statusText'></div>";
        html += "</div>";

        html += "</div>";

        html += "<script>";
        html += "var scanning=false;";
        html += "function scanWifi(){";
        html += "if(scanning)return;scanning=true;";
        html += "document.getElementById('scanBtn').disabled=true;";
        html += "document.getElementById('wifiList').innerHTML='<div class=\"status loading\">正在扫描...</div>';";
        html += "fetch('/scan').then(r=>r.json()).then(data=>{";
        html += "scanning=false;document.getElementById('scanBtn').disabled=false;";
        html += "var html='';";
        html += "if(data.networks.length===0){html='<div class=\"status\">未找到WiFi网络</div>';}";
        html += "else{data.networks.forEach(function(n){";
        html += "html+='<div class=\"wifi-item\" onclick=\"selectWifi(\\''+n.ssid+'\\')\">';";
        html += "html+='<span class=\"wifi-name\">'+n.ssid+'</span>';";
        html += "html+='<span class=\"wifi-signal\">'+n.rssi+' dBm</span>';";
        html += "html+='</div>';});";
        html += "}";
        html += "document.getElementById('wifiList').innerHTML=html;";
        html += "}).catch(e=>{scanning=false;document.getElementById('scanBtn').disabled=false;})}";

        html += "function selectWifi(ssid){document.getElementById('ssid').value=ssid;document.getElementById('password').focus()}";

        html += "function saveWifi(){";
        html += "var ssid=document.getElementById('ssid').value;";
        html += "var pass=document.getElementById('password').value;";
        html += "if(!ssid){alert('请输入WiFi名称');return;}";
        html += "document.getElementById('statusDiv').style.display='block';";
        html += "document.getElementById('statusText').className='status loading';";
        html += "document.getElementById('statusText').textContent='正在保存并连接...';";
        html += "fetch('/save?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass)).then(r=>r.json()).then(d=>{";
        html += "if(d.success){document.getElementById('statusText').className='status success';document.getElementById('statusText').textContent='连接成功！设备IP: '+d.ip;}";
        html += "else{document.getElementById('statusText').className='status error';document.getElementById('statusText').textContent='连接失败：'+d.error;}";
        html += "})}";
        html += "</script>";
        html += "</body></html>";

        _server.send(200, "text/html", html);
    }

    void handleScan() {
        DEBUG_PRINTLN_TAG("PORTAL", "开始扫描WiFi...");
        int n = WiFi.scanNetworks();

        String json = "{\"networks\":[";
        for (int i = 0; i < n; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
            json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
            json += "\"enc\":" + String(WiFi.encryptionType(i)) + "}";
        }
        json += "]}";

        _server.send(200, "application/json", json);
        WiFi.scanDelete();
    }

    void handleSave() {
        String ssid = _server.arg("ssid");
        String pass = _server.arg("pass");

        if (ssid.isEmpty()) {
            _server.send(400, "application/json",
                "{\"success\":false,\"error\":\"SSID不能为空\"}");
            return;
        }

        DEBUG_PRINTF("[PORTAL] 保存WiFi配置: %s\r\n", ssid.c_str());
        saveConfig(ssid, pass);

        // 尝试连接
        WiFi.disconnect(false, false);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
            delay(300);
        }

        if (WiFi.status() == WL_CONNECTED) {
            String ip = WiFi.localIP().toString();
            _server.send(200, "application/json",
                "{\"success\":true,\"ip\":\"" + ip + "\"}");
            DEBUG_PRINTF("[PORTAL] 连接成功: %s，即将重启进入运行模式\r\n", ip.c_str());
            delay(500);
            ESP.restart();
        } else {
            _server.send(200, "application/json",
                "{\"success\":false,\"error\":\"连接失败，请检查密码\"}");
            // 重新进入 AP 模式
            WiFi.mode(WIFI_AP_STA);
            WiFi.softAP(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : NULL, 6, 0, 4);
        }
    }

    void handleStatus() {
        String json = "{";
        json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
        json += "\"ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI());
        json += "}";
        _server.send(200, "application/json", json);
    }

    void handleNotFound() {
        _server.send(404, "text/plain", "Not Found");
    }
};

#endif // WIFI_MANAGER_H
