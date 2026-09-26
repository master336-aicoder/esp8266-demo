/*
 * AutoWifiConfig - ESP8266自动配网程序
 *
 * 功能说明：
 * 1. 启动后读取EEPROM中存储的WiFi配置
 * 2. 如果有有效配置，尝试连接WiFi（重试3次）
 * 3. 连接成功则进入正常运行模式
 * 4. 如果无配置或连接失败，则进入配网模式
 * 5. 配网模式下开启AP，用户连接后通过Web页面配置WiFi
 * 6. 配置成功后保存并重启，连接新的WiFi
 * 7. LED通过不同闪烁频率指示不同状态
 *
 * LED状态说明：
 * - 常灭：空闲/初始化
 * - 中闪(500ms)：正在连接WiFi
 * - 慢闪(2000ms)：WiFi已连接（正常运行）
 * - 快闪(200ms)：配网模式
 * - 超快闪(100ms)：连接失败/错误
 */

#include <Arduino.h>
#include "config.h"
#include "LedStatus.h"
#include "WifiManager.h"
#include "ConfigPortal.h"
#include "DeviceStatusWeb.h"
#include "MqttManager.h"
#include "TimeManager.h"
#include "DHTManager.h"

// ========== 全局对象 ==========
LedStatus led;
WifiManager wifiMgr;
ConfigPortal* portal = nullptr;
DeviceStatusWeb* statusWeb = nullptr;
MqttManager mqttMgr;
TimeManager timeMgr;
DHTManager dhtMgr;

// GPIO模式跟踪数组（用于GPIO16等特殊引脚）
uint8_t g_gpioMode[17] = {INPUT, INPUT, INPUT, INPUT, INPUT, INPUT,
                           INPUT, INPUT, INPUT, INPUT, INPUT,
                           INPUT, INPUT, INPUT, INPUT, INPUT, INPUT};

// ========== 系统状态枚举 ==========
enum AppState {
    STATE_INIT = 0,           // 初始化
    STATE_READ_CONFIG,        // 读取配置
    STATE_CONNECTING_WIFI,    // 正在连接WiFi
    STATE_WIFI_CONNECTED,     // WiFi已连接（正常运行）
    STATE_START_CONFIG,       // 启动配网模式
    STATE_CONFIG_MODE,        // 配网模式中
    STATE_CONFIG_CONNECTING,  // 配网模式下尝试连接
    STATE_RESTART             // 重启
};

AppState currentState = STATE_INIT;
AppState nextState = STATE_INIT;
bool wifiServiceInitialized = false; // WiFi连接后启动的服务是否已初始化

// 配网模式下尝试连接的WiFi信息
String configSsid = "";
String configPassword = "";
bool configConnectRequested = false;

// ========== 函数声明 ==========
void setState(AppState state);
void handleStateInit();
void handleStateReadConfig();
void handleStateConnectingWifi();
void handleStateWifiConnected();
void handleStateStartConfig();
void handleStateConfigMode();
void handleStateConfigConnecting();
void handleStateRestart();
void onConfigSubmitted(const char* ssid, const char* password);

// ========== setup ==========
void setup() {
    // 初始化调试串口
    DEBUG_SERIAL.begin(DEBUG_BAUD_RATE);
    delay(100);

    DEBUG_PRINTLN();
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("  ESP8266 自动配网程序启动");
    DEBUG_PRINTLN("========================================");

    // 初始化LED
    led.begin();

    // 初始化WiFi管理
    wifiMgr.begin();

    // 初始化MQTT管理
    mqttMgr.begin();

    DEBUG_PRINTF("[系统] 固件版本: 1.0.0\r\n");
    DEBUG_PRINTF("[系统] CPU频率: %d MHz\r\n", ESP.getCpuFreqMHz());
    DEBUG_PRINTF("[系统] Flash大小: %d KB\r\n", ESP.getFlashChipSize() / 1024);
    DEBUG_PRINTF("[系统] 可用内存: %d bytes\r\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================");

    // 进入初始状态
    setState(STATE_INIT);
}

// ========== loop ==========
void loop() {
    // 更新LED状态
    led.update();

    // 状态机处理
    switch (currentState) {
        case STATE_INIT:
            handleStateInit();
            break;
        case STATE_READ_CONFIG:
            handleStateReadConfig();
            break;
        case STATE_CONNECTING_WIFI:
            handleStateConnectingWifi();
            break;
        case STATE_WIFI_CONNECTED:
            handleStateWifiConnected();
            break;
        case STATE_START_CONFIG:
            handleStateStartConfig();
            break;
        case STATE_CONFIG_MODE:
            handleStateConfigMode();
            break;
        case STATE_CONFIG_CONNECTING:
            handleStateConfigConnecting();
            break;
        case STATE_RESTART:
            handleStateRestart();
            break;
    }
}

