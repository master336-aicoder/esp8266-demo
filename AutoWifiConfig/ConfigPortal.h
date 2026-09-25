/*
 * ConfigPortal - 配网门户模块
 * 提供Web配网页面，让用户选择WiFi并输入密码
 */

#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "WifiManager.h"
#include "LedStatus.h"

// 配网结果回调函数类型
typedef std::function<void(const char* ssid, const char* password)> ConfigCallback;

class ConfigPortal {
public:
    ConfigPortal(WifiManager& wifiMgr, LedStatus& led)
        : _wifiMgr(wifiMgr), _led(led), _server(WEB_SERVER_PORT), _configCallback(nullptr) {}

    void begin(ConfigCallback callback) {
        _configCallback = callback;

        // 启动AP模式
        startAP();

        // 配置Web服务器路由
        _server.on("/", std::bind(&ConfigPortal::handleRoot, this));
        _server.on("/scan", std::bind(&ConfigPortal::handleScan, this));
        _server.on("/connect", HTTP_POST, std::bind(&ConfigPortal::handleConnect, this));
        _server.on("/status", std::bind(&ConfigPortal::handleStatus, this));
        _server.on("/reset", std::bind(&ConfigPortal::handleReset, this));
        _server.onNotFound(std::bind(&ConfigPortal::handleNotFound, this));

        _server.begin();
        DEBUG_PRINTLN("[Portal] 配网Web服务器已启动");
        DEBUG_PRINTF("[Portal] 请连接WiFi: %s\r\n", AP_SSID);
        DEBUG_PRINTF("[Portal] 然后访问: http://192.168.4.1\r\n");
    }

    void stop() {
        _server.stop();
        WiFi.softAPdisconnect(true);
        DEBUG_PRINTLN("[Portal] 配网服务器已停止");
    }

    // 在loop中调用处理客户端请求
    void handleClient() {
        _server.handleClient();
    }

private:
    ESP8266WebServer _server;
    WifiManager& _wifiMgr;
    LedStatus& _led;
    ConfigCallback _configCallback;

    // 启动AP模式
    void startAP() {
        DEBUG_PRINTLN("[Portal] 启动AP模式...");

        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

        bool success;
        if (strlen(AP_PASSWORD) > 0) {
            success = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONNECTIONS);
        } else {
            success = WiFi.softAP(AP_SSID, "", AP_CHANNEL, 0, AP_MAX_CONNECTIONS);
        }

