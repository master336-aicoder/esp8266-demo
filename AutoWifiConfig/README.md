# ESP8266 自动配网程序

## 功能特性

- 自动读取已保存的 WiFi 配置并连接
- 连接失败自动重试（默认3次）
- 无配置或连接失败时自动进入配网模式
- 精美的 Web 配网页面，支持扫描 WiFi 列表
- LED 状态指示（不同闪烁频率表示不同状态）
- WiFi 凭证保存在 EEPROM 中，掉电不丢失
- 连接断开自动重连
- 完整的串口调试输出

## 硬件连接

- **ESP12 开发板** 通过 USB 连接到电脑
- **串口号**: COM3（根据实际情况调整）
- **LED引脚**: GPIO2（板载LED）

## LED 状态说明

| 状态 | 闪烁频率 | 说明 |
|------|----------|------|
| 常灭 | - | 空闲/初始化 |
| 中闪 | 500ms | 正在连接 WiFi |
| 慢闪 | 2000ms | WiFi 已连接（正常运行） |
| 快闪 | 200ms | 配网模式 |
| 超快闪 | 100ms | 连接失败/错误 |

## 配网流程

```
启动
  │
  ├─► 读取EEPROM中的WiFi配置
  │     │
  │     ├─ 有配置 ──► 尝试连接WiFi（重试3次）
  │     │               │
  │     │               ├─ 成功 ──► 正常运行模式
  │     │               │           （慢闪，每60秒输出状态）
  │     │               │
  │     │               └─ 失败 ──► 进入配网模式
  │     │
  │     └─ 无配置 ──► 进入配网模式
  │
  └─► 配网模式
        │
        ├─ 开启AP: "ESP8266_Config"
        ├─ 用户连接AP后访问 http://192.168.4.1
        ├─ 选择WiFi并输入密码
        ├─ 尝试连接
        │     │
        │     ├─ 成功 ──► 保存配置并进入正常运行
        │     │
        │     └─ 失败 ──► 返回配网模式
        │
        └─ LED快闪提示
```

## 编译和烧录

### 方法一：使用 Arduino IDE（推荐）

1. **安装 Arduino IDE**
   - 下载地址: https://www.arduino.cc/en/software

2. **安装 ESP8266 开发板支持**
   - 打开 Arduino IDE
   - 文件 → 首选项
   - 在"附加开发板管理器网址"中添加：
     ```
     http://arduino.esp8266.com/stable/package_esp8266com_index.json
     ```
   - 工具 → 开发板 → 开发板管理器
   - 搜索 "esp8266" 并安装

3. **打开项目**
   - 文件 → 打开 → 选择 `AutoWifiConfig.ino`

4. **配置开发板**
   - 工具 → 开发板 → ESP8266 Boards → Generic ESP8266 Module
   - 工具 → Flash Size → 4MB (FS:2MB OTA:~1019KB)
   - 工具 → CPU Frequency → 80 MHz
   - 工具 → Upload Speed → 115200
   - 工具 → 端口 → COM3（根据实际情况选择）

5. **上传程序**
   - 点击 "上传" 按钮（→）
   - 等待上传完成

### 方法二：使用 esptool.py 烧录（已安装）

如果你已经有编译好的 bin 文件，可以使用 esptool 烧录：

```bash
# 擦除Flash（可选，首次烧录或清除配置时使用）
python -m esptool --port COM3 erase_flash

# 烧录固件
python -m esptool --port COM3 write_flash 0x00000 firmware.bin
```

## 使用方法

### 首次使用（配网）

1. 给 ESP8266 上电
2. 观察 LED 快速闪烁，表示进入配网模式
3. 用手机或电脑连接 WiFi 热点：**ESP8266_Config**
4. 打开浏览器访问：**http://192.168.4.1**
5. 点击"扫描WiFi网络"按钮
6. 选择要连接的 WiFi，输入密码
7. 点击"连接WiFi"按钮
8. 等待连接成功（页面会显示成功提示和IP地址）
9. LED 变为慢闪，表示连接成功

### 正常使用

- 上电后自动连接已保存的 WiFi
- LED 慢闪表示正常运行
- 串口每60秒输出一次运行状态

