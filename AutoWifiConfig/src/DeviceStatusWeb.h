/*
 * DeviceStatusWeb - 设备状态监控Web服务
 * WiFi连接成功后启动，展示所有GPIO接口状态和系统信息
 */

#ifndef DEVICE_STATUS_WEB_H
#define DEVICE_STATUS_WEB_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>

// GPIO引脚定义（ESP8266可用引脚）
struct GpioInfo {
    uint8_t pin;
    const char* name;
    const char* label;
};

// ESP8266 常用GPIO列表
const GpioInfo GPIO_LIST[] = {
    { 0,  "GPIO0",  "D0 / GPIO0" },
    { 1,  "GPIO1",  "TX / GPIO1" },
    { 2,  "GPIO2",  "D4 / GPIO2" },
    { 3,  "GPIO3",  "RX / GPIO3" },
    { 4,  "GPIO4",  "D2 / GPIO4" },
    { 5,  "GPIO5",  "D1 / GPIO5" },
    { 12, "GPIO12", "D6 / GPIO12" },
    { 13, "GPIO13", "D7 / GPIO13" },
    { 14, "GPIO14", "D5 / GPIO14" },
    { 15, "GPIO15", "D8 / GPIO15" },
    { 16, "GPIO16", "D0 / GPIO16" },
};
const int GPIO_COUNT = sizeof(GPIO_LIST) / sizeof(GPIO_LIST[0]);

class DeviceStatusWeb {
public:
    DeviceStatusWeb(uint16_t port = 80) : _server(port) {}

    void begin() {
        // 配置路由
        _server.on("/", std::bind(&DeviceStatusWeb::handleRoot, this));
        _server.on("/api/status", std::bind(&DeviceStatusWeb::handleApiStatus, this));
        _server.on("/api/gpio", std::bind(&DeviceStatusWeb::handleApiGpio, this));
        _server.onNotFound(std::bind(&DeviceStatusWeb::handleNotFound, this));

        _server.begin();
        Serial.println(F("[状态页] 设备状态Web服务已启动"));
        Serial.printf("[状态页] 访问地址: http://%s\r\n", WiFi.localIP().toString().c_str());
    }

    void stop() {
        _server.stop();
        Serial.println(F("[状态页] 设备状态Web服务已停止"));
    }

    void handleClient() {
        _server.handleClient();
    }

private:
    ESP8266WebServer _server;

    // 获取GPIO模式字符串
    String getPinModeStr(uint8_t pin) {
        // 简化：通过读取输出寄存器判断是否为输出模式
        if (pin < 16) {
            // 检查 GPIO_ENABLE 寄存器
            uint32_t enable_reg = READ_PERI_REG(PERIPHS_GPIO_BASEADDR + GPIO_ENABLE_ADDRESS);
            if (enable_reg & (1 << pin)) {
                return "输出";
            }
        }
        return "输入";
    }

    // 主页
    void handleRoot() {
        String html = getStatusPageHTML();
        _server.send(200, "text/html", html);
    }

    // API: 完整状态
    void handleApiStatus() {
        String json = "{";
        
        // 系统信息
        json += "\"system\":{";
        json += "\"chip_id\":\"0x" + String(ESP.getChipId(), HEX) + "\",";
        json += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz()) + ",";
        json += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
        json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"sketch_size\":" + String(ESP.getSketchSize()) + ",";
        json += "\"free_sketch_space\":" + String(ESP.getFreeSketchSpace()) + ",";
        json += "\"uptime\":" + String(millis() / 1000);
        json += "},";
        
        // WiFi信息
        json += "\"wifi\":{";
        json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"mac\":\"" + WiFi.macAddress() + "\",";
        json += "\"gateway\":\"" + WiFi.gatewayIP().toString() + "\",";
        json += "\"subnet\":\"" + WiFi.subnetMask().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"channel\":" + String(WiFi.channel());
        json += "},";
        
