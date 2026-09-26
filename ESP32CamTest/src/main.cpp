/*
 * ESP32 摄像头测试程序
 * 功能：实时视频流、拍照、分辨率/质量调节、特效切换
 * 支持：AI-Thinker ESP32-CAM / ESP32-WROVER / 自定义引脚
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_camera.h>
#include <vector>
#include <algorithm>

// ============== 用户配置 ==============

// WiFi 配置（请修改为你的 WiFi）
const char* WIFI_SSID = "TP-LINK_5G";
const char* WIFI_PASSWORD = "";

// 摄像头模块类型选择
// 0 = AI-Thinker ESP32-CAM (最常见)
// 1 = ESP32-WROVER-KIT
// 2 = M5Stack Timer Camera
// 3 = 自定义引脚
#define CAMERA_MODEL 0

// =======================================

// 摄像头引脚配置
#if CAMERA_MODEL == 0
// AI-Thinker ESP32-CAM
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

#elif CAMERA_MODEL == 1
// ESP32-WROVER-KIT
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     21
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       19
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM        5
#define Y2_GPIO_NUM        4
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#elif CAMERA_MODEL == 2
// M5Stack Timer Camera
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     25
#define SIOC_GPIO_NUM     23
#define Y9_GPIO_NUM       19
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM        5
#define Y4_GPIO_NUM       34
#define Y3_GPIO_NUM       35
#define Y2_GPIO_NUM       32
#define VSYNC_GPIO_NUM    22
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

#else
// 自定义引脚 - 请根据实际硬件修改
#define PWDN_GPIO_NUM     -1
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
#endif

// ============== 全局变量 ==============

WebServer server(80);

// 当前摄像头设置
int currentResolution = 10;  // QVGA 320x240
int currentQuality = 12;     // JPEG 质量 0-63 (越小质量越好)
int currentFrameSize = FRAMESIZE_QVGA;
bool streamEnabled = true;

// 分辨率选项
const char* resolutionNames[] = {
    "96X96", "QQVGA (160x120)", "QCIF (176x144)",
    "HQVGA (240x176)", "240x240", "QVGA (320x240)",
    "CIF (400x296)", "VGA (640x480)", "SVGA (800x600)",
    "XGA (1024x768)", "HD (1280x720)", "SXGA (1280x1024)",
    "UXGA (1600x1200)"
};

// 特效选项
const char* effectNames[] = {
    "无", "负片", "灰度", "红色调",
    "绿色调", "蓝色调", "复古", "黑白",
    "正片", "对比度+", "对比度-",
    "亮度+", "亮度-", "饱和度+", "饱和度-"
};

int currentEffect = 0;

// ============== 函数声明 ==============

bool initCamera();
void initWiFi();
void handleRoot();
void handleStream();
void handleCapture();
void handleSetResolution();
void handleSetQuality();
void handleSetEffect();
void handleStatus();
String getStatusJSON();

// ============== 摄像头初始化 ==============

bool initCamera() {
    Serial.println("[CAM] 初始化摄像头...");

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; // sda
    config.pin_sccb_scl = SIOC_GPIO_NUM; // scl
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    // 检测 PSRAM 并设置帧大小
    if (psramFound()) {
        Serial.println("[CAM] 检测到 PSRAM");
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 2;  // 双缓冲
    } else {
        Serial.println("[CAM] 未检测到 PSRAM，降低分辨率");
        config.frame_size = FRAMESIZE_QQVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    // 初始化摄像头
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAM] 初始化失败: 0x%x\n", err);
        return false;
    }

    Serial.println("[CAM] 初始化成功！");

    // 打印传感器信息
    sensor_t* s = esp_camera_sensor_get();
    Serial.printf("[CAM] 传感器ID: 0x%x\n", s->id);
    Serial.printf("[CAM] 当前分辨率: %s\n", resolutionNames[s->status.framesize]);

    return true;
}

// ============== WiFi 连接 ==============

void initWiFi() {
    Serial.printf("[WiFi] 连接到: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30) {
        delay(500);
        Serial.print(".");
        retry++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println();
        Serial.println("[WiFi] 连接成功！");
        Serial.printf("[WiFi] IP 地址: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[WiFi] 信号强度: %d dBm\n", WiFi.RSSI());
    } else {
        Serial.println();
        Serial.println("[WiFi] 连接失败！");
        Serial.println("[WiFi] 请检查 WiFi 配置（main.cpp 顶部）");
    }
}

// ============== Web 页面 ==============

void handleRoot() {
    String html = "<!DOCTYPE html><html lang='zh-CN'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>ESP32 摄像头测试</title>";
    html += "<style>";
    html += "*{box-sizing:border-box;margin:0;padding:0}";
    html += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Microsoft YaHei',sans-serif;background:#f5f6fa;color:#1e2230;padding:20px}";
    html += ".container{max-width:800px;margin:0 auto}";
    html += ".header{background:linear-gradient(135deg,#6366f1,#8b5cf6);color:white;padding:24px;border-radius:12px;margin-bottom:20px;box-shadow:0 4px 12px rgba(0,0,0,0.1)}";
    html += ".header h1{font-size:24px;margin-bottom:8px}";
    html += ".header p{opacity:0.9;font-size:14px}";
    html += ".card{background:white;border-radius:12px;padding:20px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,0.05);border:1px solid #e4e7ef}";
    html += ".card h2{font-size:18px;margin-bottom:16px;color:#1e2230;border-bottom:2px solid #6366f1;padding-bottom:8px;display:inline-block}";
    html += ".video-container{text-align:center;background:#1e2230;border-radius:8px;padding:10px;margin-bottom:16px}";
    html += ".video-container img{max-width:100%;border-radius:4px}";
    html += ".controls{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px}";
    html += ".control-group{display:flex;flex-direction:column;gap:6px}";
    html += ".control-group label{font-size:13px;color:#6b7280;font-weight:500}";
    html += ".control-group select,.control-group input{padding:8px 12px;border:1px solid #d0d5e0;border-radius:6px;font-size:14px;background:white}";
    html += ".btn{padding:10px 20px;border:none;border-radius:6px;font-size:14px;font-weight:500;cursor:pointer;transition:all 0.15s}";
    html += ".btn-primary{background:#6366f1;color:white}";
    html += ".btn-primary:hover{background:#4f46e5}";
    html += ".btn-success{background:#10b981;color:white}";
    html += ".btn-success:hover{background:#059669}";
    html += ".btn-row{display:flex;gap:8px;flex-wrap:wrap}";
    html += ".status-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:12px}";
    html += ".status-item{background:#f5f6fa;padding:12px;border-radius:8px}";
    html += ".status-label{font-size:12px;color:#6b7280;margin-bottom:4px}";
    html += ".status-value{font-size:16px;font-weight:600;color:#1e2230}";
    html += ".status-ok{color:#10b981}";
    html += ".status-err{color:#ef4444}";
    html += "hr{border:none;border-top:1px solid #e4e7ef;margin:16px 0}";
    html += "</style></head><body>";

    html += "<div class='container'>";

    // 标题
    html += "<div class='header'>";
    html += "<h1>📷 ESP32 摄像头测试</h1>";
    html += "<p>实时视频流 · 拍照 · 分辨率调节 · 特效切换</p>";
    html += "</div>";

    // 视频流
    html += "<div class='card'>";
    html += "<h2>实时画面</h2>";
    html += "<div class='video-container'>";
    html += "<img id='video' src='/stream' alt='摄像头画面' onclick='refreshImage()' title='点击刷新'>";
    html += "</div>";
    html += "<div class='btn-row'>";
    html += "<button class='btn btn-primary' onclick='refreshImage()'>🔄 刷新画面</button>";
    html += "<button class='btn btn-success' onclick='capturePhoto()'>📸 拍照保存</button>";
    html += "</div>";
    html += "</div>";

    // 控制
    html += "<div class='card'>";
    html += "<h2>摄像头设置</h2>";
    html += "<div class='controls'>";

    // 分辨率
    html += "<div class='control-group'>";
    html += "<label>分辨率</label>";
    html += "<select id='resolution' onchange='setResolution()'>";
    for (int i = 0; i < 13; i++) {
        html += "<option value='" + String(i) + "'>" + String(resolutionNames[i]) + "</option>";
    }
    html += "</select>";
    html += "</div>";

    // 质量
    html += "<div class='control-group'>";
    html += "<label>JPEG 质量 (数值越小质量越好)</label>";
    html += "<input type='range' id='quality' min='4' max='63' value='12' oninput='setQuality()'>";
    html += "<span id='qualityVal'>12</span>";
    html += "</div>";

    // 特效
    html += "<div class='control-group'>";
    html += "<label>图像特效</label>";
    html += "<select id='effect' onchange='setEffect()'>";
    for (int i = 0; i < 15; i++) {
        html += "<option value='" + String(i) + "'>" + String(effectNames[i]) + "</option>";
    }
    html += "</select>";
    html += "</div>";

    html += "</div></div>";

    // 状态信息
    html += "<div class='card'>";
    html += "<h2>系统状态</h2>";
    html += "<div class='status-grid' id='statusGrid'>";
    html += "<div class='status-item'><div class='status-label'>WiFi</div><div class='status-value' id='wifiStatus'>-</div></div>";
    html += "<div class='status-item'><div class='status-label'>IP地址</div><div class='status-value' id='ipAddr'>-</div></div>";
    html += "<div class='status-item'><div class='status-label'>分辨率</div><div class='status-value' id='curRes'>-</div></div>";
    html += "<div class='status-item'><div class='status-label'>PSRAM</div><div class='status-value' id='psram'>-</div></div>";
    html += "<div class='status-item'><div class='status-label'>可用内存</div><div class='status-value' id='freeHeap'>-</div></div>";
    html += "<div class='status-item'><div class='status-label'>运行时间</div><div class='status-value' id='uptime'>-</div></div>";
    html += "</div>";
    html += "</div>";

    // 底部提示
    html += "<div class='card' style='background:#fef3c7;border-color:#f59e0b'>";
    html += "<h2 style='border-color:#f59e0b'>💡 使用提示</h2>";
    html += "<ul style='padding-left:20px;line-height:1.8;font-size:14px'>";
    html += "<li>点击画面可以刷新</li>";
    html += "<li>分辨率越高，帧率越低</li>";
    html += "<li>如果画面卡顿，降低分辨率或提高质量数值</li>";
    html += "<li>PSRAM 是必需的，否则只能用最低分辨率</li>";
    html += "<li>串口监视器可以查看详细日志（115200 baud）</li>";
    html += "</ul></div>";

    html += "</div>";  // container

    // JavaScript
    html += "<script>";
    html += "function refreshImage(){document.getElementById('video').src='/stream?t='+Date.now()}";
    html += "function setResolution(){var v=document.getElementById('resolution').value;fetch('/set_resolution?value='+v).then(r=>r.json()).then(d=>{if(d.success){refreshImage()}})}";
    html += "function setQuality(){var v=document.getElementById('quality').value;document.getElementById('qualityVal').textContent=v;fetch('/set_quality?value='+v).then(r=>r.json())}";
    html += "function setEffect(){var v=document.getElementById('effect').value;fetch('/set_effect?value='+v).then(r=>r.json()).then(d=>{if(d.success){refreshImage()}})}";
    html += "function capturePhoto(){var a=document.createElement('a');a.href='/capture?t='+Date.now();a.download='capture_'+Date.now()+'.jpg';a.click()}";
    html += "function updateStatus(){fetch('/status').then(r=>r.json()).then(d=>{";
    html += "document.getElementById('wifiStatus').textContent=d.wifi?'已连接':'未连接';document.getElementById('wifiStatus').className='status-value '+(d.wifi?'status-ok':'status-err');";
    html += "document.getElementById('ipAddr').textContent=d.ip;";
    html += "document.getElementById('curRes').textContent=d.resolution;";
    html += "document.getElementById('psram').textContent=d.psram?'有':'无';document.getElementById('psram').className='status-value '+(d.psram?'status-ok':'status-err');";
    html += "document.getElementById('freeHeap').textContent=Math.round(d.free_heap/1024)+' KB';";
    html += "var s=d.uptime;var h=Math.floor(s/3600);var m=Math.floor((s%3600)/60);s=s%60;";
    html += "document.getElementById('uptime').textContent=h+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0');";
    html += "document.getElementById('resolution').value=d.res_index;";
    html += "document.getElementById('quality').value=d.quality;document.getElementById('qualityVal').textContent=d.quality;";
    html += "document.getElementById('effect').value=d.effect;";
    html += "})}";
    html += "setInterval(updateStatus,5000);updateStatus();";
    html += "</script>";

    html += "</body></html>";

    server.send(200, "text/html", html);
}

// ============== 视频流 MJPEG ==============

void handleStream() {
    WiFiClient client = server.client();
    if (!client.connected()) return;

    Serial.println("[STREAM] 客户端连接");

    // 发送 MJPEG 响应头
    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "\r\n";
    server.sendContent(response);

    int frameCount = 0;
    unsigned long startTime = millis();

    while (client.connected()) {
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[STREAM] 获取帧失败");
            delay(10);
            continue;
        }

        // 发送帧头
        String header = "--frame\r\n";
        header += "Content-Type: image/jpeg\r\n";
        header += "Content-Length: " + String(fb->len) + "\r\n\r\n";
        server.sendContent(header);

        // 发送帧数据
        size_t sent = client.write(fb->buf, fb->len);
        if (sent != fb->len) {
            Serial.println("[STREAM] 发送不完整，客户端断开");
            esp_camera_fb_return(fb);
            break;
        }

        server.sendContent("\r\n");
        esp_camera_fb_return(fb);

        frameCount++;

        // 每 30 帧打印一次帧率
        if (frameCount % 30 == 0) {
            float elapsed = (millis() - startTime) / 1000.0;
            float fps = frameCount / elapsed;
            Serial.printf("[STREAM] FPS: %.1f, 已发送 %d 帧\n", fps, frameCount);
        }

        // 客户端超时断开
        if (!client.available() && !client.connected()) {
            break;
        }

        delay(1);  // 小延时
    }

    float elapsed = (millis() - startTime) / 1000.0;
    Serial.printf("[STREAM] 结束: %d 帧, %.1f 秒, %.1f FPS\n",
                  frameCount, elapsed, frameCount / max(elapsed, 0.001f));
}

// ============== 拍照 ==============

void handleCapture() {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        server.send(500, "text/plain", "拍照失败");
        return;
    }

    Serial.printf("[CAPTURE] 拍照: %d 字节\n", fb->len);

    server.sendHeader("Content-Type", "image/jpeg");
    server.sendHeader("Content-Length", String(fb->len));
    server.sendHeader("Content-Disposition", "attachment; filename=capture.jpg");
    server.send(200, "image/jpeg", "");
    server.sendContent((const char*)fb->buf, fb->len);

    esp_camera_fb_return(fb);
}

// ============== 设置分辨率 ==============

void handleSetResolution() {
    if (!server.hasArg("value")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"missing value\"}");
        return;
    }

    int value = server.arg("value").toInt();
    if (value < 0 || value > 12) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"invalid value\"}");
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_framesize(s, (framesize_t)value);
        currentFrameSize = value;
        Serial.printf("[CAM] 分辨率设置为: %s\n", resolutionNames[value]);
    }

    server.send(200, "application/json", "{\"success\":true}");
}

// ============== 设置质量 ==============

void handleSetQuality() {
    if (!server.hasArg("value")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"missing value\"}");
        return;
    }

    int value = server.arg("value").toInt();
    if (value < 4 || value > 63) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"invalid value\"}");
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_quality(s, value);
        currentQuality = value;
        Serial.printf("[CAM] 质量设置为: %d\n", value);
    }

    server.send(200, "application/json", "{\"success\":true}");
}

// ============== 设置特效 ==============

void handleSetEffect() {
    if (!server.hasArg("value")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"missing value\"}");
        return;
    }

    int value = server.arg("value").toInt();
    if (value < 0 || value > 14) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"invalid value\"}");
        return;
    }

    sensor_t* s = esp_camera_sensor_get();
    if (s) {
        s->set_special_effect(s, value);
        currentEffect = value;
        Serial.printf("[CAM] 特效设置为: %s\n", effectNames[value]);
    }

    server.send(200, "application/json", "{\"success\":true}");
}

// ============== 状态查询 ==============

String getStatusJSON() {
    sensor_t* s = esp_camera_sensor_get();
    String json = "{";
    json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
    json += "\"resolution\":\"" + String(s ? resolutionNames[s->status.framesize] : "未知") + "\",";
    json += "\"res_index\":" + String(s ? s->status.framesize : 0) + ",";
    json += "\"quality\":" + String(s ? s->status.quality : currentQuality) + ",";
    json += "\"effect\":" + String(currentEffect) + ",";
    json += "\"psram\":" + String(psramFound() ? "true" : "false") + ",";
    json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
    json += "\"uptime\":" + String(millis() / 1000);
    json += "}";
    return json;
}

void handleStatus() {
    server.send(200, "application/json", getStatusJSON());
}

// ============== setup ==============

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("   ESP32 摄像头测试程序");
    Serial.println("========================================");
    Serial.println();

    // 打印系统信息
    Serial.printf("[SYS] 芯片型号: %s\n", ESP.getChipModel());
    Serial.printf("[SYS] CPU 频率: %d MHz\n", ESP.getCpuFreqMHz());
    Serial.printf("[SYS] Flash 大小: %d MB\n", ESP.getFlashChipSize() / 1024 / 1024);
    Serial.printf("[SYS] 可用内存: %d KB\n", ESP.getFreeHeap() / 1024);
    Serial.printf("[SYS] PSRAM: %s\n", psramFound() ? "已检测到" : "未检测到");
    Serial.println();

    // 初始化摄像头
    if (!initCamera()) {
        Serial.println("[ERR] 摄像头初始化失败！");
        Serial.println("[ERR] 请检查接线或修改 CAMERA_MODEL 配置");
        while (1) {
            delay(1000);
        }
    }

    // 连接 WiFi
    initWiFi();

    // 启动 Web 服务器
    server.on("/", handleRoot);
    server.on("/stream", handleStream);
    server.on("/capture", handleCapture);
    server.on("/set_resolution", handleSetResolution);
    server.on("/set_quality", handleSetQuality);
    server.on("/set_effect", handleSetEffect);
    server.on("/status", handleStatus);

    server.begin();
    Serial.println("[WEB] 服务器已启动");
    Serial.printf("[WEB] 访问地址: http://%s/\n", WiFi.localIP().toString().c_str());
    Serial.println();
    Serial.println("========================================");
    Serial.println("  摄像头测试就绪，请在浏览器访问上述地址");
    Serial.println("========================================");
}

// ============== loop ==============

void loop() {
    server.handleClient();
    delay(1);
}
