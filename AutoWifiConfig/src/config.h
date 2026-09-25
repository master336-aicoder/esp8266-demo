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

// ==================== EEPROM配置 ====================
#define EEPROM_SIZE             512     // EEPROM大小
#define EEPROM_MAGIC            0xA5    // 魔术字，用于验证数据有效性
#define EEPROM_MAGIC_ADDR       0       // 魔术字地址
#define EEPROM_SSID_ADDR        1       // SSID存储起始地址
#define EEPROM_PASSWORD_ADDR    33      // 密码存储起始地址（SSID最多32字节）
#define SSID_MAX_LENGTH         32      // SSID最大长度
#define PASSWORD_MAX_LENGTH     64      // 密码最大长度

// ==================== LED状态闪烁配置 ====================
// 不同状态的LED闪烁频率（毫秒）
#define BLINK_SLOW              2000    // 慢闪：已连接WiFi（正常运行）
#define BLINK_MEDIUM            500     // 中闪：正在连接WiFi
#define BLINK_FAST              200     // 快闪：配网模式
#define BLINK_VERY_FAST         100     // 超快闪：连接失败/错误
#define LED_STEADY_ON           0       // 常亮
#define LED_STEADY_OFF          -1      // 常灭

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
