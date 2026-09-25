/*
 * AutoWifiConfig - 配置文件
 * 自动配网功能的所有配置参数
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== 引脚配置 ====================
#define LED_PIN             2       // ESP8266板载LED引脚 (GPIO2)
#define LED_ON_LEVEL        LOW     // LED点亮电平（低电平点亮，因为大多数ESP12模块是共阳的）

// ==================== WiFi配置 ====================
#define WIFI_CONNECT_TIMEOUT    10000   // 单次连接超时时间（毫秒）
#define WIFI_RETRY_COUNT        3       // 连接失败重试次数
#define WIFI_RECONNECT_DELAY    2000    // 重连间隔（毫秒）

// ==================== 配网AP配置 ====================
#define AP_SSID                 "ESP8266_Config"   // 配网模式AP名称
#define AP_PASSWORD             ""                  // 配网模式AP密码（空表示无密码）
#define AP_CHANNEL              1                   // AP信道
#define AP_MAX_CONNECTIONS      4                   // 最大连接数

// ==================== Web服务器配置 ====================
#define WEB_SERVER_PORT         80      // Web服务器端口

// ==================== MQTT默认配置 ====================
#define MQTT_DEFAULT_HOST       "nas.master336.cn"   // 默认MQTT服务器地址
#define MQTT_DEFAULT_PORT       8086                   // 默认MQTT端口
#define MQTT_DEFAULT_USER       "zd"                   // 默认用户名
#define MQTT_DEFAULT_PASS       "123456"               // 默认密码
#define MQTT_DEFAULT_TOPIC      "esp8266/status"       // 默认状态上报主题
#define MQTT_DEFAULT_INFO_TOPIC "esp8266/info"         // 默认设备信息上报主题
#define MQTT_DEFAULT_INTERVAL   5000                   // 默认上报间隔（毫秒）
#define MQTT_RECONNECT_DELAY    5000                   // 重连间隔（毫秒）

// ==================== NTP时间配置 ====================
#define NTP_DEFAULT_SERVER1     "ntp.aliyun.com"        // 默认NTP服务器1
#define NTP_DEFAULT_SERVER2     "ntp1.aliyun.com"       // 默认NTP服务器2
#define NTP_DEFAULT_SERVER3     "time.windows.com"      // 默认NTP服务器3
#define NTP_DEFAULT_TIMEZONE    8                       // 默认时区（东8区）
#define NTP_DEFAULT_INTERVAL    1800000                 // 默认同步间隔（毫秒，30分钟）

// NTP配置存储地址（MQTT结束于396，NTP从400开始）
#define EEPROM_NTP_MAGIC_ADDR   400     // NTP魔术字地址
#define EEPROM_NTP_MAGIC        0xC9    // NTP魔术字
#define EEPROM_NTP_SERVER1_ADDR 401     // NTP服务器1（32字节）
#define EEPROM_NTP_SERVER2_ADDR 433     // NTP服务器2（24字节）
#define EEPROM_NTP_SERVER3_ADDR 457     // NTP服务器3（24字节）
#define EEPROM_NTP_TZ_ADDR      481     // 时区（1字节，偏移12存储）
#define EEPROM_NTP_INTERVAL_ADDR 482    // 同步间隔（4字节，毫秒）

// ==================== EEPROM配置 ====================
#define EEPROM_SIZE             512     // EEPROM大小
#define EEPROM_MAGIC            0xA5    // 魔术字，用于验证数据有效性
#define EEPROM_MAGIC_ADDR       0       // 魔术字地址
#define EEPROM_SSID_ADDR        1       // SSID存储起始地址
#define EEPROM_PASSWORD_ADDR    33      // 密码存储起始地址（SSID最多32字节）
#define SSID_MAX_LENGTH         32      // SSID最大长度
#define PASSWORD_MAX_LENGTH     64      // 密码最大长度

// MQTT配置存储地址
#define EEPROM_MQTT_MAGIC_ADDR  97      // MQTT配置魔术字地址
#define EEPROM_MQTT_MAGIC       0xB8    // MQTT配置魔术字（v3版本，增加client_id和info_topic）
#define MQTT_HOST_MAX_LENGTH    64      // MQTT服务器地址最大长度
#define MQTT_USER_MAX_LENGTH    32      // MQTT用户名最大长度
#define MQTT_PASS_MAX_LENGTH    32      // MQTT密码最大长度
#define MQTT_TOPIC_MAX_LENGTH   64      // MQTT主题最大长度
#define MQTT_CLIENTID_MAX_LENGTH 32     // Client ID最大长度
#define EEPROM_MQTT_HOST_ADDR   98      // MQTT服务器地址
#define EEPROM_MQTT_PORT_ADDR   162     // MQTT端口（2字节）
#define EEPROM_MQTT_USER_ADDR   164     // MQTT用户名
#define EEPROM_MQTT_PASS_ADDR   196     // MQTT密码
#define EEPROM_MQTT_TOPIC_ADDR  228     // 状态上报主题
#define EEPROM_MQTT_CLIENTID_ADDR 292   // Client ID
#define EEPROM_MQTT_INTERVAL_ADDR 324   // 上报间隔（4字节，毫秒）
#define EEPROM_MQTT_PINS_ADDR   328     // 上报引脚掩码（4字节，支持GPIO0-16）
#define EEPROM_MQTT_ENABLE_ADDR 332     // MQTT上报使能（1字节）
#define EEPROM_MQTT_INFO_TOPIC_ADDR 333 // 设备信息上报主题

// ==================== LED状态闪烁配置 ====================
// 不同状态的LED闪烁频率（毫秒）
#define BLINK_SLOW              2000    // 慢闪：已连接WiFi（正常运行）
#define BLINK_MEDIUM            500     // 中闪：正在连接WiFi
#define BLINK_FAST              200     // 快闪：配网模式
#define BLINK_VERY_FAST         100     // 超快闪：连接失败/错误
#define LED_STEADY_ON           0       // 常亮
#define LED_STEADY_OFF          -1      // 常灭

// ==================== GPIO模式跟踪 ====================
// 由于GPIO16是RTC引脚，无法通过常规寄存器读取模式
// 使用全局数组跟踪所有GPIO的模式
#include <Arduino.h>
extern uint8_t g_gpioMode[17]; // GPIO0~GPIO16

// 获取引脚模式（INPUT/OUTPUT/INPUT_PULLUP）
inline String getGpioModeStr(uint8_t pin) {
    if (pin > 16) return "UNKNOWN";
    switch (g_gpioMode[pin]) {
        case OUTPUT: return "OUTPUT";
        case INPUT_PULLUP: return "INPUT_PULLUP";
        default: return "INPUT";
    }
}

// 设置引脚模式（同时更新跟踪数组）
inline void setGpioMode(uint8_t pin, uint8_t mode) {
    if (pin > 16) return;
    pinMode(pin, mode);
    g_gpioMode[pin] = mode;
}

// ==================== 调试配置 ====================
#define DEBUG_SERIAL            Serial  // 调试串口
#define DEBUG_BAUD_RATE         115200  // 调试串口波特率
#define ENABLE_DEBUG            true    // 是否启用调试输出

// 调试输出宏
#if ENABLE_DEBUG
  #define DEBUG_PRINT(x)        DEBUG_SERIAL.print(x)
  #define DEBUG_PRINTLN(x)      DEBUG_SERIAL.println(x)
  #define DEBUG_PRINTF(...)     DEBUG_SERIAL.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H
