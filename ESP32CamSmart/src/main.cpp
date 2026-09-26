/*
 * ESP32-CAM 智能摄像头 - 主程序
 * 功能：自动配网 + Web管理 + 拍照 + 视频录制 + SD卡管理 + NTP时间同步
 */

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"
#include "WifiManager.h"
#include "CameraManager.h"
#include "SdCardManager.h"
#include "VideoRecorder.h"
#include "TimeManager.h"
#include "WebManager.h"

// 全局对象
WifiManager g_wifi;
CameraManager g_camera;
SdCardManager g_sd;
VideoRecorder* g_recorder;
TimeManager g_time;
WebManager* g_web;

// 系统状态
PortalState g_state = STATE_STARTUP;
unsigned long g_lastRecFrame = 0;

// ========== 辅助函数 ==========

void flashLed(int times, int delayMs) {
    pinMode(LED_PIN, OUTPUT);
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, LED_ON);
        delay(delayMs);
        digitalWrite(LED_PIN, LED_OFF);
        delay(delayMs);
    }
}

// ========== 启动初始化 ==========

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("   ESP32-CAM 智能摄像头");
    Serial.println("========================================");
    Serial.println();

    // 系统信息
    Serial.printf("[SYS] 芯片: %s\r\n", ESP.getChipModel());
    Serial.printf("[SYS] CPU: %d MHz\r\n", ESP.getCpuFreqMHz());
    Serial.printf("[SYS] Flash: %d MB\r\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("[SYS] PSRAM: %s\r\n", psramFound() ? "已检测到" : "未检测到");
    Serial.printf("[SYS] 可用内存: %d KB\r\n", ESP.getFreeHeap() / 1024);
    Serial.println();

    // EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // LED 指示
    flashLed(2, 200);

    // WiFi 初始化
    g_wifi.begin();

    // 尝试连接 WiFi
    g_state = STATE_WIFI_CONNECTING;
    bool wifiOk = false;

    if (g_wifi.hasConfig()) {
        for (int i = 0; i < WIFI_RETRY_COUNT; i++) {
            Serial.printf("[SYS] WiFi 连接尝试 %d/%d...\r\n", i + 1, WIFI_RETRY_COUNT);
            if (g_wifi.connect()) {
                wifiOk = true;
                break;
            }
            delay(WIFI_RETRY_DELAY);
        }
    }

    if (wifiOk) {
        // WiFi 连接成功，进入正常运行模式
        g_state = STATE_RUNNING;
        Serial.println();
        Serial.println("[SYS] WiFi 连接成功，进入运行模式");

        // 初始化摄像头
        if (!g_camera.begin()) {
            Serial.println("[ERR] 摄像头初始化失败！");
        }

        // 初始化 SD 卡
        g_sd.begin();  // SD 卡可能不存在，失败也没关系

        // 初始化时间
        g_time.begin();

        // 初始化录制器
        g_recorder = new VideoRecorder(&g_camera, &g_sd);

        // 初始化 Web 管理
        g_web = new WebManager(&g_camera, &g_sd, g_recorder, &g_wifi, &g_time);
        g_web->begin();

        flashLed(3, 100);

    } else {
        // WiFi 连接失败，进入配网模式
        g_state = STATE_PORTAL;
        Serial.println();
        Serial.println("[SYS] WiFi 连接失败，进入配网模式");
        Serial.printf("[SYS] 请连接热点: %s\r\n", AP_SSID);
        Serial.println("[SYS] 访问: http://192.168.4.1/");

        g_wifi.startPortal();

        flashLed(5, 300);
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("  启动完成！");
    Serial.println("========================================");
}

// ========== 主循环 ==========

void loop() {
    switch (g_state) {
        case STATE_PORTAL:
            // 配网模式：处理配网门户
            g_wifi.loop();
            break;

        case STATE_RUNNING:
            // 运行模式
            if (g_web) g_web->loop();
            g_time.loop();

            // 视频录制处理
            if (g_recorder && g_recorder->isRecording()) {
                // 按 FPS 定时抓取帧
                unsigned long now = millis();
                unsigned long frameInterval = 1000 / g_recorder->getFps();
                if (now - g_lastRecFrame >= frameInterval) {
                    g_lastRecFrame = now;
                    g_recorder->captureAndAddFrame();
                }
            }
            break;

        default:
            break;
    }

    delay(1);
}
