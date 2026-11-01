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
unsigned long g_lastHeartbeat = 0;  // 串口心跳调试

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
    // 最优先把 LED 拉到熄灭电平，避免上电后补光灯常亮
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);

    Serial.begin(115200);
    delay(1000);

    setCpuFrequencyMhz(240);

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
    if (psramFound()) {
        Serial.printf("[SYS] PSRAM 总量: %lu KB, 空闲: %lu KB\r\n",
            (unsigned long)(ESP.getPsramSize() / 1024),
            (unsigned long)(ESP.getFreePsram() / 1024));
    }
    Serial.printf("[SYS] 可用内存: %d KB\r\n", ESP.getFreeHeap() / 1024);
    Serial.println();

    // EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // LED 指示
    flashLed(2, 200);

    // WiFi 初始化（仅加载配置，暂不连接）
    g_wifi.begin();

    // 摄像头 & SD 卡优先初始化（避开 WiFi 关联时的电流峰值）
    if (!g_camera.begin()) {
        Serial.println("[ERR] 摄像头初始化失败！");
    }

    if (!g_sd.begin()) {
        Serial.println("[ERR] SD卡未就绪，拍照保存与录像功能不可用");
    }

    // 时间
    g_time.begin();

    // 录制器
    g_recorder = new VideoRecorder(&g_camera, &g_sd);

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
        // 已连上路由器：启动完整 Web 管理（设置页里也能改 WiFi）
        g_web = new WebManager(&g_camera, &g_sd, g_recorder, &g_wifi, &g_time);
        g_web->begin();
        g_state = STATE_RUNNING;

        Serial.println();
        Serial.println("[SYS] WiFi 连接成功，进入运行模式");
        Serial.printf("[SYS] IP地址: %s\r\n", WiFi.localIP().toString().c_str());
        Serial.printf("[SYS] 信号强度: %d dBm\r\n", WiFi.RSSI());
        Serial.printf("[SYS] Web管理: http://%s/\r\n", WiFi.localIP().toString().c_str());
        flashLed(3, 100);
    } else {
        // 无配置或连接失败：自动进入配网模式（AP + 配网门户，独占 80 端口）
        g_state = STATE_PORTAL;
        g_wifi.startPortal();

        Serial.println();
        Serial.println("[SYS] 未连接路由器，已进入自动配网模式");
        Serial.printf("[SYS] 请连接热点: %s（开放网络）\r\n", AP_SSID);
        Serial.println("[SYS] 浏览器打开: http://192.168.4.1/");
        Serial.println("[SYS] 选择 WiFi 并输入密码，配网成功后设备自动重启");
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
            // 配网模式下串口心跳：持续有输出，便于手动配网时观察
            {
                unsigned long now = millis();
                if (now - g_lastHeartbeat >= 8000) {
                    g_lastHeartbeat = now;
                    IPAddress a = WiFi.softAPIP();
                    char ip[32];
                    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
                    Serial.printf("[HB] uptime=%lus mode=PORTAL ap_clients=%u heap=%lu min=%lu ip=%s\r\n",
                        now / 1000, (unsigned)WiFi.softAPgetStationNum(),
                        ESP.getFreeHeap(), ESP.getMinFreeHeap(), ip);
                }
            }
            break;

        case STATE_RUNNING:
            // 运行模式
            if (g_web) g_web->loop();
            g_time.loop();
            g_wifi.maintainWifi();  // 掉线自动重连

            // 串口心跳（每 8 秒）：全部使用栈缓冲，避免临时 String 造成堆碎片
            {
                unsigned long now = millis();
                if (now - g_lastHeartbeat >= 8000) {
                    g_lastHeartbeat = now;
                    bool sta = WiFi.status() == WL_CONNECTED;

                    char ip[32];
                    if (sta) {
                        IPAddress a = WiFi.localIP();
                        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
                    } else {
                        IPAddress a = WiFi.softAPIP();
                        snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
                    }

                    char tm[32];
                    g_time.getDateTimeChars(tm, sizeof(tm));

                    Serial.printf("[HB] uptime=%lus heap=%lu min=%lu wifi=%s ip=%s rssi=%d sd=%s rec=%s frames=%u time=%s\r\n",
                        now / 1000, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                        sta ? "STA" : (WiFi.getMode() == WIFI_AP ? "AP" : "NONE"),
                        ip, WiFi.RSSI(),
                        g_sd.isInitialized() ? "OK" : "NO",
                        (g_recorder && g_recorder->isRecording()) ? "ON" : "OFF",
                        g_recorder ? g_recorder->getFrameCount() : 0,
                        tm);
                }
            }

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

    delay(2);
}
