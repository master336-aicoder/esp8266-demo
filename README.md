# ESP8266 IoT 开发项目

ESP8266 物联网固件开发项目，包含自动配网、MQTT 上报、NTP 时间同步、Web 控制等完整功能。

> 本仓库是一个完整的 ESP8266 开发工程，可直接用于产品开发或学习参考。

---

## 目录

- [项目结构](#项目结构)
- [开发环境搭建](#开发环境搭建)
  - [1. USB 驱动安装](#1-usb-驱动安装)
  - [2. PlatformIO 安装（推荐）](#2-platformio-安装推荐)
  - [3. Arduino CLI 安装（备选）](#3-arduino-cli-安装备选)
  - [4. 验证开发环境](#4-验证开发环境)
- [快速上手](#快速上手)
  - [编译固件](#编译固件)
  - [烧录固件](#烧录固件)
  - [查看串口输出](#查看串口输出)
- [固件功能文档](#固件功能文档)
- [二次开发详细指南](#二次开发详细指南)
  - [开发流程](#开发流程)
  - [代码架构](#代码架构)
  - [添加新功能：完整示例](#添加新功能完整示例)
  - [修改现有功能](#修改现有功能)
  - [调试技巧](#调试技巧)
  - [代码规范](#代码规范)
- [工具使用说明](#工具使用说明)
  - [serial_monitor_web.py — Web 版串口监控](#serial_monitor_webpy--web-版串口监控)
  - [stability_monitor.py — 稳定性自动测试](#stability_monitorpy--稳定性自动测试)
  - [serial_monitor.py — 命令行串口监控](#serial_monitorpy--命令行串口监控)
  - [esptool.py — Flash 工具](#esptoolpy--flash-工具)
- [常见问题](#常见问题)
- [Git 提交规范](#git-提交规范)
- [许可证](#许可证)

---

## 项目结构

```
esp8266/
├── AutoWifiConfig/          # 主固件项目
│   ├── platformio.ini       # PlatformIO 配置文件
│   ├── src/
│   │   ├── main.cpp         # 主程序入口
│   │   ├── config.h         # 全局配置
│   │   ├── LedStatus.h      # LED 状态指示
│   │   ├── WifiManager.h    # WiFi 连接管理
│   │   ├── ConfigPortal.h   # 配网 Web 门户
│   │   ├── DeviceStatusWeb.h # 设备状态 Web 界面
│   │   ├── MqttManager.h    # MQTT 客户端
│   │   └── TimeManager.h    # NTP 时间管理
│   └── README.md            # 固件功能详细文档
├── tools/                   # 开发工具
│   ├── serial_monitor_web.py    # Web 版串口监控
│   ├── serial_monitor.py        # 命令行串口监控
│   ├── stability_monitor.py     # 稳定性自动测试
│   └── arduino-cli/             # Arduino CLI 工具（可选）
├── .gitignore               # Git 忽略文件
├── arduino-cli.yaml         # Arduino CLI 配置
└── README.md                # 本文件（开发者文档）
```

---

## 开发环境搭建

### 1. USB 驱动安装

ESP8266 开发板通常使用 CH340 或 CP2102 USB 转串口芯片。

#### Windows

**CH340 驱动：**
1. 下载：http://www.wch-ic.com/downloads/CH341SER_EXE.html
2. 解压后运行 `CH341SER.EXE` 安装
3. 安装完成后插入开发板，在设备管理器中查看端口号（如 `COM4`）

**CP2102 驱动：**
1. 下载：https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
2. 安装后查看端口号

#### 验证

插入开发板后，在设备管理器的"端口 (COM 和 LPT)"中能看到类似 `USB-SERIAL CH340 (COM4)` 的设备，说明驱动安装成功。

---

### 2. PlatformIO 安装（推荐）

PlatformIO 是一个专业的嵌入式开发平台，支持 ESP8266/ESP32/Arduino 等多种平台。

#### 方式 A：安装 PlatformIO Core（命令行版）

```bash
# 使用 pip 安装（推荐）
pip install -U platformio

# 验证安装
pio --version
```

#### 方式 B：VS Code + PlatformIO 插件

1. 安装 [VS Code](https://code.visualstudio.com/)
2. 在扩展商店搜索 "PlatformIO IDE" 安装
3. 重启 VS Code 即可使用

#### 首次编译

首次编译时 PlatformIO 会自动下载 ESP8266 平台和工具链，需要联网：

```bash
cd AutoWifiConfig
pio run
```

等待下载完成（约 5~10 分钟，取决于网络速度）。

---

### 3. Arduino CLI 安装（备选）

如果你习惯使用 Arduino 框架，可以使用 Arduino CLI。

本仓库已自带 Windows 版 Arduino CLI：

```bash
# 进入工具目录
cd tools/arduino-cli

# 初始化配置
arduino-cli config init --dest-file ../../arduino-cli.yaml

# 添加 ESP8266 开发板地址
arduino-cli core update-index
arduino-cli core install esp8266:esp8266

# 验证
arduino-cli board listall esp8266
```

> 💡 本项目主要使用 PlatformIO，Arduino CLI 作为备选方案。

---

### 4. 验证开发环境

```bash
# 进入项目目录
cd AutoWifiConfig

# 编译（不烧录）
pio run

# 看到类似输出说明成功：
# RAM:   [======= ] 74.8% (used 61240 bytes from 81920 bytes)
# Flash: [===     ] 32.5% (used 339873 bytes from 1044464 bytes)
```

---

## 快速上手

### 编译固件

```bash
cd AutoWifiConfig

# 编译
pio run

# 查看占用大小
pio run --target size

# 清理编译产物
pio run --target clean
```

### 烧录固件

```bash
# 烧录到 COM4（修改为实际端口）
pio run --target upload --upload-port COM4

# 烧录成功后会自动重启设备
```

### 查看串口输出

```bash
# 方式 1：PlatformIO 内置监视器
pio device monitor -b 115200 -p COM4

# 方式 2：Web 版串口监控（推荐，有UI）
python ../tools/serial_monitor_web.py
# 然后浏览器访问 http://localhost:5000
```

---

## 固件功能文档

👉 详细的固件功能说明请查看 **[AutoWifiConfig/README.md](AutoWifiConfig/README.md)**

包含：
- 自动配网 / Web 界面 / MQTT / NTP / GPIO 控制等功能详解
- API 接口文档
- MQTT 数据格式
- EEPROM 地址映射
- 稳定性保障说明

---

## 二次开发详细指南

### 开发流程

标准的二次开发流程：

```
1. 理解需求
   ↓
2. 确定修改哪个模块（新建/修改）
   ↓
3. 编写代码
   ↓
4. 编译 pio run
   ↓
5. 烧录测试 pio run --target upload
   ↓
6. 串口调试 / 功能验证
   ↓
7. 稳定性测试（运行至少30分钟无异常）
   ↓
8. Git 提交
```

---

### 代码架构

采用**模块化设计**，每个功能独立成一个 `.h` 头文件（Header-only 模式），便于维护和扩展。

#### 模块依赖关系

```
main.cpp (状态机调度)
  ├─ LedStatus.h       (LED状态，无依赖)
  ├─ WifiManager.h     (WiFi管理，依赖EEPROM)
  ├─ ConfigPortal.h    (配网门户，依赖WifiManager+WebServer)
  ├─ TimeManager.h     (NTP时间，依赖WiFi)
  ├─ MqttManager.h     (MQTT客户端，依赖PubSubClient+WiFi)
  └─ DeviceStatusWeb.h (Web界面，依赖所有模块)
```

#### 状态机

系统有 4 个核心状态，在 `main.cpp` 中定义：

```cpp
enum SystemState {
    STATE_STARTUP,       // 启动初始化
    STATE_WIFI_CONNECT,  // WiFi连接中
    STATE_PORTAL,        // 配网模式
    STATE_RUNNING        // 正常运行
};
```

每个状态对应一个 `loopXXX()` 函数，主循环调用当前状态的处理函数。

---

### 添加新功能：完整示例

下面以**添加一个温湿度传感器（DHT11）功能**为例，演示完整的二次开发过程。

#### 步骤 1：添加依赖库

编辑 `AutoWifiConfig/platformio.ini`：

```ini
lib_deps =
    knolleary/PubSubClient @ ^2.8
    adafruit/DHT sensor library @ ^1.4   # 新增
    adafruit/Adafruit Unified Sensor @ ^1.1  # 新增
```

#### 步骤 2：创建新模块

在 `AutoWifiConfig/src/` 下创建 `DhtSensor.h`：

```cpp
#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

#include <Arduino.h>
#include <DHT.h>
#include "config.h"

// 配置
#define DHT_PIN     4   // 传感器接GPIO4 (D2)
#define DHT_TYPE    DHT11

class DhtSensor {
public:
    DhtSensor() : _dht(DHT_PIN, DHT_TYPE), _temperature(0), _humidity(0),
                  _lastRead(0), _readInterval(2000), _valid(false) {}

    void begin() {
        _dht.begin();
        DEBUG_PRINTLN("[DHT] 温湿度传感器初始化完成");
    }

    void loop() {
        unsigned long now = millis();
        if (now - _lastRead < _readInterval) return;
        _lastRead = now;

        float t = _dht.readTemperature();
        float h = _dht.readHumidity();

        if (!isnan(t) && !isnan(h)) {
            _temperature = t;
            _humidity = h;
            _valid = true;
        } else {
            DEBUG_PRINTLN("[DHT] 读取失败");
        }
    }

    float getTemperature() { return _temperature; }
    float getHumidity() { return _humidity; }
    bool isValid() { return _valid; }

private:
    DHT _dht;
    float _temperature;
    float _humidity;
    unsigned long _lastRead;
    unsigned long _readInterval;
    bool _valid;
};

#endif // DHT_SENSOR_H
```

#### 步骤 3：在主程序中集成

编辑 `AutoWifiConfig/src/main.cpp`：

```cpp
// 1. 添加头文件
#include "DhtSensor.h"

// 2. 创建全局对象（在其他全局对象后面）
DhtSensor g_dht;

// 3. 在 setupRunning() 中初始化
void setupRunning() {
    // ... 现有代码 ...
    g_timeMgr.begin();
    g_mqttMgr.begin();
    g_dht.begin();  // 新增
    // ...
}

// 4. 在 loopRunning() 中调用 loop()
void loopRunning() {
    g_webServer.handleClient();
    g_mqttMgr.loop();
    g_timeMgr.loop();
    g_dht.loop();   // 新增
    g_led.loop();
}
```

#### 步骤 4：添加 Web API

在 `DeviceStatusWeb.h` 中添加温湿度 API：

```cpp
// 在类中声明指针（private 区域）
DhtSensor* _dhtSensor;

// 添加 set 方法
void setDhtSensor(DhtSensor* dht) { _dhtSensor = dht; }

// 在 begin() 中注册路由
_server.on("/api/dht", std::bind(&DeviceStatusWeb::handleApiDht, this));

// 添加处理函数
void handleApiDht() {
    if (!_dhtSensor) {
        _server.send(500, "application/json", "{\"error\":\"not initialized\"}");
        return;
    }
    String json = "{";
    json += "\"valid\":" + String(_dhtSensor->isValid() ? "true" : "false") + ",";
    json += "\"temperature\":" + String(_dhtSensor->getTemperature(), 1) + ",";
    json += "\"humidity\":" + String(_dhtSensor->getHumidity(), 0);
    json += "}";
    _server.send(200, "application/json", json);
}
```

然后在 `main.cpp` 中关联：

```cpp
g_webServer.setDhtSensor(&g_dht);
```

#### 步骤 5：在 MQTT 上报中添加温湿度

在 `MqttManager.h` 的 `publishStatus()` 中添加：

```cpp
// 需要先设置 DhtSensor 指针
DhtSensor* _dhtSensor;
void setDhtSensor(DhtSensor* dht) { _dhtSensor = dht; }

// 在 publishStatus() 中添加字段
if (_dhtSensor && _dhtSensor->isValid()) {
    payload += "\"temperature\":" + String(_dhtSensor->getTemperature(), 1) + ",";
    payload += "\"humidity\":" + String(_dhtSensor->getHumidity(), 0) + ",";
}
```

#### 步骤 6：编译测试

```bash
cd AutoWifiConfig
pio run
pio run --target upload --upload-port COM4
```

访问 `http://设备IP/api/dht` 验证数据。

---

### 修改现有功能

#### 修改默认配置

所有可配置的常量都在 `config.h` 中，修改后重新编译即可：

```cpp
// 修改配网热点名称
#define AP_SSID  "MyDevice_Config"

// 修改默认MQTT服务器
#define MQTT_DEFAULT_HOST  "mqtt.mycompany.com"
#define MQTT_DEFAULT_PORT  1883

// 修改LED引脚
#define LED_PIN  5
```

#### 修改上报间隔

在 `config.h` 中修改：

```cpp
#define MQTT_DEFAULT_INTERVAL   10000   // 改为10秒
```

> 💡 也可以在 Web 界面的 MQTT 配置页中动态修改，无需重新编译。

#### 添加自定义上报字段

在 `MqttManager.h` 的 `publishStatus()` 函数中，`payload` 组装部分添加你的字段即可。注意使用 `reserve()` 预分配足够空间。

#### 修改 Web 页面样式

所有 CSS 在 `DeviceStatusWeb.h` 的 `sendPageHeader()` 函数中。修改后重新编译即可。

---

### 调试技巧

#### 1. 串口调试输出

使用 `DEBUG_PRINT` / `DEBUG_PRINTLN` / `DEBUG_PRINTF` 宏输出调试信息：

```cpp
DEBUG_PRINTLN("[MyFeature] 初始化完成");
DEBUG_PRINTF("[MyFeature] 数值: %d, 字符串: %s\r\n", value, str.c_str());
```

这些宏在 `config.h` 中定义，可通过 `ENABLE_DEBUG` 开关控制：

```cpp
#define ENABLE_DEBUG    false   // 禁用调试输出，节省Flash和RAM
```

#### 2. Web 页面调试

如果 Web 页面显示异常：
- 打开浏览器 F12 开发者工具
- 查看 Console 是否有 JS 报错
- 查看 Network 标签确认 API 响应

#### 3. 内存问题定位

```cpp
// 在代码中打印可用内存
DEBUG_PRINTF("[调试] 可用堆: %d 字节\r\n", ESP.getFreeHeap());
DEBUG_PRINTF("[调试] 堆碎片: %d%%\r\n", ESP.getHeapFragmentation());
```

如果可用内存持续下降，说明有内存泄漏。常见原因：
- String 频繁拼接导致碎片 → 改用 `reserve()` 预分配
- 循环内创建大对象 → 移到循环外
- 忘记释放资源 → 检查是否有 new 没有 delete

#### 4. 崩溃调试（Exception 解码）

如果发生 Exception 崩溃：

```
Exception (29):
epc1=0x4000df64 epc2=0x00000000 ...
>>>stack>>>
...
<<<stack<<<
```

可以使用 `pio device monitor` 的 Exception Decoder 解码（需要烧录时保留 elf 文件）。

#### 5. 看门狗复位

ESP8266 有软件看门狗和硬件看门狗。如果 loop() 中执行时间过长（>1秒），会触发看门狗复位。

**解决方案：**
- 长循环中添加 `yield()`
- 耗时操作分帧处理
- 避免阻塞式等待

```cpp
// 错误示例：长循环可能触发看门狗
for (int i = 0; i < 100000; i++) {
    // 耗时操作
}

// 正确示例：循环中添加 yield()
for (int i = 0; i < 100000; i++) {
    // 耗时操作
    if (i % 1000 == 0) yield();
}
```

---

### 代码规范

为了保持代码可维护性，请遵循以下规范：

#### 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 类名 | 大驼峰 (PascalCase) | `WifiManager`, `MqttConfig` |
| 函数名 | 小驼峰 (camelCase) | `getStatus()`, `publishStatus()` |
| 全局变量 | `g_` 前缀 + 小驼峰 | `g_mqttMgr`, `g_led` |
| 成员变量 | `_` 前缀 + 小驼峰 | `_connected`, `_config` |
| 常量/宏 | 全大写 + 下划线 | `MQTT_DEFAULT_PORT`, `LED_PIN` |
| 结构体成员 | 小驼峰 | `cfg.host`, `cfg.port` |

#### 文件结构

每个模块一个 `.h` 文件，结构如下：

```cpp
#ifndef MODULE_NAME_H    // 头文件保护
#define MODULE_NAME_H

#include <Arduino.h>    // 基础头文件
#include "config.h"     // 配置文件

class ModuleName {
public:
    ModuleName();      // 构造函数
    void begin();      // 初始化
    void loop();       // 主循环调用
    // ... 公共方法 ...

private:
    // ... 私有变量和方法 ...
};

#endif // MODULE_NAME_H
```

#### 注释规范

- 类、公共函数使用 Doxygen 风格注释
- 复杂逻辑添加行内注释
- 调试代码用 `DEBUG_PRINT` 输出

---

## 工具使用说明

### serial_monitor_web.py — Web 版串口监控

一个基于 Web 的串口监控工具，适合在浏览器中查看设备日志。

```bash
# 启动
python tools/serial_monitor_web.py

# 浏览器访问
# http://localhost:5000
```

**功能：**
- 实时显示串口输出
- 支持自动滚动
- 时间戳显示
- 暂停/继续滚动

**配置：**
- 默认端口：`COM4`
- 波特率：`115200`
- Web 端口：`5000`

如需修改，编辑脚本顶部的配置变量。

---

### stability_monitor.py — 稳定性自动测试

自动化稳定性测试工具，持续监控串口，检测崩溃/重启/异常。

```bash
# 启动60分钟稳定性测试
python tools/stability_monitor.py
```

**功能：**
- 自动监控指定时长（默认 60 分钟）
- 检测异常关键词（Exception、WDT reset、crash 等）
- 异常时自动捕获堆栈信息
- 结果写入状态文件
- 统计日志行数和异常次数

**输出文件：**
- `tools/monitor_log.txt` — 完整串口日志
- `tools/monitor_status.txt` — 测试状态（running/completed/error）
- `tools/monitor_errors.txt` — 异常详情

**状态文件格式：**
```
status=running
errors=0
elapsed_min=30
lines=233
updated=2026-09-26 10:30:00
```

**修改监控时长：**
编辑脚本顶部的 `DURATION_MINUTES` 变量。

---

### serial_monitor.py — 命令行串口监控

轻量级命令行串口监控工具。

```bash
# 启动
python tools/serial_monitor.py

# 按 Ctrl+C 停止
```

**功能：**
- 实时显示串口输出
- 异常关键词检测
- 自动重连串口
- 日志写入文件

---

### esptool.py — Flash 工具

用于底层 Flash 操作（擦除、读取、备份等）。

```bash
# 安装
pip install esptool

# 读取芯片信息
esptool.py --port COM4 chip_id

# 读取 MAC 地址
esptool.py --port COM4 read_mac

# 擦除 Flash（恢复出厂）
esptool.py --port COM4 erase_flash

# 备份固件
esptool.py --port COM4 read_flash 0 0x400000 backup.bin

# 烧录固件（一般用 pio run --target upload 即可）
esptool.py --port COM4 write_flash 0x00000 firmware.bin
```

---

## 常见问题

### Q: 编译时提示找不到 PubSubClient 库？

A: 首次编译会自动下载，如失败检查网络连接。也可以手动安装：

```bash
pio lib install knolleary/PubSubClient
```

### Q: 烧录失败，提示 "Timed out waiting for packet header"？

A: 
1. 检查串口号是否正确
2. 按住开发板的 FLASH/BOOT 按钮再烧录
3. 降低烧录波特率：`--upload-speed 9600`
4. 检查 USB 线是否是数据线（不是充电线）

### Q: 如何修改 CPU 频率？

A: 编辑 `platformio.ini`，添加：
```ini
build_flags =
    -D F_CPU=80000000L   ; 80MHz (默认)
    ; -D F_CPU=160000000L ; 160MHz 超频
```

### Q: 如何减少固件体积？

A:
1. 关闭调试输出：`#define ENABLE_DEBUG false`
2. 移除不需要的功能模块
3. 使用 `PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY`（低内存版LWIP）

### Q: 如何增加可用内存？

A:
1. 减少 `DEBUG_PRINTF` 的使用（字符串占用 RAM）
2. 大数组改用 `PROGMEM` 存储在 Flash
3. 减少 String 使用，改用固定大小 char 数组
4. 关闭不需要的功能

### Q: MQTT 最多能上报多大的消息？

A: PubSubClient 默认缓冲区是 256 字节，本项目已设置为 1024 字节。如需更大，在 `MqttManager.h` 中修改：

```cpp
_client.setBufferSize(2048); // 改为2048字节
```

### Q: 可以同时运行多少个 Web 连接？

A: ESP8266WebServer 是单线程的，一次处理一个请求。一般场景下足够使用。

---

## Git 提交规范

提交信息格式：

```
<类型>: <简短描述>

<详细说明（可选）>
```

**类型：**
- `feat` — 新功能
- `fix` — 修复问题
- `docs` — 文档更新
- `refactor` — 代码重构
- `perf` — 性能优化
- `chore` — 构建/工具/配置变更

**示例：**
```
feat: 增加温湿度传感器支持

- 添加 DHT11/DHT22 支持
- 在 MQTT 上报中增加温湿度字段
- 添加 /api/dht 接口
```

---

## 许可证

MIT License

---

# 关于作者

> 源码猿（master336）—— 一个把「踩过的坑」写成「别人脚下的路」的技术作者。

## 一句话介绍

**源码猿（master336）**，现就职于**某公司**，担任**高级技术架构师 / 产品售前专家**（腾讯云开发者社区认证）。长期深耕 Java 后端与分布式架构，近年将主要精力投向 **AI Agent、大模型私有化部署与工程化落地**，坚持用「能跑通、可复现、讲原理」的标准写每一篇文章。

## 身份与平台

| 平台 | 主页 | 身份 / 荣誉 |
| --- | --- | --- |
| CSDN 博客 | [master336.blog.csdn.net](https://master336.blog.csdn.net) | 人工智能领域**新星创作者**，原力等级 **298**，累计获赞 **461** 次、评论 **86** 条 |
| 腾讯云开发者社区 | [cloud.tencent.com/developer/user/1558800](https://cloud.tencent.com/developer/user/1558800) | 认证作者 · 某公司｜高级技术架构师、产品售前专家 |

## 技术视野

作者的写作版图横跨「从底层到应用、从传统工程到 AI 原生」的完整链路：

- **后端与架构**：Java、Spring / SpringBoot / SpringCloud、微服务架构、Maven
- **数据与存储**：MySQL、Redis、PostgreSQL、Greenplum、GaussDB、ETL 与大数据处理
- **运维与基础设施**：Linux、Nginx、操作系统、网络协议（TCP/UDP、HTTP/HTTPS、OSI 七层模型）、RabbitMQ
- **前端与客户端**：JavaScript、CSS、Vue.js、HTML、Android、微信小程序
- **AI 与智能化**：机器学习、NLP、图像处理、数据分析、AIGC
- **AI 工程化（近期主线）**：AI Agent 架构与多智能体协同、Ollama / ComfyUI 私有大模型部署、MCP 协议、Skill 资产化、大模型评测与上下文召回

## 写作风格

不同于「Hello World 式」的速食教程，作者的文章有三个鲜明特点：

1. **讲透原理** —— 不满足于告诉你「怎么做」，更在意解释「为什么这么做」；
2. **给出可落地的工程** —— 完整目录结构、核心代码、硬件选型、避坑清单，拿来就能改；
3. **有数据、有出处** —— 明确区分哪些是实测、哪些是预测、哪些不可横向比较，拒绝拍脑袋结论。

近期在腾讯云开发者社区连续产出《用 CodeArts 把一个自托管云相册真正做完》《Ollama 私有大模型部署与生态配置完全指南》《从零构建 AI Agent：一份写给工程师的深度开发指南》《一次讲透 HTTP 与 HTTPS》等长文，几乎保持日更节奏。

## 联系方式

- CSDN：[https://master336.blog.csdn.net](https://master336.blog.csdn.net)
- 腾讯云开发者社区：[https://cloud.tencent.com/developer/user/1558800](https://cloud.tencent.com/developer/user/1558800)

---

## ☕ 支持作者

看到这里，说明你已经陪我读到了文章的最后。

如果这些文字曾经帮你**少踩一个坑、少熬一个夜、少走一次弯路**，那它就是有价值的。写作这件事，靠的不只是兴趣，还有一台够用的机器、几个付费的实验环境、以及愿意花在查证与实测上的时间。

**你的每一份支持，都会变成下一篇文章里的一个真实实验、一次更深入的拆解。**

> 不用有压力 —— 一杯咖啡的钱，就足够让我开心很久。
> 当然，一个点赞、一条评论、一次转发，同样是最好的鼓励。

### 打赏渠道

<div align="center">

| 微信支付 | 支付宝 |
| :---: | :---: |
| ![微信支付二维码](images/wx.png) | ![支付宝二维码](images/zfb.jpg) |
| *推荐使用微信支付* | *支持信用卡 / 花呗付款* |

</div>

### 不打烊的支持方式

如果不方便打赏，下面这些同样珍贵：

- 👍 **点赞** —— 让更多人看到这篇内容
- ⭐ **收藏** —— 下次要用时能第一时间找到
- 💬 **评论** —— 你的问题常常是下一篇选题的起点
- 🔗 **转发** —— 把有用的东西分享给需要的朋友

感谢每一位读到这里的人。我们下篇文章见。

---

*Made with ❤️ by 源码猿（master336）*