        if (success) {
            DEBUG_PRINTF("[Portal] AP启动成功: %s\r\n", AP_SSID);
            DEBUG_PRINTF("[Portal] AP IP地址: %s\r\n", WiFi.softAPIP().toString().c_str());
        } else {
            DEBUG_PRINTLN("[Portal] AP启动失败！");
        }
    }

    // 主页 - 配网页面
    void handleRoot() {
        DEBUG_PRINTLN("[Portal] 访问配网主页");
        String html = getConfigPageHTML();
        _server.send(200, "text/html", html);
    }

    // 扫描WiFi
    void handleScan() {
        DEBUG_PRINTLN("[Portal] 收到扫描WiFi请求");
        _wifiMgr.scanNetworks();
        String wifiList = _wifiMgr.getWifiListHTML();
        _server.send(200, "text/html", wifiList);
    }

    // 连接WiFi
    void handleConnect() {
        DEBUG_PRINTLN("[Portal] 收到连接WiFi请求");

        if (!_server.hasArg("ssid")) {
            _server.send(400, "application/json", "{\"success\":false,\"message\":\"SSID不能为空\"}");
            return;
        }

        String ssid = _server.arg("ssid");
        String password = _server.arg("password");

        DEBUG_PRINTF("[Portal] 尝试连接: %s\r\n", ssid.c_str());

        // 先返回响应，然后在后台尝试连接
        _server.send(200, "application/json", "{\"success\":true,\"message\":\"正在尝试连接，请稍候...\"}");

        delay(100); // 确保响应发送完成

        // 调用回调函数处理连接
        if (_configCallback) {
            _configCallback(ssid.c_str(), password.c_str());
        }
    }

    // 查询连接状态
    void handleStatus() {
        bool connected = _wifiMgr.isConnected();
        String ip = _wifiMgr.getIP();
        String status = _led.getStatusText();

        String json = "{";
        json += "\"connected\":" + String(connected ? "true" : "false") + ",";
        json += "\"ip\":\"" + ip + "\",";
        json += "\"status\":\"" + status + "\"";
        json += "}";

        _server.send(200, "application/json", json);
    }

    // 重置WiFi配置
    void handleReset() {
        _wifiMgr.clearCredentials();
        _server.send(200, "application/json", "{\"success\":true,\"message\":\"配置已清除\"}");
    }

    // 404页面
    void handleNotFound() {
        String message = "File Not Found\n\n";
        message += "URI: ";
        message += _server.uri();
        message += "\nMethod: ";
        message += (_server.method() == HTTP_GET) ? "GET" : "POST";
        message += "\nArguments: ";
        message += _server.args();
        message += "\n";

        for (uint8_t i = 0; i < _server.args(); i++) {
            message += " " + _server.argName(i) + ": " + _server.arg(i) + "\n";
        }

        _server.send(404, "text/plain", message);
    }

    // 配网页面HTML
    String getConfigPageHTML() {
        String html = "<!DOCTYPE html>";
        html += "<html lang='zh-CN'>";
        html += "<head>";
        html += "<meta charset='UTF-8'>";
        html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        html += "<title>ESP8266 WiFi配网</title>";
        html += "<style>";
        html += "* { margin: 0; padding: 0; box-sizing: border-box; }";
        html += "body {";
        html += "    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;";
        html += "    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
        html += "    min-height: 100vh;";
        html += "    display: flex;";
        html += "    justify-content: center;";
        html += "    align-items: center;";
        html += "    padding: 20px;";
        html += "}";
        html += ".container {";
        html += "    background: white;";
        html += "    border-radius: 20px;";
        html += "    box-shadow: 0 20px 60px rgba(0,0,0,0.3);";
        html += "    width: 100%;";
        html += "    max-width: 450px;";
        html += "    overflow: hidden;";
        html += "}";
        html += ".header {";
        html += "    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
        html += "    color: white;";
        html += "    padding: 30px;";
        html += "    text-align: center;";
        html += "}";
        html += ".header h1 { font-size: 24px; margin-bottom: 8px; }";
        html += ".header p { opacity: 0.9; font-size: 14px; }";
        html += ".content { padding: 25px; }";
        html += ".form-group { margin-bottom: 18px; }";
        html += "label {";
        html += "    display: block;";
        html += "    margin-bottom: 8px;";
        html += "    color: #333;";
        html += "    font-weight: 500;";
        html += "    font-size: 14px;";
        html += "}";
        html += "input[type='text'], input[type='password'] {";
        html += "    width: 100%;";
        html += "    padding: 12px 15px;";
        html += "    border: 2px solid #e0e0e0;";
        html += "    border-radius: 10px;";
        html += "    font-size: 15px;";
        html += "    transition: border-color 0.3s;";
        html += "}";
        html += "input[type='text']:focus, input[type='password']:focus {";
        html += "    outline: none;";
        html += "    border-color: #667eea;";
        html += "}";
        html += ".btn {";
        html += "    width: 100%;";
        html += "    padding: 14px;";
        html += "    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);";
        html += "    color: white;";
        html += "    border: none;";
        html += "    border-radius: 10px;";
        html += "    font-size: 16px;";
        html += "    font-weight: 600;";
        html += "    cursor: pointer;";
        html += "    transition: transform 0.2s, box-shadow 0.2s;";
        html += "}";
        html += ".btn:hover { transform: translateY(-2px); box-shadow: 0 5px 20px rgba(102,126,234,0.4); }";
        html += ".btn:active { transform: translateY(0); }";
        html += ".btn:disabled { opacity: 0.6; cursor: not-allowed; transform: none; }";
        html += ".btn-scan {";
        html += "    background: #f0f0f0;";
        html += "    color: #667eea;";
        html += "    margin-bottom: 15px;";
        html += "}";
        html += ".wifi-list {";
        html += "    max-height: 250px;";
        html += "    overflow-y: auto;";
        html += "    border: 1px solid #e0e0e0;";
        html += "    border-radius: 10px;";
        html += "    margin-bottom: 18px;";
        html += "}";
        html += ".wifi-item {";
        html += "    padding: 12px 15px;";
        html += "    border-bottom: 1px solid #f0f0f0;";
        html += "    cursor: pointer;";
        html += "    transition: background 0.2s;";
        html += "    display: flex;";
        html += "    justify-content: space-between;";
        html += "    align-items: center;";
        html += "}";
        html += ".wifi-item:hover { background: #f8f9ff; }";
        html += ".wifi-item:last-child { border-bottom: none; }";
        html += ".wifi-name { font-size: 14px; color: #333; font-weight: 500; }";
        html += ".wifi-signal { display: flex; align-items: center; gap: 3px; }";
        html += ".signal-bar {";
        html += "    width: 4px;";
        html += "    background: #ddd;";
        html += "    border-radius: 2px;";
        html += "}";
        html += ".signal-bar.active { background: #667eea; }";
        html += ".signal-bar:nth-child(1) { height: 6px; }";
        html += ".signal-bar:nth-child(2) { height: 10px; }";
        html += ".signal-bar:nth-child(3) { height: 14px; }";
        html += ".signal-bar:nth-child(4) { height: 18px; }";
        html += ".rssi-text { font-size: 11px; color: #999; margin-left: 5px; }";
        html += ".message {";
        html += "    padding: 12px 15px;";
        html += "    border-radius: 10px;";
        html += "    margin-bottom: 15px;";
        html += "    font-size: 14px;";
        html += "    display: none;";
        html += "}";
        html += ".message.show { display: block; }";
        html += ".message.info { background: #e3f2fd; color: #1565c0; }";
        html += ".message.success { background: #e8f5e9; color: #2e7d32; }";
        html += ".message.error { background: #ffebee; color: #c62828; }";
        html += ".loading {";
        html += "    display: none;";
        html += "    text-align: center;";
        html += "    padding: 20px;";
        html += "}";
        html += ".loading.show { display: block; }";
        html += ".spinner {";
        html += "    width: 40px;";
        html += "    height: 40px;";
        html += "    border: 4px solid #f3f3f3;";
        html += "    border-top: 4px solid #667eea;";
        html += "    border-radius: 50%;";
        html += "    animation: spin 1s linear infinite;";
        html += "    margin: 0 auto 10px;";
        html += "}";
        html += "@keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }";
        html += ".success-icon {";
        html += "    font-size: 60px;";
        html += "    text-align: center;";
        html += "    margin-bottom: 10px;";
        html += "}";
        html += ".ip-address {";
        html += "    background: #f8f9ff;";
        html += "    padding: 10px;";
        html += "    border-radius: 8px;";
        html += "    text-align: center;";
        html += "    font-family: monospace;";
        html += "    font-size: 16px;";
        html += "    color: #667eea;";
        html += "    margin-top: 10px;";
        html += "}";
        html += "</style>";
        html += "</head>";
        html += "<body>";
        html += "<div class='container'>";
        html += "    <div class='header'>";
        html += "        <h1>&#128246; WiFi 配网</h1>";
        html += "        <p>ESP8266 智能配置</p>";
        html += "    </div>";
        html += "    <div class='content'>";
        html += "        <div id='message' class='message info'>请选择要连接的WiFi网络</div>";
        html += "";
        html += "        <button class='btn btn-scan' onclick='scanWifi()' id='scanBtn'>";
        html += "            &#128269; 扫描WiFi网络";
        html += "        </button>";
        html += "";
        html += "        <div class='wifi-list' id='wifiList'>";
        html += "            <div style='padding:20px;text-align:center;color:#999;'>点击上方按钮扫描WiFi</div>";
        html += "        </div>";
        html += "";
        html += "        <div class='form-group'>";
        html += "            <label for='ssid'>WiFi名称 (SSID)</label>";
        html += "            <input type='text' id='ssid' placeholder='输入或选择WiFi名称'>";
        html += "        </div>";
        html += "";
        html += "        <div class='form-group'>";
        html += "            <label for='password'>WiFi密码</label>";
        html += "            <input type='password' id='password' placeholder='输入WiFi密码'>";
        html += "        </div>";
        html += "";
        html += "        <button class='btn' onclick='connectWifi()' id='connectBtn'>";
        html += "            &#128279; 连接WiFi";
        html += "        </button>";
        html += "";
        html += "        <div id='loading' class='loading'>";
        html += "            <div class='spinner'></div>";
        html += "            <p>正在连接WiFi，请稍候...</p>";
        html += "        </div>";
        html += "";
        html += "        <div id='success' style='display:none;text-align:center;'>";
        html += "            <div class='success-icon'>&#9989;</div>";
        html += "            <h3 style='color:#2e7d32;margin-bottom:10px;'>连接成功！</h3>";
        html += "            <p style='color:#666;margin-bottom:10px;'>设备已成功连接到WiFi网络</p>";
        html += "            <div class='ip-address' id='ipAddr'>-</div>";
        html += "        </div>";
        html += "    </div>";
        html += "</div>";
        html += "";
        html += "<script>";
        html += "function showMessage(text, type) {";
        html += "    var msg = document.getElementById('message');";
        html += "    msg.className = 'message ' + type + ' show';";
        html += "    msg.textContent = text;";
        html += "}";
        html += "";
        html += "function selectWifi(ssid, encrypted) {";
        html += "    document.getElementById('ssid').value = ssid;";
        html += "    if (encrypted) {";
        html += "        document.getElementById('password').focus();";
        html += "    }";
        html += "    showMessage('已选择: ' + ssid, 'info');";
        html += "}";
        html += "";
        html += "function scanWifi() {";
        html += "    var btn = document.getElementById('scanBtn');";
        html += "    var list = document.getElementById('wifiList');";
        html += "    btn.disabled = true;";
        html += "    btn.textContent = '扫描中...';";
        html += "    list.innerHTML = '<div style=\"padding:20px;text-align:center;color:#999;\">正在扫描...</div>';";
        html += "";
        html += "    fetch('/scan')";
        html += "        .then(response => response.text())";
        html += "        .then(html => {";
        html += "            list.innerHTML = html;";
        html += "            btn.disabled = false;";
        html += "            btn.textContent = '🔄 重新扫描';";
        html += "            showMessage('扫描完成，请选择要连接的WiFi', 'info');";
        html += "        })";
        html += "        .catch(err => {";
        html += "            list.innerHTML = '<div style=\"padding:20px;text-align:center;color:#f44336;\">扫描失败</div>';";
        html += "            btn.disabled = false;";
        html += "            btn.textContent = '🔄 重新扫描';";
        html += "        });";
        html += "}";
        html += "";
        html += "function connectWifi() {";
        html += "    var ssid = document.getElementById('ssid').value.trim();";
        html += "    var password = document.getElementById('password').value;";
        html += "";
        html += "    if (!ssid) {";
        html += "        showMessage('请输入WiFi名称', 'error');";
        html += "        return;";
        html += "    }";
        html += "";
        html += "    document.getElementById('loading').classList.add('show');";
        html += "    document.getElementById('success').style.display = 'none';";
        html += "    document.getElementById('connectBtn').disabled = true;";
        html += "    showMessage('正在尝试连接到 ' + ssid + '...', 'info');";
        html += "";
        html += "    var formData = new FormData();";
        html += "    formData.append('ssid', ssid);";
        html += "    formData.append('password', password);";
        html += "";
        html += "    fetch('/connect', { method: 'POST', body: formData })";
        html += "        .then(response => response.json())";
        html += "        .then(data => {";
        html += "            if (data.success) {";
        html += "                showMessage(data.message, 'info');";
        html += "                // 开始轮询连接状态";
        html += "                pollStatus();";
        html += "            } else {";
        html += "                document.getElementById('loading').classList.remove('show');";
        html += "                document.getElementById('connectBtn').disabled = false;";
        html += "                showMessage(data.message, 'error');";
        html += "            }";
        html += "        })";
        html += "        .catch(err => {";
        html += "            document.getElementById('loading').classList.remove('show');";
        html += "            document.getElementById('connectBtn').disabled = false;";
        html += "            showMessage('连接请求失败', 'error');";
        html += "        });";
        html += "}";
        html += "";
        html += "var pollCount = 0;";
        html += "var maxPolls = 30;";
        html += "";
        html += "function pollStatus() {";
        html += "    pollCount++;";
        html += "    if (pollCount > maxPolls) {";
        html += "        document.getElementById('loading').classList.remove('show');";
        html += "        document.getElementById('connectBtn').disabled = false;";
        html += "        showMessage('连接超时，请检查密码后重试', 'error');";
        html += "        pollCount = 0;";
        html += "        return;";
        html += "    }";
        html += "";
        html += "    fetch('/status')";
        html += "        .then(response => response.json())";
        html += "        .then(data => {";
        html += "            if (data.connected) {";
        html += "                // 连接成功";
        html += "                document.getElementById('loading').classList.remove('show');";
        html += "                document.getElementById('success').style.display = 'block';";
        html += "                document.getElementById('ipAddr').textContent = data.ip;";
        html += "                showMessage('WiFi连接成功！', 'success');";
        html += "                pollCount = 0;";
        html += "            } else {";
        html += "                // 继续轮询";
        html += "                setTimeout(pollStatus, 1000);";
        html += "            }";
        html += "        })";
        html += "        .catch(err => {";
        html += "            setTimeout(pollStatus, 1000);";
        html += "        });";
        html += "}";
        html += "</script>";
        html += "</body>";
        html += "</html>";

        return html;
    }
};

#endif // CONFIG_PORTAL_H
