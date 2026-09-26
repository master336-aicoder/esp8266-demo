/*
 * ESP32-CAM 智能摄像头 - 全局配置
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============== 调试输出 ==============
#define ENABLE_DEBUG    true

#if ENABLE_DEBUG
  #define DEBUG_PRINT(x)        Serial.print(x)
  #define DEBUG_PRINTLN(x)      Serial.println(x)
  #define DEBUG_PRINTF(...)     Serial.printf(__VA_ARGS__)
  #define DEBUG_PRINTLN_TAG(t, x)  Serial.printf("[%s] %s\r\n", t, x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
  #define DEBUG_PRINTLN_TAG(t, x)
#endif

// ============== 摄像头引脚 (AI-Thinker ESP32-CAM) ==============
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ============== LED 引脚 ==============
#define LED_PIN           4    // 闪光灯引脚（也做状态指示）
#define LED_ON            LOW
#define LED_OFF           HIGH

// ============== SD 卡引脚 ==============
#define SD_CS_PIN          13
#define SD_MOSI_PIN        15
#define SD_MISO_PIN        2
#define SD_SCK_PIN         14

// ============== WiFi 配置 ==============
#define WIFI_RETRY_COUNT   3
#define WIFI_RETRY_DELAY   2000  // ms
#define AP_SSID            "ESP32-CAM_Config"
#define AP_PASSWORD        ""    // 留空则无密码

// ============== EEPROM 地址 ==============
#define EEPROM_SIZE        512
#define EEPROM_MAGIC       0x5A  // 标识位，表示配置已保存

// WiFi 配置
#define EEP_ADDR_MAGIC     0
#define EEP_ADDR_SSID      1     // 32字节
#define EEP_ADDR_PASS     33     // 64字节
#define EEP_ADDR_HOSTNAME 97     // 32字节

// NTP 配置
#define EEP_ADDR_NTP_EN   129    // 1字节
#define EEP_ADDR_NTP_SVR1 130    // 64字节
#define EEP_ADDR_NTP_SVR2 194    // 64字节
#define EEP_ADDR_NTP_TZ   258    // 1字节（时区偏移）

// 摄像头配置
#define EEP_ADDR_CAM_RES  260    // 1字节
#define EEP_ADDR_CAM_QUAL 261    // 1字节
#define EEP_ADDR_CAM_EFF  262    // 1字节
#define EEP_ADDR_CAM_FLIP 263    // 1字节
#define EEP_ADDR_CAM_MIRROR 264  // 1字节

// ============== 配网状态 ==============
enum PortalState {
    STATE_STARTUP = 0,
    STATE_WIFI_CONNECTING,
    STATE_PORTAL,
    STATE_RUNNING
};

// ============== 函数声明 ==============
String formatBytes(size_t bytes);

#endif // CONFIG_H