### 清除配置重新配网

如果需要重新配置 WiFi，有两种方法：

1. **代码方式**: 在程序中调用 `wifiMgr.clearCredentials()` 然后重启
2. **擦除Flash**: 使用 esptool 擦除整个 Flash
   ```bash
   python -m esptool --port COM3 erase_flash
   ```

## 文件结构

```
AutoWifiConfig/
├── AutoWifiConfig.ino    # 主程序（状态机控制）
├── config.h              # 配置文件（引脚、超时、LED频率等）
├── LedStatus.h           # LED状态指示模块
├── WifiManager.h         # WiFi管理模块（连接、存储、读取）
└── ConfigPortal.h        # 配网门户模块（Web服务器+配网页面）
```

## 自定义配置

所有配置参数都在 `config.h` 文件中，可以根据需要修改：

```cpp
// WiFi配置
#define WIFI_CONNECT_TIMEOUT    10000   // 单次连接超时（毫秒）
#define WIFI_RETRY_COUNT        3       // 连接失败重试次数

// 配网AP配置
#define AP_SSID                 "ESP8266_Config"   // 配网热点名称
#define AP_PASSWORD             ""                  // 配网热点密码

// LED闪烁配置
#define BLINK_SLOW              2000    // 慢闪（已连接）
#define BLINK_MEDIUM            500     // 中闪（连接中）
#define BLINK_FAST              200     // 快闪（配网模式）
#define BLINK_VERY_FAST         100     // 超快闪（失败/错误）
```

## 添加业务代码

在 `AutoWifiConfig.ino` 的 `handleStateWifiConnected()` 函数中添加你的业务逻辑：

```cpp
void handleStateWifiConnected() {
    // ... 现有代码 ...

    // ========== 在这里添加你的业务代码 ==========
    // 例如：读取传感器数据、控制设备等
    // ==========================================

    delay(100);
}
```

## 串口调试

打开串口监视器，波特率 **115200**，可以看到完整的调试输出：

```
========================================
  ESP8266 自动配网程序启动
========================================
[LED] LED状态模块初始化完成
[WiFi] WiFi管理模块初始化完成
[系统] 固件版本: 1.0.0
[系统] CPU频率: 80 MHz
[系统] Flash大小: 4096 KB
[系统] 可用内存: 45678 bytes
========================================
[状态机] 初始化 -> 初始化
[系统] 正在初始化...
[状态机] 初始化 -> 读取配置
[系统] 读取WiFi配置...
[WiFi] 读取到WiFi配置: SSID=MyWiFi
[系统] 找到已保存的WiFi配置: MyWiFi
[状态机] 读取配置 -> 连接WiFi中
[LED] 状态: WiFi连接中 (中闪)
[WiFi] 正在连接: MyWiFi (最多重试3次)
[WiFi] 第1次尝试连接...
[WiFi] 连接成功！
[WiFi] SSID: MyWiFi
[WiFi] IP地址: 192.168.1.100
[WiFi] 信号强度: -45 dBm
[WiFi] 连接成功！进入正常运行模式
[LED] 状态: WiFi已连接 (慢闪)
[状态机] 连接WiFi中 -> WiFi已连接
[运行] 系统运行中 | IP: 192.168.1.100 | RSSI: -45 dBm | 内存: 42345 bytes
```

## 常见问题

**Q: 找不到配网热点怎么办？**
A: 检查LED是否快闪，如果不是快闪说明设备已经连接了其他WiFi。可以擦除Flash重新配网。

**Q: 配网页面打不开？**
A: 确保已连接到 "ESP8266_Config" 热点，然后访问 http://192.168.4.1。如果是手机，可能需要关闭移动数据。

**Q: WiFi连接一直失败？**
A: 检查密码是否正确，WiFi信号是否足够强。ESP8266只支持2.4GHz WiFi，不支持5GHz。

**Q: 如何修改配网热点名称和密码？**
A: 修改 `config.h` 中的 `AP_SSID` 和 `AP_PASSWORD` 定义。

## 技术支持

如有问题，请检查串口输出的调试信息，这对定位问题非常有帮助。
