/*
 * ESP32-CAM 最小测试程序
 * 功能：串口输出 + WiFi AP + LED闪烁
 */

#include <Arduino.h>
#include <WiFi.h>

#define LED_PIN 4  // ESP32-CAM 闪光灯LED

const char* AP_SSID = "ESP32-CAM-Test";
const char* AP_PASSWORD = "12345678";

void setup() {
    Serial.begin(115200);
    delay(500);
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("   ESP32-CAM 硬件测试程序");
    Serial.println("========================================");
    Serial.printf("芯片: %s\r\n", ESP.getChipModel());
    Serial.printf("CPU: %d MHz\r\n", ESP.getCpuFreqMHz());
    Serial.printf("Flash: %d MB\r\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("可用内存: %d bytes\r\n", ESP.getFreeHeap());
    Serial.printf("MAC地址: %s\r\n", WiFi.macAddress().c_str());
    Serial.println();
    
    // LED初始化
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    
    // 启动WiFi AP模式
    Serial.println("[WiFi] 启动AP模式...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[WiFi] AP名称: %s\r\n", AP_SSID);
    Serial.printf("[WiFi] AP密码: %s\r\n", AP_PASSWORD);
    Serial.printf("[WiFi] AP地址: http://%s/\r\n", ip.toString().c_str());
    Serial.println();
    Serial.println("✅ 启动完成！LED每秒闪烁一次");
    Serial.println();
}

void loop() {
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
    
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        Serial.printf("[运行中] uptime=%lds, 连接数=%d, 内存=%d\r\n", 
                      millis()/1000, WiFi.softAPgetStationNum(), ESP.getFreeHeap());
    }
}