// ========== 状态切换 ==========
void setState(AppState state) {
    if (currentState == state) return;

    DEBUG_PRINTF("[状态机] %s -> ",
        currentState == STATE_INIT ? "初始化" :
        currentState == STATE_READ_CONFIG ? "读取配置" :
        currentState == STATE_CONNECTING_WIFI ? "连接WiFi中" :
        currentState == STATE_WIFI_CONNECTED ? "WiFi已连接" :
        currentState == STATE_START_CONFIG ? "启动配网" :
        currentState == STATE_CONFIG_MODE ? "配网模式" :
        currentState == STATE_CONFIG_CONNECTING ? "配网连接中" :
        currentState == STATE_RESTART ? "重启中" : "未知"
    );

    currentState = state;

    DEBUG_PRINTLN(
        currentState == STATE_INIT ? "初始化" :
        currentState == STATE_READ_CONFIG ? "读取配置" :
        currentState == STATE_CONNECTING_WIFI ? "连接WiFi中" :
        currentState == STATE_WIFI_CONNECTED ? "WiFi已连接" :
        currentState == STATE_START_CONFIG ? "启动配网" :
        currentState == STATE_CONFIG_MODE ? "配网模式" :
        currentState == STATE_CONFIG_CONNECTING ? "配网连接中" :
        currentState == STATE_RESTART ? "重启中" : "未知"
    );
}

// ========== 状态：初始化 ==========
void handleStateInit() {
    DEBUG_PRINTLN("[系统] 正在初始化...");
    delay(500);
    setState(STATE_READ_CONFIG);
}

// ========== 状态：读取配置 ==========
void handleStateReadConfig() {
    DEBUG_PRINTLN("[系统] 读取WiFi配置...");

    WifiCredentials creds = wifiMgr.readCredentials();

    if (creds.valid) {
        DEBUG_PRINTF("[系统] 找到已保存的WiFi配置: %s\r\n", creds.ssid);
        setState(STATE_CONNECTING_WIFI);
    } else {
        DEBUG_PRINTLN("[系统] 未找到WiFi配置，进入配网模式");
        setState(STATE_START_CONFIG);
    }
}

// ========== 状态：连接WiFi中 ==========
void handleStateConnectingWifi() {
    led.setStatus(STATUS_WIFI_CONNECTING);

    WifiCredentials creds = wifiMgr.readCredentials();

    if (!creds.valid) {
        DEBUG_PRINTLN("[WiFi] 配置无效，进入配网模式");
        setState(STATE_START_CONFIG);
        return;
    }

    DEBUG_PRINTF("[WiFi] 正在连接: %s\r\n", creds.ssid);

    bool connected = wifiMgr.connect(creds.ssid, creds.password, WIFI_RETRY_COUNT);

    if (connected) {
        DEBUG_PRINTLN("[WiFi] 连接成功！进入正常运行模式");
        led.setStatus(STATUS_WIFI_CONNECTED);
        setState(STATE_WIFI_CONNECTED);
    } else {
        DEBUG_PRINTLN("[WiFi] 连接失败，进入配网模式");
        led.setStatus(STATUS_WIFI_FAILED);
        delay(1000); // 失败状态显示一会儿
        setState(STATE_START_CONFIG);
    }
}

// ========== 状态：WiFi已连接（正常运行） ==========
void handleStateWifiConnected() {
    // 第一次进入时启动状态Web服务和时间同步
    if (!wifiServiceInitialized) {
        wifiServiceInitialized = true;

        // 初始化DHT温湿度传感器
        dhtMgr.begin();

        statusWeb = new DeviceStatusWeb(80);
        statusWeb->setMqttManager(&mqttMgr);
        statusWeb->setTimeManager(&timeMgr);
        statusWeb->setDHTManager(&dhtMgr);
        statusWeb->begin();

        // 启动NTP时间同步
        timeMgr.begin();
        timeMgr.forceSync();
    }

    // 处理Web客户端请求
    if (statusWeb) {
        statusWeb->handleClient();
    }

    // 时间同步
    timeMgr.loop();

    // DHT温湿度读取
    dhtMgr.loop();

    // 处理MQTT
    mqttMgr.loop();

    // 检查WiFi连接状态
    static unsigned long lastCheck = 0;
    unsigned long now = millis();

    if (now - lastCheck >= 5000) {
        lastCheck = now;

        if (!wifiMgr.isConnected()) {
            DEBUG_PRINTLN("[WiFi] 连接断开，尝试重连...");
            led.setStatus(STATUS_WIFI_CONNECTING);
            
            // 停止MQTT
            mqttMgr.disconnect();
            
            // 停止Web服务
            if (statusWeb) {
                statusWeb->stop();
                delete statusWeb;
                statusWeb = nullptr;
            }
            wifiServiceInitialized = false;

            setState(STATE_CONNECTING_WIFI);
            return;
        }

        // 输出运行状态
        static int counter = 0;
        counter++;
        if (counter % 12 == 0) { // 每分钟输出一次
            bool mqttOk = mqttMgr.isConnected();
            DEBUG_PRINTF("[运行] 系统运行中 | IP: %s | RSSI: %d dBm | MQTT: %s | 内存: %d bytes\r\n",
                wifiMgr.getIP().c_str(),
                WiFi.RSSI(),
                mqttOk ? "已连接" : "未连接",
                ESP.getFreeHeap());
        }
    }

    // ========== 在这里添加你的业务代码 ==========
    // 例如：读取传感器数据、控制设备等
    // ==========================================

    delay(10);
}

