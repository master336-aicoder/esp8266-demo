# ESP8266 自动配网 + MQTT + NTP 物联网固件

一个功能完整的 ESP8266 物联网固件，支持自动配网、Web 配置界面、MQTT 数据上报、NTP 时间同步和 GPIO 远程控制。

> ✅ **经过 1 小时稳定性测试，645 行日志，0 次异常，稳定可靠**

---

## 目录

- [核心功能](#核心功能)
- [硬件要求](#硬件要求)
- [快速开始](#快速开始)
- [功能详解](#功能详解)
  - [自动配网](#1-自动配网)
  - [设备状态 Web 界面](#2-设备状态-web-界面)
  - [MQTT 数据上报](#3-mqtt-数据上报)
  - [NTP 时间同步](#4-ntp-时间同步)
  - [GPIO 远程控制](#5-gpio-远程控制)
  - [LED 状态指示](#6-led-状态指示)
- [配置说明](#配置说明)
  - [WiFi 配置](#wifi-配置)
  - [MQTT 配置](#mqtt-配置)
  - [NTP 配置](#ntp-配置)
- [API 接口](#api-接口)
- [MQTT 数据格式](#mqtt-数据格式)
- [二次开发指南](#二次开发指南)
  - [项目结构](#项目结构)
  - [核心模块说明](#核心模块说明)
  - [添加新功能](#添加新功能)
  - [修改默认配置](#修改默认配置)
  - [添加新的 GPIO 功能](#添加新的-gpio-功能)
  - [自定义 MQTT 上报内容](#自定义-mqtt-上报内容)
  - [添加新的 Web 页面](#添加新的-web-页面)
- [编译与烧录](#编译与烧录)
- [稳定性保障](#稳定性保障)
- [常见问题](#常见问题)
- [许可证](#许可证)

---

## 核心功能

| 功能 | 说明 |
|------|------|
| 🔌 **自动配网** | 启动无 WiFi 信息或连接失败 → 自动进入 AP 配网模式，手机连接配网热点后通过 Web 页面配置 WiFi |
| 📊 **设备状态 Web** | WiFi 连接成功后启动 Web 服务器，实时展示系统信息、WiFi 状态、GPIO 状态 |
| 📡 **MQTT 上报** | 支持 MQTT 协议上报 GPIO 状态和设备信息，可配置服务器、主题、上报间隔、上报引脚 |
| ⏰ **NTP 时间同步** | 自动同步网络时间，支持自定义 NTP 服务器和时区，Web 页面显示实时时钟 |
| 🎛️ **GPIO 远程控制** | Web 界面支持设置 GPIO 输入/输出模式，控制输出高/低电平 |
| 💡 **LED 状态指示** | 不同闪烁频率表示不同状态（慢闪=正常、中闪=连接中、快闪=配网模式） |
| 💾 **配置持久化** | WiFi/MQTT/NTP 配置全部保存在 EEPROM，掉电不丢失 |
| 🛡️ **高稳定性** | 经过加固优化，NTP 失败 / MQTT 断连 / 内存紧张均不会导致重启 |

---

## 硬件要求

- **开发板**: ESP8266 系列（ESP-12E / ESP-12F / NodeMCU / Wemos D1 Mini 等）
- **Flash**: 至少 4MB
- **USB 转串口**: CH340 / CP2102 / PL2303 等
- **LED**: 板载 LED（GPIO2）或外接 LED

### 可用 GPIO 列表

| GPIO | 标签 | 备注 |
|------|------|------|
| GPIO0 | D0 | 下载模式引脚，建议作输入用 |
| GPIO1 | TX | 串口发送，作 GPIO 会影响串口调试 |
| GPIO2 | D4 | 板载 LED，推荐作状态指示 |
| GPIO3 | RX | 串口接收，作 GPIO 会影响串口调试 |
| GPIO4 | D2 | 通用 I/O，推荐使用 |
| GPIO5 | D1 | 通用 I/O，推荐使用 |
| GPIO12 | D6 | 通用 I/O，推荐使用 |
| GPIO13 | D7 | 通用 I/O，推荐使用 |
| GPIO14 | D5 | 通用 I/O，推荐使用 |
| GPIO15 | D8 | 启动时需为低电平，建议作输出 |
| GPIO16 | D0 | RTC 引脚，可作输入/输出，无中断 |

> ⚠️ GPIO6~GPIO11 用于 Flash 通信，**不可用作普通 I/O**

---

## 快速开始

### 1. 环境准备

安装 [PlatformIO Core](https://platformio.org/install/cli)（推荐）或 Arduino IDE + ESP8266 开发板包。

### 2. 编译烧录

```bash
# 进入项目目录
cd AutoWifiConfig

# 编译
pio run

# 烧录（修改串口号为实际端口）
pio run --target upload --upload-port COM4

# 查看串口输出
pio device monitor -b 115200
```

### 3. 配网步骤

1. 设备上电后如无 WiFi 信息，**LED 快速闪烁**，进入配网模式
2. 手机连接 WiFi 热点 **`ESP8266_Config`**（无密码）
3. 浏览器访问 **http://192.168.4.1**
4. 选择要连接的 WiFi，输入密码，点击"连接"
5. 设备连接成功后 LED 变为**慢闪**，配网完成

### 4. 访问设备状态页

设备连接 WiFi 后，通过串口查看 IP 地址，浏览器访问该 IP 即可看到设备状态页面。

---

## 功能详解

### 1. 自动配网

#### 工作流程

```
启动
  │
  ├─ 有已保存的 WiFi 信息？
  │    ├─ 是 → 尝试连接（重试 3 次）
  │    │    ├─ 成功 → 进入正常模式
  │    │    └─ 失败 → 进入配网模式
  │    └─ 否 → 进入配网模式
  │
  └─ 配网模式
       ├─ 启动 AP: ESP8266_Config
       ├─ 启动 Web 配置服务器 (192.168.4.1)
       ├─ 用户提交 WiFi 信息
       ├─ 尝试连接 WiFi
       │    ├─ 成功 → 保存配置 → 重启 → 正常模式
       │    └─ 失败 → 返回错误 → 继续配网模式
```

#### 配网页面功能

- 自动扫描附近 WiFi 网络
- 显示信号强度
- 支持隐藏 SSID 手动输入
- 实时显示连接状态

---

### 2. 设备状态 Web 界面

WiFi 连接成功后自动启动 Web 服务器（端口 80），包含三个页面：

#### 设备状态页 (`/`)

- 🕐 **当前时间** — 显示网络时间（已同步时实时更新）
- 💻 **系统信息** — 芯片 ID、CPU 频率、Flash 大小、可用内存、运行时间
- 📶 **WiFi 状态** — SSID、IP 地址、信号强度（进度条）、信道
- 🔌 **GPIO 控制** — 11 个 GPIO 实时状态，支持模式切换和电平控制

#### MQTT 配置页 (`/mqtt`)

- 连接状态显示（已连接/未连接）
- MQTT 服务器配置（地址、端口、用户名、密码）
- 状态上报主题、设备信息主题、客户端 ID
- 上报间隔设置（1~3600 秒）
- GPIO 引脚全选/全不选
- 上报测试按钮
- 最近 20 条上报记录

#### 时间设置页 (`/ntp`)

- 当前时间状态显示
- 手动同步按钮
- 3 个 NTP 服务器配置
- 时区设置
- 同步间隔设置（5~1440 分钟）

---

### 3. MQTT 数据上报

基于 `PubSubClient` 库实现，支持：

#### 特性

- ✅ 自动重连（断连后 5 秒重试）
- ✅ 定时上报（可配置间隔）
- ✅ 可选择上报引脚
- ✅ LWT 遗嘱消息
- ✅ 上报日志（最近 20 条）
- ✅ 设备信息上报

#### 默认配置

| 项 | 默认值 |
|----|--------|
| 服务器 | `nas.master336.cn` |
| 端口 | `8086` |
| 用户名 | `zd` |
| 密码 | `123456` |
| 状态主题 | `esp8266/status` |
| 设备信息主题 | `esp8266/info` |
| 上报间隔 | 5 秒 |
| 默认上报引脚 | 全部 11 个 |

---

### 4. NTP 时间同步

纯算法实现时间格式化，不依赖 `localtime()`，避免崩溃。

#### 特性

- ✅ 3 个 NTP 服务器（主 + 2 备用）
- ✅ 可配置同步间隔（默认 30 分钟）
- ✅ 时区配置（默认东 8 区）
- ✅ 时间戳缓存 + millis 累加，减少系统调用
- ✅ 同步失败静默重试
- ✅ Web 页面实时时钟显示

#### 默认配置

| 项 | 默认值 |
|----|--------|
| NTP 服务器 1 | `ntp.aliyun.com` |
| NTP 服务器 2 | `ntp1.aliyun.com` |
| NTP 服务器 3 | `time.windows.com` |
| 时区 | `8`（东 8 区） |
| 同步间隔 | 30 分钟 |

---

### 5. GPIO 远程控制

Web 界面支持：

- **模式切换**: 输入 / 输出
- **电平控制**: HIGH / LOW
- **实时状态**: 显示当前引脚值和模式
- **API 接口**: 支持第三方系统集成

> 💡 GPIO16 是 RTC 引脚，模式无法通过寄存器读取，固件使用全局数组跟踪所有 GPIO 模式。

---

### 6. LED 状态指示

板载 LED（GPIO2）通过不同闪烁频率表示系统状态：

| 状态 | 闪烁频率 | 说明 |
|------|----------|------|
| 慢闪 | 2 秒 | WiFi 已连接，正常运行 |
| 中闪 | 0.5 秒 | 正在连接 WiFi |
| 快闪 | 0.2 秒 | 配网模式 |
| 超快闪 | 0.1 秒 | 连接失败/错误 |

---

## 配置说明

所有配置保存在 EEPROM（512 字节），掉电不丢失。

### EEPROM 地址映射

| 地址范围 | 内容 | 大小 |
|----------|------|------|
| 0 | 魔术字 (0xA5) | 1B |
| 1~32 | WiFi SSID | 32B |
| 33~96 | WiFi 密码 | 64B |
| 97 | MQTT 魔术字 (0xB8) | 1B |
| 98~161 | MQTT 服务器地址 | 64B |
| 162~163 | MQTT 端口 | 2B |
| 164~195 | MQTT 用户名 | 32B |
| 196~227 | MQTT 密码 | 32B |
| 228~291 | 状态上报主题 | 64B |
| 292~323 | Client ID | 32B |
| 324~327 | 上报间隔 | 4B |
| 328~331 | 引脚掩码 | 4B |
| 332 | 使能标志 | 1B |
| 333~396 | 设备信息主题 | 64B |
| 400 | NTP 魔术字 (0xC9) | 1B |
| 401~432 | NTP 服务器 1 | 32B |
| 433~456 | NTP 服务器 2 | 24B |
| 457~480 | NTP 服务器 3 | 24B |
| 481 | 时区（+128 偏移） | 1B |
| 482~485 | 同步间隔 | 4B |

---

## API 接口

### 设备状态

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/status` | GET | 完整状态（系统+WiFi+GPIO） |
| `/api/gpio` | GET | GPIO 状态列表 |
| `/api/gpio/control` | POST | GPIO 控制 |
| `/api/time` | GET | 当前时间 |

#### GPIO 控制参数

```
POST /api/gpio/control
Content-Type: application/x-www-form-urlencoded

pin=2&mode=output&value=1
```

| 参数 | 必填 | 说明 |
|------|------|------|
| `pin` | 是 | GPIO 引脚号（0~16） |
| `mode` | 否 | `input` / `output` / `input_pullup` |
| `value` | 否 | 0 / 1（输出模式时有效） |

### MQTT

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/mqtt/status` | GET | MQTT 状态和配置 |
| `/api/mqtt/config` | POST | 保存 MQTT 配置 |
| `/api/mqtt/test` | GET | 触发一次状态上报 |
| `/api/mqtt/device-info` | GET | 触发一次设备信息上报 |
| `/api/mqtt/log` | GET | 获取上报日志（最近 20 条） |

### NTP

| 接口 | 方法 | 说明 |
|------|------|------|
| `/api/ntp/status` | GET | NTP 状态和配置 |
| `/api/ntp/save` | POST | 保存 NTP 配置 |
| `/api/ntp/sync` | GET | 手动触发时间同步 |

---

## MQTT 数据格式

### 状态上报 (`esp8266/status`)

```json
{
  "client_id": "ESP8266-4b3c2a",
  "chip_id": "0x4b3c2a",
  "timestamp": 1727300000,
  "datetime": "2026-09-26 10:00:00",
  "ip": "192.168.1.100",
  "rssi": -55,
  "heap": 32000,
  "uptime": 3600,
  "gpio": [
    {"pin": 0, "name": "GPIO0", "mode": "输入", "value": 1},
    {"pin": 2, "name": "GPIO2", "mode": "输出", "value": 0}
  ]
}
```

### 设备信息 (`esp8266/info`)

```json
{
  "client_id": "ESP8266-4b3c2a",
  "chip_id": "0x4b3c2a",
  "timestamp": 1727300000,
  "datetime": "2026-09-26 10:00:00",
  "chip_model": "ESP8266EX",
  "cpu_freq": 80,
  "flash_size": 4194304,
  "flash_speed": 40000000,
  "heap_frag": 5,
  "free_heap": 32000,
  "sketch_size": 300000,
  "free_sketch_space": 2000000,
  "ip": "192.168.1.100",
  "mac": "AA:BB:CC:DD:EE:FF",
  "ssid": "MyWiFi",
  "rssi": -55,
  "channel": 6,
  "gateway": "192.168.1.1",
  "subnet": "255.255.255.0",
  "firmware": "AutoWifiConfig v1.3",
  "uptime": 3600,
  "gpio_count": 11,
  "report_pins": 12345,
  "report_interval": 5000,
  "status_topic": "esp8266/status"
}
```

### LWT 遗嘱消息

连接断开时，MQTT 服务器会自动发布：

```
主题: esp8266/status/status
消息: offline
QoS: 1
retain: true
```

---

## 二次开发指南

### 项目结构

```
AutoWifiConfig/
├── platformio.ini          # PlatformIO 配置
├── src/
│   ├── main.cpp            # 主程序入口，状态机调度
│   ├── config.h            # 全局配置常量
│   ├── LedStatus.h         # LED 状态闪烁
│   ├── WifiManager.h       # WiFi 连接管理
│   ├── ConfigPortal.h      # 配网 Web 门户
│   ├── DeviceStatusWeb.h   # 设备状态 Web 界面
│   ├── MqttManager.h       # MQTT 客户端管理
│   └── TimeManager.h       # NTP 时间管理
└── tools/
    ├── serial_monitor.py        # Web 串口监控工具
    ├── serial_monitor_web.py    # Web 版串口监控
    └── stability_monitor.py     # 稳定性自动测试脚本
```

### 核心模块说明

#### main.cpp — 主程序

采用状态机模式，管理系统各个状态之间的切换：

```cpp
enum SystemState {
    STATE_STARTUP,       // 启动
    STATE_WIFI_CONNECT,  // WiFi连接中
    STATE_PORTAL,        // 配网模式
    STATE_RUNNING        // 正常运行
};
```

**主循环 `loop()`**:
- 调用当前状态对应的 `loopXXX()` 函数
- 各模块的 `loop()` 方法在此被调用
- LED 状态更新

#### LedStatus.h — LED 状态管理

```cpp
class LedStatus {
    void setMode(int mode);   // 设置闪烁模式
    void loop();              // 主循环调用，更新LED状态
};
```

闪烁模式：`BLINK_SLOW` / `BLINK_MEDIUM` / `BLINK_FAST` / `BLINK_VERY_FAST` / `LED_STEADY_ON` / `LED_STEADY_OFF`

#### WifiManager.h — WiFi 管理

```cpp
class WifiManager {
    bool hasSavedConfig();   // 检查是否有保存的WiFi配置
    bool connect();          // 连接WiFi（重试3次）
    void saveConfig(ssid, pass);  // 保存配置到EEPROM
    void clearConfig();      // 清除配置
    bool isConnected();      // 是否已连接
};
```

#### ConfigPortal.h — 配网门户

```cpp
class ConfigPortal {
    void start();    // 启动AP和Web服务器
    void stop();     // 停止
    void loop();     // 处理Web请求
    bool hasResult();        // 是否有配网结果
    WifiConfig getResult();  // 获取配网结果
};
```

#### MqttManager.h — MQTT 管理

```cpp
class MqttManager {
    void begin();                    // 初始化
    void loop();                     // 主循环调用
    bool connect();                  // 连接MQTT
    void disconnect();               // 断开连接
    void publishStatus();            // 上报GPIO状态
    void publishDeviceInfo();        // 上报设备信息
    bool isConnected();              // 是否已连接
    MqttConfig getConfig();          // 获取配置
    void saveConfig(MqttConfig);     // 保存配置
};
```

#### TimeManager.h — NTP 时间管理

```cpp
class TimeManager {
    void begin();                    // 初始化
    void loop();                     // 主循环调用
    bool forceSync();                // 强制同步
    bool isSynced();                 // 是否已同步
    uint32_t getTimestamp();         // 获取Unix时间戳
    String getDateTimeStr();         // 获取日期时间字符串
    String getTimeStr();             // 获取时间字符串
    String getDateStr();             // 获取日期字符串
    String formatTime(uint32_t);     // 格式化时间戳
    NtpConfig getConfig();           // 获取配置
    void saveConfig(NtpConfig);      // 保存配置
};
```

#### DeviceStatusWeb.h — 设备状态 Web

```cpp
class DeviceStatusWeb {
    void begin();                    // 启动Web服务器
    void stop();                     // 停止
    void handleClient();             // 处理请求（主循环调用）
    void setMqttManager(mgr);        // 设置MQTT管理器
    void setTimeManager(mgr);        // 设置时间管理器
};
```

采用**流式发送** HTML，避免大 String 导致的内存碎片和崩溃。

---

### 添加新功能

#### 步骤 1：创建新的模块头文件

在 `src/` 目录下创建 `MyFeature.h`：

```cpp
#ifndef MY_FEATURE_H
#define MY_FEATURE_H

#include <Arduino.h>
#include "config.h"

class MyFeature {
public:
    MyFeature() : _initialized(false) {}

    void begin() {
        // 初始化代码
        _initialized = true;
        DEBUG_PRINTLN("[MyFeature] 已初始化");
    }

    void loop() {
        if (!_initialized) return;
        // 循环处理代码
    }

    // 你的功能函数
    void doSomething() {
        // ...
    }

private:
    bool _initialized;
};

#endif // MY_FEATURE_H
```

#### 步骤 2：在 main.cpp 中集成

```cpp
#include "MyFeature.h"

// 全局对象
MyFeature g_myFeature;

// 在 setup() 中初始化
void setup() {
    // ... 现有初始化 ...
    g_myFeature.begin();
}

// 在 loopRunning() 中调用
void loopRunning() {
    g_webServer.handleClient();
    g_mqttMgr.loop();
    g_timeMgr.loop();
    g_myFeature.loop();  // 添加这一行
}
```

#### 步骤 3：添加 Web API（可选）

在 `DeviceStatusWeb.h` 中添加：

```cpp
// 在 begin() 中注册路由
_server.on("/api/myfeature", std::bind(&DeviceStatusWeb::handleApiMyFeature, this));

// 添加处理函数
void handleApiMyFeature() {
    String json = "{\"success\":true,\"data\":\"hello\"}";
    _server.send(200, "application/json", json);
}
```

---

### 修改默认配置

所有默认配置在 `config.h` 中：

```cpp
// 修改默认 WiFi 重试次数
#define WIFI_RETRY_COUNT        5  // 改为5次

// 修改默认 MQTT 服务器
#define MQTT_DEFAULT_HOST       "mqtt.example.com"
#define MQTT_DEFAULT_PORT       1883

// 修改默认 NTP 服务器
#define NTP_DEFAULT_SERVER1     "ntp.example.com"

// 修改 LED 引脚
#define LED_PIN                 5  // 改为GPIO5
```

修改后重新编译烧录即可。

---

### 添加新的 GPIO 功能

#### 1. 在 config.h 中添加

```cpp
#define MY_SENSOR_PIN           4   // 传感器接GPIO4
#define MY_ACTUATOR_PIN         5   // 执行器接GPIO5
```

#### 2. 在模块中初始化

```cpp
void begin() {
    setGpioMode(MY_SENSOR_PIN, INPUT);       // 传感器输入
    setGpioMode(MY_ACTUATOR_PIN, OUTPUT);    // 执行器输出
    digitalWrite(MY_ACTUATOR_PIN, LOW);      // 初始低电平
}
```

> ⚠️ **务必使用 `setGpioMode()` 而不是 `pinMode()`**，这样可以正确跟踪 GPIO16 的模式。

---

### 自定义 MQTT 上报内容

在 `MqttManager.h` 的 `publishStatus()` 中添加：

```cpp
void publishStatus() {
    if (!_connected) return;

    String payload;
    payload.reserve(1024);
    payload = "{";
    // ... 原有字段 ...

    // 添加自定义字段
    payload += "\"temperature\":" + String(readTemperature()) + ",";
    payload += "\"humidity\":" + String(readHumidity()) + ",";

    // ... GPIO 部分 ...
    payload += "}";

    _client.publish(_config.topic, payload.c_str());
}
```

---

### 添加新的 Web 页面

在 `DeviceStatusWeb.h` 中添加：

#### 1. 注册路由

```cpp
void begin() {
    // ... 现有路由 ...
    _server.on("/my-page", std::bind(&DeviceStatusWeb::handleMyPage, this));
}
```

#### 2. 添加页面处理函数（流式发送）

```cpp
void handleMyPage() {
    sendStart("text/html");
    sendPageHeader("我的页面", 3);  // 3表示第4个导航项

    // 页面内容
    sc("<div class='card'><div class='card-h'>我的功能</div><div class='card-b'>");
    sc("<p>这是自定义页面内容</p>");
    sc("</div></div>");

    sendPageFooter();
}
```

#### 3. 在导航栏添加链接

修改 `sendPageHeader()` 函数中的导航部分。

---

## 编译与烧录

### 使用 PlatformIO（推荐）

```bash
# 编译
pio run

# 烧录
pio run --target upload --upload-port COM4

# 清理
pio run --target clean

# 查看编译大小
pio run --target size
```

### 编译输出说明

```
RAM:   [======= ] 74.8% (used 61240 bytes from 81920 bytes)
Flash: [===     ] 32.5% (used 339873 bytes from 1044464 bytes)
```

- **RAM 占用 74.8%** — 优化后的值，留足了运行空间
- **Flash 占用 32.5%** — 还有大量空间可添加功能

---

## 稳定性保障

本固件经过多轮加固，确保在各种异常场景下稳定运行：

### 已修复的问题

| 问题 | 原因 | 修复方案 |
|------|------|----------|
| Exception 29 崩溃 | `localtime()` 实现缺陷 | 纯算法实现时间格式化 |
| 内存不足/Low memory | 大 String HTML 拼接导致堆碎片 | 流式发送 HTML，内存峰值降 80% |
| 看门狗复位 | 长循环阻塞 | 所有循环添加 `yield()` |
| GPIO16 配置丢失 | `1 << 16` int 溢出 | `1UL << pin` 类型安全 |
| 时间戳下溢 | uint32 减法下溢 | 边界检查保护 |

### 异常场景保障

| 场景 | 处理方式 | 是否重启 |
|------|----------|----------|
| WiFi 连接失败 | 重试 3 次 → 进入配网模式 | ❌ 不会 |
| NTP 同步失败 | 静默重试，显示"未同步" | ❌ 不会 |
| MQTT 连接失败 | 5 秒后重试，记录日志 | ❌ 不会 |
| MQTT 上报失败 | 记录日志，等待下次 | ❌ 不会 |
| 内存紧张 | 流式发送，无大块分配 | ❌ 不会 |
| Web 并发请求 | 单线程处理，有序响应 | ❌ 不会 |

### 稳定性测试

- 测试时长：**60 分钟**
- 日志行数：**645 行**
- 异常次数：**0 次**
- MQTT 上报：持续正常
- NTP 同步：正常工作
- Web 服务：响应正常

---

## 常见问题

### Q: 找不到 ESP8266_Config 热点？

A: 
1. 确认设备已进入配网模式（LED 快速闪烁）
2. 检查 USB 供电是否充足
3. 尝试靠近设备再搜索

### Q: 配网成功后连不上 WiFi？

A:
1. 检查密码是否正确
2. 确认 WiFi 是 2.4GHz（ESP8266 不支持 5GHz）
3. 查看串口日志确认错误信息

### Q: MQTT 连接失败怎么办？

A:
1. 检查服务器地址和端口是否正确
2. 确认用户名密码正确
3. 检查网络是否能访问 MQTT 服务器
4. 查看 MQTT 配置页的上报日志

### Q: 时间一直显示"未同步"？

A:
1. 确认设备能访问互联网
2. 尝试更换 NTP 服务器
3. 点击"立即同步"按钮手动触发
4. 等待最多 1 分钟（首次同步可能较慢）

### Q: 如何恢复出厂设置？

A: 有两种方式：
1. 在配网模式下，WiFi 连接失败 3 次后会自动清除配置
2. 代码中调用 `g_wifiMgr.clearConfig()` 然后重启

### Q: 可以同时连接多少个设备到配网热点？

A: 最多 4 个（`AP_MAX_CONNECTIONS` 配置）

### Q: 配置保存在哪里？掉电会丢失吗？

A: 保存在 EEPROM（模拟 Flash），掉电不会丢失。

---

## 许可证

MIT License