        // GPIO列表
        json += "\"gpio\":[";
        for (int i = 0; i < GPIO_COUNT; i++) {
            if (i > 0) json += ",";
            int val = digitalRead(GPIO_LIST[i].pin);
            String mode = getPinModeStr(GPIO_LIST[i].pin);
            json += "{\"pin\":" + String(GPIO_LIST[i].pin) + ",";
            json += "\"name\":\"" + String(GPIO_LIST[i].name) + "\",";
            json += "\"label\":\"" + String(GPIO_LIST[i].label) + "\",";
            json += "\"value\":" + String(val) + ",";
            json += "\"mode\":\"" + mode + "\"}";
        }
        json += "]";
        
        json += "}";
        _server.send(200, "application/json", json);
    }

    // API: GPIO状态（简化版）
    void handleApiGpio() {
        String json = "[";
        for (int i = 0; i < GPIO_COUNT; i++) {
            if (i > 0) json += ",";
            int val = digitalRead(GPIO_LIST[i].pin);
            json += "{\"pin\":" + String(GPIO_LIST[i].pin) + ",";
            json += "\"value\":" + String(val) + "}";
        }
        json += "]";
        _server.send(200, "application/json", json);
    }

    // 404
    void handleNotFound() {
        _server.send(404, "text/plain", "Not Found");
    }

    // 状态页面HTML
    String getStatusPageHTML() {
        String html = "<!DOCTYPE html><html lang='zh-CN'><head>";
        html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<meta http-equiv='refresh' content='5'>";
        html += "<title>ESP8266 设备状态</title>";
        html += "<style>";
        html += "*{box-sizing:border-box;margin:0;padding:0}";
        html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f0f2f5;min-height:100vh;padding:20px}";
        html += ".container{max-width:900px;margin:0 auto}";
        html += "h1{text-align:center;color:#1a1a2e;margin-bottom:20px;font-size:24px}";
        html += ".card{background:#fff;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,.08);margin-bottom:20px;overflow:hidden}";
        html += ".card-header{background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;padding:16px 20px;font-size:16px;font-weight:600}";
        html += ".card-body{padding:20px}";
        html += ".info-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px}";
        html += ".info-item{background:#f8f9fa;padding:12px 15px;border-radius:8px}";
        html += ".info-label{font-size:12px;color:#888;margin-bottom:4px}";
        html += ".info-value{font-size:15px;color:#333;font-weight:500;font-family:monospace}";
        html += ".gpio-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:12px}";
        html += ".gpio-item{background:#f8f9fa;padding:14px;border-radius:10px;text-align:center;transition:all .2s}";
        html += ".gpio-pin{font-size:12px;color:#888;margin-bottom:6px}";
        html += ".gpio-val{font-size:24px;font-weight:700;margin-bottom:4px}";
        html += ".gpio-val.high{color:#4caf50}";
        html += ".gpio-val.low{color:#f44336}";
        html += ".gpio-mode{font-size:11px;color:#999}";
        html += ".rssi-bar{height:6px;background:#e0e0e0;border-radius:3px;overflow:hidden;margin-top:6px}";
        html += ".rssi-fill{height:100%;background:linear-gradient(90deg,#f44336,#ff9800,#4caf50);border-radius:3px;transition:width .3s}";
        html += ".footer{text-align:center;color:#999;font-size:12px;margin-top:20px}";
        html += ".badge{display:inline-block;padding:3px 10px;border-radius:20px;font-size:11px;font-weight:600}";
        html += ".badge-green{background:#e8f5e9;color:#2e7d32}";
        html += ".badge-red{background:#ffebee;color:#c62828}";
        html += "</style></head><body>";
        html += "<div class='container'>";
        html += "<h1>&#128225; ESP8266 设备状态监控</h1>";

        // 系统信息卡片
        html += "<div class='card'><div class='card-header'>&#128187; 系统信息</div><div class='card-body'>";
        html += "<div class='info-grid'>";
        html += "<div class='info-item'><div class='info-label'>芯片ID</div><div class='info-value'>0x" + String(ESP.getChipId(), HEX) + "</div></div>";
        html += "<div class='info-item'><div class='info-label'>CPU频率</div><div class='info-value'>" + String(ESP.getCpuFreqMHz()) + " MHz</div></div>";
        html += "<div class='info-item'><div class='info-label'>Flash大小</div><div class='info-value'>" + String(ESP.getFlashChipSize() / 1024) + " KB</div></div>";
        html += "<div class='info-item'><div class='info-label'>可用内存</div><div class='info-value'>" + String(ESP.getFreeHeap()) + " 字节</div></div>";
        html += "<div class='info-item'><div class='info-label'>固件大小</div><div class='info-value'>" + String(ESP.getSketchSize() / 1024) + " KB</div></div>";
        html += "<div class='info-item'><div class='info-label'>运行时间</div><div class='info-value'>" + formatUptime(millis() / 1000) + "</div></div>";
        html += "</div></div></div>";

        // WiFi信息卡片
        int rssi = WiFi.RSSI();
        int signalPercent = (rssi + 100) * 2; // -100~0dBm -> 0~200%, 限制一下
        if (signalPercent > 100) signalPercent = 100;
        if (signalPercent < 0) signalPercent = 0;

        html += "<div class='card'><div class='card-header'>&#128246; WiFi 状态</div><div class='card-body'>";
        html += "<div class='info-grid'>";
        html += "<div class='info-item'><div class='info-label'>SSID</div><div class='info-value'>" + String(WiFi.SSID()) + "</div></div>";
        html += "<div class='info-item'><div class='info-label'>IP地址</div><div class='info-value'>" + WiFi.localIP().toString() + "</div></div>";
        html += "<div class='info-item'><div class='info-label'>MAC地址</div><div class='info-value'>" + WiFi.macAddress() + "</div></div>";
        html += "<div class='info-item'><div class='info-label'>网关</div><div class='info-value'>" + WiFi.gatewayIP().toString() + "</div></div>";
        html += "<div class='info-item'><div class='info-label'>信号强度</div><div class='info-value'>" + String(rssi) + " dBm";
        html += "<div class='rssi-bar'><div class='rssi-fill' style='width:" + String(signalPercent) + "%'></div></div>";
        html += "</div></div>";
        html += "<div class='info-item'><div class='info-label'>信道</div><div class='info-value'>" + String(WiFi.channel()) + "</div></div>";
        html += "</div></div></div>";

        // GPIO状态卡片
        html += "<div class='card'><div class='card-header'>&#128268; GPIO 接口状态</div><div class='card-body'>";
        html += "<div class='gpio-grid'>";

        for (int i = 0; i < GPIO_COUNT; i++) {
            int val = digitalRead(GPIO_LIST[i].pin);
            String mode = getPinModeStr(GPIO_LIST[i].pin);
            const char* valClass = val ? "high" : "low";
            const char* valText = val ? "HIGH" : "LOW";

            html += "<div class='gpio-item'>";
            html += "<div class='gpio-pin'>" + String(GPIO_LIST[i].label) + "</div>";
            html += "<div class='gpio-val " + String(valClass) + "'>" + String(valText) + "</div>";
            html += "<div class='gpio-mode'>" + mode + "</div>";
            html += "</div>";
        }

        html += "</div></div></div>";

        html += "<div class='footer'>页面每5秒自动刷新 | ESP8266 AutoWifiConfig v1.0</div>";
        html += "</div></body></html>";

        return html;
    }

    // 格式化运行时间
    String formatUptime(unsigned long seconds) {
        unsigned long days = seconds / 86400;
        unsigned long hours = (seconds % 86400) / 3600;
        unsigned long mins = (seconds % 3600) / 60;
        unsigned long secs = seconds % 60;

        String result;
        if (days > 0) {
            result += String(days) + "天 ";
        }
        if (hours > 0 || days > 0) {
            result += String(hours) + "时";
        }
        result += String(mins) + "分" + String(secs) + "秒";
        return result;
    }
};

#endif // DEVICE_STATUS_WEB_H