// ========== 状态：启动配网模式 ==========
void handleStateStartConfig() {
    DEBUG_PRINTLN("[配网] 启动配网模式...");

    // 断开WiFi连接
    wifiMgr.disconnect();
    delay(500);

    // 创建配网门户
    if (portal) {
        delete portal;
    }
    portal = new ConfigPortal(wifiMgr, led);
    portal->begin(onConfigSubmitted);

    led.setStatus(STATUS_CONFIG_MODE);
    setState(STATE_CONFIG_MODE);
}

// ========== 状态：配网模式中 ==========
void handleStateConfigMode() {
    // 处理Web客户端请求
    if (portal) {
        portal->handleClient();
    }

    // 检查是否有配网连接请求
    if (configConnectRequested) {
        configConnectRequested = false;
        setState(STATE_CONFIG_CONNECTING);
        return;
    }

    delay(10);
}

// ========== 状态：配网模式下尝试连接 ==========
void handleStateConfigConnecting() {
    DEBUG_PRINTF("[配网] 尝试连接WiFi: %s\r\n", configSsid.c_str());
    led.setStatus(STATUS_WIFI_CONNECTING);

    // 停止配网门户的AP（但保持Web服务器运行以便返回状态）
    // 实际上我们需要切换到STA模式尝试连接
    WiFi.mode(WIFI_STA);
    WiFi.begin(configSsid.c_str(), configPassword.c_str());

    unsigned long startTime = millis();
    bool connected = false;

    // 等待连接（最多30秒，给前端轮询留时间）
    while (millis() - startTime < 30000) {
        if (portal) {
            portal->handleClient(); // 继续处理Web请求
        }
        led.update();

        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            break;
        }
        delay(50);
    }

    if (connected) {
        DEBUG_PRINTLN("[配网] WiFi连接成功！");
        DEBUG_PRINTF("[配网] IP地址: %s\r\n", WiFi.localIP().toString().c_str());

        // 保存配置
        wifiMgr.saveCredentials(configSsid.c_str(), configPassword.c_str());

        led.setStatus(STATUS_WIFI_CONNECTED);

        // 等待几秒钟让前端看到成功状态
        DEBUG_PRINTLN("[配网] 等待前端确认...");
        unsigned long waitStart = millis();
        while (millis() - waitStart < 3000) {
            if (portal) {
                portal->handleClient();
            }
            led.update();
            delay(50);
        }

        // 停止配网门户，进入正常运行
        if (portal) {
            portal->stop();
            delete portal;
            portal = nullptr;
        }

        setState(STATE_WIFI_CONNECTED);
    } else {
        DEBUG_PRINTLN("[配网] WiFi连接失败，返回配网模式");
        led.setStatus(STATUS_WIFI_FAILED);
        delay(1000);

        // 重新启动配网AP
        wifiMgr.disconnect();
        delay(200);

        if (portal) {
            portal->stop();
            delete portal;
        }
        portal = new ConfigPortal(wifiMgr, led);
        portal->begin(onConfigSubmitted);

        led.setStatus(STATUS_CONFIG_MODE);
        setState(STATE_CONFIG_MODE);
    }
}

// ========== 状态：重启 ==========
void handleStateRestart() {
    DEBUG_PRINTLN("[系统] 系统即将重启...");
    delay(1000);
    ESP.restart();
}

// ========== 配网提交回调 ==========
void onConfigSubmitted(const char* ssid, const char* password) {
    DEBUG_PRINTF("[配网] 收到配网请求: SSID=%s\r\n", ssid);
    configSsid = ssid;
    configPassword = password;
    configConnectRequested = true;
}
