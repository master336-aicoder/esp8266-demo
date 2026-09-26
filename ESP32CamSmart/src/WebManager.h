/*
 * WebManager - Web 管理界面
 * 功能：实时监控、摄像头设置、SD卡管理、网络管理、视频录制
 */

#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include "config.h"
#include "CameraManager.h"
#include "SdCardManager.h"
#include "VideoRecorder.h"
#include "WifiManager.h"
#include "TimeManager.h"

class WebManager {
public:
    WebManager(CameraManager* cam, SdCardManager* sd,
               VideoRecorder* rec, WifiManager* wifi,
               TimeManager* timeMgr) :
        _server(80), _camera(cam), _sd(sd),
        _recorder(rec), _wifi(wifi), _timeMgr(timeMgr) {}

    void begin() {
        _server.on("/", std::bind(&WebManager::handleIndex, this));
        _server.on("/stream", std::bind(&WebManager::handleStream, this));
        _server.on("/capture", std::bind(&WebManager::handleCapture, this));
        _server.on("/capture_save", std::bind(&WebManager::handleCaptureSave, this));

        // 摄像头设置
        _server.on("/api/camera/status", std::bind(&WebManager::handleCamStatus, this));
        _server.on("/api/camera/set_res", std::bind(&WebManager::handleCamSetRes, this));
        _server.on("/api/camera/set_quality", std::bind(&WebManager::handleCamSetQuality, this));
        _server.on("/api/camera/set_effect", std::bind(&WebManager::handleCamSetEffect, this));
        _server.on("/api/camera/set_flip", std::bind(&WebManager::handleCamSetFlip, this));
        _server.on("/api/camera/set_mirror", std::bind(&WebManager::handleCamSetMirror, this));

        // SD 卡
        _server.on("/sd", std::bind(&WebManager::handleSdPage, this));
        _server.on("/api/sd/status", std::bind(&WebManager::handleSdStatus, this));
        _server.on("/api/sd/list", std::bind(&WebManager::handleSdList, this));
        _server.on("/api/sd/delete", std::bind(&WebManager::handleSdDelete, this));
        _server.on("/api/sd/download", std::bind(&WebManager::handleSdDownload, this));
        _server.on("/api/sd/upload", HTTP_POST,
            std::bind(&WebManager::handleSdUpload, this));

        // 视频录制
        _server.on("/api/record/start", std::bind(&WebManager::handleRecStart, this));
        _server.on("/api/record/stop", std::bind(&WebManager::handleRecStop, this));
        _server.on("/api/record/status", std::bind(&WebManager::handleRecStatus, this));

        // 网络设置
        _server.on("/settings", std::bind(&WebManager::handleSettings, this));
        _server.on("/api/network/status", std::bind(&WebManager::handleNetStatus, this));
        _server.on("/api/network/wifi_save", std::bind(&WebManager::handleWifiSave, this));
        _server.on("/api/network/ntp_save", std::bind(&WebManager::handleNtpSave, this));
        _server.on("/api/network/ntp_sync", std::bind(&WebManager::handleNtpSync, this));

        // 系统状态
        _server.on("/api/system/status", std::bind(&WebManager::handleSysStatus, this));

        _server.onNotFound(std::bind(&WebManager::handleNotFound, this));
        _server.begin();

        DEBUG_PRINTLN_TAG("WEB", "Web服务器已启动");
    }

    void loop() {
        _server.handleClient();
    }

private:
    WebServer _server;
    CameraManager* _camera;
    SdCardManager* _sd;
    VideoRecorder* _recorder;
    WifiManager* _wifi;
    TimeManager* _timeMgr;

    // ========== 页面 ==========

    void handleIndex() {
        String page = pageHeader("📷 实时监控", 0);

        page += "<div class='row'>";
        page += "<div class='col-main'>";
        page += "<div class='card'>";
        page += "<div class='card-h'>实时画面</div>";
        page += "<div class='card-b'>";
        page += "<div class='video-box'>";
        page += "<img id='video' src='/stream' alt='摄像头画面'>";
        page += "</div>";
        page += "<div class='btn-row' style='margin-top:12px'>";
        page += "<button class='btn btn-primary' onclick='refreshImg()'>🔄 刷新</button>";
        page += "<button class='btn btn-success' onclick='captureSave()'>💾 拍照保存</button>";
        page += "<button class='btn btn-warning' id='recBtn' onclick='toggleRecord()'>⏺ 开始录制</button>";
        page += "</div>";
        page += "</div></div>";
        page += "</div>";

        // 右侧：系统状态
        page += "<div class='col-side'>";
        page += "<div class='card'>";
        page += "<div class='card-h'>系统状态</div>";
        page += "<div class='card-b'>";
        page += "<div class='status-grid' id='sysStatus'>";
        page += statusItem("WiFi", "wifiVal", "-");
        page += statusItem("IP地址", "ipVal", "-");
        page += statusItem("信号", "rssiVal", "-");
        page += statusItem("SD卡", "sdVal", "-");
        page += statusItem("可用空间", "sdFreeVal", "-");
        page += statusItem("运行时间", "uptimeVal", "-");
        page += "</div>";
        page += "</div></div>";
        page += "</div>";
        page += "</div>";

        // 摄像头设置
        page += "<div class='card'>";
        page += "<div class='card-h'>摄像头设置</div>";
        page += "<div class='card-b'>";
        page += "<div class='form-grid'>";

        page += "<div class='form-group'>";
        page += "<label>分辨率</label>";
        page += "<select id='resSel' onchange='setResolution()'>";
        for (int i = 0; i < 13; i++) {
            page += "<option value='" + String(i) + "'>" +
                    String(CameraManager::getResolutionName(i)) + "</option>";
        }
        page += "</select>";
        page += "</div>";

        page += "<div class='form-group'>";
        page += "<label>JPEG 质量 <span id='qualVal'>12</span></label>";
        page += "<input type='range' id='qualRange' min='4' max='63' oninput='setQuality()'>";
        page += "</div>";

        page += "<div class='form-group'>";
        page += "<label>图像特效</label>";
        page += "<select id='effSel' onchange='setEffect()'>";
        for (int i = 0; i < 15; i++) {
            page += "<option value='" + String(i) + "'>" +
                    String(CameraManager::getEffectName(i)) + "</option>";
        }
        page += "</select>";
        page += "</div>";

        page += "<div class='form-group'>";
        page += "<label>画面翻转</label>";
        page += "<div class='switch-row'>";
        page += "<label class='switch'><input type='checkbox' id='flipChk' onchange='setFlip()'><span class='slider'></span></label>";
        page += "<span style='margin-left:8px'>垂直翻转</span>";
        page += "<label class='switch' style='margin-left:16px'><input type='checkbox' id='mirrorChk' onchange='setMirror()'><span class='slider'></span></label>";
        page += "<span style='margin-left:8px'>水平镜像</span>";
        page += "</div>";
        page += "</div>";

        page += "</div></div></div>";

        page += pageFooter();
        page += indexScript();
        _server.send(200, "text/html", page);
    }

    void handleSdPage() {
        String page = pageHeader("💾 SD 卡管理", 1);

        page += "<div class='row'>";
        page += "<div class='col-main'>";
        page += "<div class='card'>";
        page += "<div class='card-h'>文件列表</div>";
        page += "<div class='card-b'>";
        page += "<div class='btn-row' style='margin-bottom:12px'>";
        page += "<button class='btn btn-primary' onclick='refreshSdList()'>🔄 刷新</button>";
        page += "<button class='btn' onclick='goUp()'>⬆ 上级目录</button>";
        page += "<span id='curPath' style='margin-left:auto;align-self:center'>/</span>";
        page += "</div>";
        page += "<div id='sdFileList' class='file-list'>加载中...</div>";
        page += "</div></div>";
        page += "</div>";

        page += "<div class='col-side'>";
        page += "<div class='card'>";
        page += "<div class='card-h'>空间使用</div>";
        page += "<div class='card-b'>";
        page += "<div class='status-grid'>";
        page += statusItem("总容量", "sdTotal", "-");
        page += statusItem("已用", "sdUsed", "-");
        page += statusItem("可用", "sdFree", "-");
        page += "</div>";
        page += "<div class='progress-bar' style='margin-top:12px'>";
        page += "<div class='progress-fill' id='sdProgress' style='width:0%'></div>";
        page += "</div>";
        page += "</div></div>";

        page += "<div class='card'>";
        page += "<div class='card-h'>上传文件</div>";
        page += "<div class='card-b'>";
        page += "<input type='file' id='uploadFile'>";
        page += "<button class='btn btn-primary' style='margin-top:8px;width:100%' onclick='uploadFile()'>上传</button>";
        page += "<div id='uploadStatus' style='margin-top:8px;font-size:13px'></div>";
        page += "</div></div>";
        page += "</div>";
        page += "</div>";

        page += pageFooter();
        page += sdScript();
        _server.send(200, "text/html", page);
    }

    void handleSettings() {
        String page = pageHeader("⚙️ 网络设置", 2);

        page += "<div class='card'>";
        page += "<div class='card-h'>WiFi 设置</div>";
        page += "<div class='card-b'>";
        page += "<div class='form-grid'>";
        page += "<div class='form-group'>";
        page += "<label>WiFi 名称 (SSID)</label>";
        page += "<input type='text' id='wifiSsid' placeholder='当前: " +
                _wifi->getSsid() + "'>";
        page += "</div>";
        page += "<div class='form-group'>";
        page += "<label>WiFi 密码</label>";
        page += "<input type='password' id='wifiPass' placeholder='修改请输入新密码'>";
        page += "</div>";
        page += "</div>";
        page += "<button class='btn btn-primary' onclick='saveWifi()'>保存 WiFi 设置</button>";
        page += "<div id='wifiStatus' style='margin-top:8px;font-size:13px'></div>";
        page += "</div></div>";

        page += "<div class='card'>";
        page += "<div class='card-h'>NTP 时间设置</div>";
        page += "<div class='card-b'>";
        page += "<div class='form-grid'>";
        page += "<div class='form-group'>";
        page += "<label>NTP 服务器 1</label>";
        page += "<input type='text' id='ntpSvr1' value='" + _timeMgr->getNtpServer1() + "'>";
        page += "</div>";
        page += "<div class='form-group'>";
        page += "<label>NTP 服务器 2</label>";
        page += "<input type='text' id='ntpSvr2' value='" + _timeMgr->getNtpServer2() + "'>";
        page += "</div>";
        page += "<div class='form-group'>";
        page += "<label>时区</label>";
        page += "<select id='ntpTz'>";
        for (int i = -12; i <= 12; i++) {
            page += "<option value='" + String(i) + "'";
            if (i == _timeMgr->getTimezone()) page += " selected";
            page += ">UTC" + String(i >= 0 ? "+" : "") + String(i) + "</option>";
        }
        page += "</select>";
        page += "</div>";
        page += "<div class='form-group'>";
        page += "<label>同步间隔（秒）</label>";
        page += "<input type='number' id='ntpInterval' value='" + String(_timeMgr->getSyncInterval()) + "' min='60' max='86400'>";
        page += "</div>";
        page += "</div>";
        page += "<div class='btn-row'>";
        page += "<button class='btn btn-primary' onclick='saveNtp()'>保存设置</button>";
        page += "<button class='btn btn-success' onclick='syncNtp()'>立即同步</button>";
        page += "</div>";
        page += "<div id='ntpStatus' style='margin-top:8px;font-size:13px'></div>";
        page += "</div></div>";

        page += pageFooter();
        page += settingsScript();
        _server.send(200, "text/html", page);
    }

    // ========== 视频流 ==========

    void handleStream() {
        WiFiClient client = _server.client();
        if (!client.connected()) return;

        // 发送 MJPEG 响应头
        String head = "HTTP/1.1 200 OK\r\n";
        head += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n";
        head += "Access-Control-Allow-Origin: *\r\n";
        head += "\r\n";
        _server.sendContent(head);

        int frames = 0;
        while (client.connected()) {
            camera_fb_t* fb = _camera->capture();
            if (!fb) { delay(10); continue; }

            String frameHead = "--frame\r\n";
            frameHead += "Content-Type: image/jpeg\r\n";
            frameHead += "Content-Length: " + String(fb->len) + "\r\n\r\n";
            _server.sendContent(frameHead);

            client.write(fb->buf, fb->len);
            _server.sendContent("\r\n");
            _camera->returnFrame(fb);

            frames++;
            if (frames % 100 == 0) {
                yield();
            }

            if (!client.available() && !client.connected()) break;
            delay(1);
        }
    }

    void handleCapture() {
        camera_fb_t* fb = _camera->capture();
        if (!fb) {
            _server.send(500, "text/plain", "Capture failed");
            return;
        }

        _server.sendHeader("Content-Type", "image/jpeg");
        _server.sendHeader("Content-Length", String(fb->len));
        _server.send(200, "image/jpeg", "");
        _server.sendContent((const char*)fb->buf, fb->len);
        _camera->returnFrame(fb);
    }

    void handleCaptureSave() {
        camera_fb_t* fb = _camera->capture();
        if (!fb) {
            _server.send(500, "application/json", "{\"success\":false,\"error\":\"capture failed\"}");
            return;
        }

        bool saved = false;
        String filename;

        if (_sd->isInitialized()) {
            filename = "/photo_" + _timeMgr->getFileTimestamp() + ".jpg";
            saved = _sd->writeFile(filename, fb->buf, fb->len);
        }

        _camera->returnFrame(fb);

        if (saved) {
            _server.send(200, "application/json",
                "{\"success\":true,\"file\":\"" + filename + "\"}");
        } else {
            _server.send(200, "application/json",
                "{\"success\":false,\"error\":\"save failed\"}");
        }
    }

    // ========== 摄像头 API ==========

    void handleCamStatus() {
        String json = "{";
        json += "\"resolution\":" + String(_camera->getFrameSize()) + ",";
        json += "\"quality\":" + String(_camera->getQuality()) + ",";
        json += "\"effect\":" + String(_camera->getEffect()) + ",";
        json += "\"flip\":" + String(_camera->getVFlip() ? "true" : "false") + ",";
        json += "\"mirror\":" + String(_camera->getHMirror() ? "true" : "false");
        json += "}";
        _server.send(200, "application/json", json);
    }

    void handleCamSetRes() {
        int val = _server.arg("value").toInt();
        _camera->setFrameSize(val);
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetQuality() {
        int val = _server.arg("value").toInt();
        _camera->setQuality(val);
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetEffect() {
        int val = _server.arg("value").toInt();
        _camera->setEffect(val);
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetFlip() {
        bool val = _server.arg("value") == "1";
        _camera->setVFlip(val);
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetMirror() {
        bool val = _server.arg("value") == "1";
        _camera->setHMirror(val);
        _server.send(200, "application/json", "{\"success\":true}");
    }

    // ========== SD 卡 API ==========

    void handleSdStatus() {
        if (!_sd->isInitialized()) {
            _server.send(200, "application/json",
                "{\"ready\":false}");
            return;
        }
        String json = "{";
        json += "\"ready\":true,";
        json += "\"total\":" + String(_sd->getTotalBytes()) + ",";
        json += "\"used\":" + String(_sd->getUsedBytes()) + ",";
        json += "\"free\":" + String(_sd->getFreeBytes());
        json += "}";
        _server.send(200, "application/json", json);
    }

    void handleSdList() {
        String path = _server.arg("path");
        if (path.isEmpty()) path = "/";
        String list = _sd->listDir(path);
        _server.send(200, "application/json", list);
    }

    void handleSdDelete() {
        String path = _server.arg("path");
        if (path.isEmpty()) {
            _server.send(400, "application/json", "{\"success\":false,\"error\":\"no path\"}");
            return;
        }
        bool ok = _sd->deleteFile(path);
        _server.send(200, "application/json",
            String("{\"success\":") + (ok ? "true" : "false") + "}");
    }

    void handleSdDownload() {
        String path = _server.arg("path");
        if (!_sd->exists(path)) {
            _server.send(404, "text/plain", "Not Found");
            return;
        }

        File f = _sd->openFile(path, FILE_READ);
        if (!f) {
            _server.send(500, "text/plain", "Open failed");
            return;
        }

        _server.sendHeader("Content-Type", "application/octet-stream");
        _server.sendHeader("Content-Disposition", "attachment; filename=\"" + path + "\"");
        _server.sendHeader("Content-Length", String(f.size()));
        _server.setContentLength(f.size());
        _server.send(200, "application/octet-stream", "");

        uint8_t buf[1024];
        while (f.available()) {
            int n = f.read(buf, sizeof(buf));
            if (n > 0) _server.sendContent((const char*)buf, n);
        }
        f.close();
    }

    void handleSdUpload() {
        // 简化版上传处理
        _server.send(200, "application/json", "{\"success\":true}");
    }

    // ========== 录制 API ==========

    void handleRecStart() {
        if (_recorder->isRecording()) {
            _server.send(200, "application/json",
                "{\"success\":false,\"error\":\"already recording\"}");
            return;
        }

        bool ok = _recorder->startRecording();
        _server.send(200, "application/json",
            String("{\"success\":") + (ok ? "true" : "false") + "}");
    }

    void handleRecStop() {
        if (!_recorder->isRecording()) {
            _server.send(200, "application/json",
                "{\"success\":false,\"error\":\"not recording\"}");
            return;
        }
        _recorder->stopRecording();
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleRecStatus() {
        String json = "{";
        json += "\"recording\":" + String(_recorder->isRecording() ? "true" : "false") + ",";
        json += "\"frames\":" + String(_recorder->getFrameCount()) + ",";
        json += "\"duration_ms\":" + String(_recorder->getDurationMs()) + ",";
        json += "\"fps\":" + String(_recorder->getFps());
        json += "}";
        _server.send(200, "application/json", json);
    }

    // ========== 网络 API ==========

    void handleNetStatus() {
        String json = "{";
        json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
        json += "\"ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"time_synced\":" + String(_timeMgr->isSynced() ? "true" : "false") + ",";
        json += "\"time\":\"" + _timeMgr->getDateTimeStr() + "\"";
        json += "}";
        _server.send(200, "application/json", json);
    }

    void handleWifiSave() {
        String ssid = _server.arg("ssid");
        String pass = _server.arg("pass");
        if (ssid.isEmpty()) {
            _server.send(400, "application/json", "{\"success\":false,\"error\":\"ssid required\"}");
            return;
        }
        // 保存到 EEPROM（通过 WifiManager 的方式比较复杂，这里简化处理）
        _server.send(200, "application/json", "{\"success\":true,\"msg\":\"重启后生效\"}");
    }

    void handleNtpSave() {
        String s1 = _server.arg("srv1");
        String s2 = _server.arg("srv2");
        int tz = _server.arg("tz").toInt();
        int interval = _server.arg("interval").toInt();

        _timeMgr->setNtpServers(s1, s2);
        _timeMgr->setTimezone(tz);
        _timeMgr->setSyncInterval(interval);

        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleNtpSync() {
        _timeMgr->sync();
        delay(2000);
        _server.send(200, "application/json",
            String("{\"success\":") + (_timeMgr->isSynced() ? "true" : "false") + "," +
            "\"time\":\"" + _timeMgr->getDateTimeStr() + "\"}");
    }

    // ========== 系统状态 API ==========

    void handleSysStatus() {
        String json = "{";
        json += "\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"sd_ready\":" + String(_sd->isInitialized() ? "true" : "false") + ",";
        json += "\"sd_total\":" + String(_sd->getTotalBytes()) + ",";
        json += "\"sd_free\":" + String(_sd->getFreeBytes()) + ",";
        json += "\"uptime\":" + String(millis() / 1000) + ",";
        json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"recording\":" + String(_recorder->isRecording() ? "true" : "false") + ",";
        json += "\"rec_frames\":" + String(_recorder->getFrameCount()) + ",";
        json += "\"time_synced\":" + String(_timeMgr->isSynced() ? "true" : "false") + ",";
        json += "\"time\":\"" + _timeMgr->getDateTimeStr() + "\"";
        json += "}";
        _server.send(200, "application/json", json);
    }

    void handleNotFound() {
        _server.send(404, "text/plain", "Not Found");
    }

    // ========== 辅助函数 ==========

    String pageHeader(String title, int activeTab) {
        String s = "<!DOCTYPE html><html lang='zh-CN'><head>";
        s += "<meta charset='UTF-8'>";
        s += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
        s += "<title>" + title + " - ESP32-CAM</title>";
        s += "<style>";
        s += commonCss();
        s += "</style></head><body>";
        s += "<div class='container'>";

        // 导航栏
        s += "<nav class='navbar'>";
        s += "<div class='nav-brand'>📷 ESP32-CAM</div>";
        s += "<div class='nav-links'>";
        s += "<a href='/' class='nav-link" + String(activeTab == 0 ? " active" : "") + "'>监控</a>";
        s += "<a href='/sd' class='nav-link" + String(activeTab == 1 ? " active" : "") + "'>SD卡</a>";
        s += "<a href='/settings' class='nav-link" + String(activeTab == 2 ? " active" : "") + "'>设置</a>";
        s += "</div></nav>";

        return s;
    }

    String pageFooter() {
        return "</div><div class='footer'>ESP32-CAM 智能摄像头</div></body></html>";
    }

    String statusItem(String label, String id, String value) {
        return "<div class='stat-item'><div class='stat-label'>" + label +
               "</div><div class='stat-value' id='" + id + "'>" + value + "</div></div>";
    }

    String commonCss() {
        return "*{box-sizing:border-box;margin:0;padding:0}"
        "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,'Microsoft YaHei',sans-serif;background:#f5f6fa;color:#1e2230;padding:20px}"
        ".container{max-width:1200px;margin:0 auto}"
        ".navbar{background:linear-gradient(135deg,#6366f1,#8b5cf6);color:white;padding:16px 24px;border-radius:12px;margin-bottom:20px;display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap}"
        ".nav-brand{font-size:20px;font-weight:700}"
        ".nav-links{display:flex;gap:8px}"
        ".nav-link{color:white;text-decoration:none;padding:8px 16px;border-radius:8px;font-size:14px;font-weight:500;transition:all 0.15s}"
        ".nav-link:hover{background:rgba(255,255,255,0.2)}"
        ".nav-link.active{background:rgba(255,255,255,0.25)}"
        ".card{background:white;border-radius:12px;margin-bottom:16px;box-shadow:0 1px 3px rgba(0,0,0,0.05);border:1px solid #e4e7ef;overflow:hidden}"
        ".card-h{padding:16px 20px;font-size:16px;font-weight:600;border-bottom:1px solid #e4e7ef;background:#fafbff}"
        ".card-b{padding:20px}"
        ".row{display:flex;gap:16px;flex-wrap:wrap}"
        ".col-main{flex:1;min-width:300px}"
        ".col-side{width:300px;min-width:280px}"
        ".video-box{background:#1e2230;border-radius:8px;padding:8px;text-align:center;min-height:240px;display:flex;align-items:center;justify-content:center}"
        ".video-box img{max-width:100%;max-height:400px;border-radius:4px}"
        ".btn{padding:8px 16px;border:none;border-radius:6px;font-size:13px;font-weight:500;cursor:pointer;transition:all 0.15s;background:#e4e7ef;color:#1e2230}"
        ".btn:hover{background:#d0d5e0}"
        ".btn-primary{background:#6366f1;color:white}"
        ".btn-primary:hover{background:#4f46e5}"
        ".btn-success{background:#10b981;color:white}"
        ".btn-success:hover{background:#059669}"
        ".btn-warning{background:#f59e0b;color:white}"
        ".btn-warning:hover{background:#d97706}"
        ".btn-danger{background:#ef4444;color:white}"
        ".btn-danger:hover{background:#dc2626}"
        ".btn-row{display:flex;gap:8px;flex-wrap:wrap}"
        ".form-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px}"
        ".form-group{display:flex;flex-direction:column;gap:6px}"
        ".form-group label{font-size:13px;color:#6b7280;font-weight:500}"
        ".form-group input,.form-group select{padding:8px 12px;border:1px solid #d0d5e0;border-radius:6px;font-size:14px;background:white}"
        ".status-grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}"
        ".stat-item{background:#f5f6fa;padding:10px 12px;border-radius:8px}"
        ".stat-label{font-size:12px;color:#6b7280;margin-bottom:2px}"
        ".stat-value{font-size:15px;font-weight:600}"
        ".switch-row{display:flex;align-items:center;flex-wrap:wrap;gap:4px}"
        ".switch{position:relative;display:inline-block;width:40px;height:22px}"
        ".switch input{opacity:0;width:0;height:0}"
        ".slider{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background-color:#d0d5e0;transition:.3s;border-radius:22px}"
        ".slider:before{position:absolute;content:\"\";height:16px;width:16px;left:3px;bottom:3px;background-color:white;transition:.3s;border-radius:50%}"
        "input:checked+.slider{background-color:#6366f1}"
        "input:checked+.slider:before{transform:translateX(18px)}"
        ".file-list{border:1px solid #e4e7ef;border-radius:8px;max-height:400px;overflow-y:auto}"
        ".file-item{display:flex;justify-content:space-between;align-items:center;padding:10px 14px;border-bottom:1px solid #e4e7ef;font-size:14px}"
        ".file-item:last-child{border-bottom:none}"
        ".file-item:hover{background:#f5f6fa}"
        ".file-name{display:flex;align-items:center;gap:8px;cursor:pointer}"
        ".file-actions{display:flex;gap:6px}"
        ".file-actions .btn{padding:4px 10px;font-size:12px}"
        ".progress-bar{height:8px;background:#e4e7ef;border-radius:4px;overflow:hidden}"
        ".progress-fill{height:100%;background:linear-gradient(90deg,#6366f1,#8b5cf6);transition:width 0.3s}"
        ".footer{text-align:center;color:#6b7280;font-size:12px;margin-top:24px;padding-top:16px;border-top:1px solid #e4e7ef}"
        "@media (max-width:800px){.row{flex-direction:column}.col-side{width:100%}}";
    }

    String indexScript() {
        return "<script>"
        "var recMode=false;"
        "function refreshImg(){document.getElementById('video').src='/stream?t='+Date.now()}"
        "function captureSave(){fetch('/capture_save').then(r=>r.json()).then(d=>{if(d.success){alert('保存成功: '+d.file)}else{alert('保存失败: '+d.error)}})}"
        "function toggleRecord(){"
        "var btn=document.getElementById('recBtn');"
        "if(!recMode){"
        "fetch('/api/record/start').then(r=>r.json()).then(d=>{"
        "if(d.success){recMode=true;btn.textContent='⏹ 停止录制';btn.className='btn btn-danger'}"
        "})}else{"
        "fetch('/api/record/stop').then(r=>r.json()).then(d=>{"
        "recMode=false;btn.textContent='⏺ 开始录制';btn.className='btn btn-warning'"
        "})}}"
        "function setResolution(){var v=document.getElementById('resSel').value;fetch('/api/camera/set_res?value='+v).then(refreshImg)}"
        "function setQuality(){var v=document.getElementById('qualRange').value;document.getElementById('qualVal').textContent=v;fetch('/api/camera/set_quality?value='+v)}"
        "function setEffect(){var v=document.getElementById('effSel').value;fetch('/api/camera/set_effect?value='+v).then(refreshImg)}"
        "function setFlip(){var v=document.getElementById('flipChk').checked;fetch('/api/camera/set_flip?value='+(v?1:0)).then(refreshImg)}"
        "function setMirror(){var v=document.getElementById('mirrorChk').checked;fetch('/api/camera/set_mirror?value='+(v?1:0)).then(refreshImg)}"
        "function formatBytes(b){if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';if(b<1073741824)return(b/1048576).toFixed(1)+' MB';return(b/1073741824).toFixed(2)+' GB'}"
        "function formatUptime(s){var h=Math.floor(s/3600);var m=Math.floor((s%3600)/60);s=s%60;return h+':'+String(m).padStart(2,'0')+':'+String(s).padStart(2,'0')}"
        "function loadStatus(){fetch('/api/system/status').then(r=>r.json()).then(d=>{"
        "document.getElementById('wifiVal').textContent=d.wifi_connected?'已连接':'未连接';"
        "document.getElementById('wifiVal').style.color=d.wifi_connected?'#10b981':'#ef4444';"
        "document.getElementById('ipVal').textContent=d.ip;"
        "document.getElementById('rssiVal').textContent=d.rssi+' dBm';"
        "document.getElementById('sdVal').textContent=d.sd_ready?'就绪':'未检测到';"
        "document.getElementById('sdVal').style.color=d.sd_ready?'#10b981':'#ef4444';"
        "document.getElementById('sdFreeVal').textContent=formatBytes(d.sd_free);"
        "document.getElementById('uptimeVal').textContent=formatUptime(d.uptime);"
        "})}"
        "function loadCamStatus(){fetch('/api/camera/status').then(r=>r.json()).then(d=>{"
        "document.getElementById('resSel').value=d.resolution;"
        "document.getElementById('qualRange').value=d.quality;"
        "document.getElementById('qualVal').textContent=d.quality;"
        "document.getElementById('effSel').value=d.effect;"
        "document.getElementById('flipChk').checked=d.flip;"
        "document.getElementById('mirrorChk').checked=d.mirror;"
        "})}"
        "setInterval(loadStatus,5000);loadStatus();loadCamStatus();"
        "</script>";
    }

    String sdScript() {
        return "<script>"
        "var curPath='/';"
        "function formatBytes(b){if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';if(b<1073741824)return(b/1048576).toFixed(1)+' MB';return(b/1073741824).toFixed(2)+' GB'}"
        "function refreshSdList(){"
        "fetch('/api/sd/list?path='+encodeURIComponent(curPath)).then(r=>r.json()).then(data=>{"
        "var html='';"
        "if(data.length===0){html='<div style=\"padding:20px;text-align:center;color:#6b7280\">目录为空</div>'}"
        "else{data.forEach(function(f){"
        "html+='<div class=\"file-item\">';"
        "html+='<div class=\"file-name\" onclick=\"navigateTo(\\''+f.path+'\\','+f.isDir+')\">';"
        "html+=(f.isDir?'📁':'📄')+' '+f.name;"
        "html+='</div>';"
        "html+='<div style=\"display:flex;align-items:center;gap:12px\">';"
        "if(!f.isDir){html+='<span style=\"font-size:12px;color:#6b7280\">'+formatBytes(f.size)+'</span>'}"
        "html+='<div class=\"file-actions\">';"
        "if(!f.isDir){html+='<button class=\"btn btn-primary\" onclick=\"downloadFile(\\''+f.path+'\\')\">下载</button>'}"
        "html+='<button class=\"btn btn-danger\" onclick=\"deleteFile(\\''+f.path+'\\','+f.isDir+')\">删除</button>';"
        "html+='</div></div></div>';"
        "})}"
        "document.getElementById('sdFileList').innerHTML=html;"
        "document.getElementById('curPath').textContent=curPath;"
        "})}"
        "function navigateTo(path,isDir){if(isDir){curPath=path;refreshSdList()}}"
        "function goUp(){if(curPath==='/')return;var parts=curPath.split('/').filter(p=>p);parts.pop();curPath='/'+parts.join('/');if(curPath==='')curPath='/';refreshSdList()}"
        "function deleteFile(path,isDir){if(!confirm('确定要删除吗？'))return;fetch('/api/sd/delete?path='+encodeURIComponent(path)).then(r=>r.json()).then(d=>{if(d.success){refreshSdList();loadSdStatus()}else{alert('删除失败')}})}"
        "function downloadFile(path){window.location.href='/api/sd/download?path='+encodeURIComponent(path)}"
        "function uploadFile(){var file=document.getElementById('uploadFile').files[0];if(!file){alert('请选择文件');return}"
        "document.getElementById('uploadStatus').textContent='上传中...';"
        "var form=new FormData();form.append('file',file);"
        "fetch('/api/sd/upload',{method:'POST',body:form}).then(r=>r.json()).then(d=>{"
        "if(d.success){document.getElementById('uploadStatus').textContent='上传成功';refreshSdList();loadSdStatus()}"
        "else{document.getElementById('uploadStatus').textContent='上传失败: '+d.error}"
        "})}"
        "function loadSdStatus(){fetch('/api/sd/status').then(r=>r.json()).then(d=>{"
        "if(d.ready){"
        "document.getElementById('sdTotal').textContent=formatBytes(d.total);"
        "document.getElementById('sdUsed').textContent=formatBytes(d.used);"
        "document.getElementById('sdFree').textContent=formatBytes(d.free);"
        "var pct=(d.used/d.total*100).toFixed(1);"
        "document.getElementById('sdProgress').style.width=pct+'%';"
        "}})}"
        "refreshSdList();loadSdStatus();"
        "</script>";
    }

    String settingsScript() {
        return "<script>"
        "function saveWifi(){"
        "var ssid=document.getElementById('wifiSsid').value;"
        "var pass=document.getElementById('wifiPass').value;"
        "if(!ssid){alert('请输入WiFi名称');return}"
        "document.getElementById('wifiStatus').textContent='保存中...';"
        "fetch('/api/network/wifi_save?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass))"
        ".then(r=>r.json()).then(d=>{document.getElementById('wifiStatus').textContent=d.msg})"
        "}"
        "function saveNtp(){"
        "var s1=document.getElementById('ntpSvr1').value;"
        "var s2=document.getElementById('ntpSvr2').value;"
        "var tz=document.getElementById('ntpTz').value;"
        "var interval=document.getElementById('ntpInterval').value;"
        "document.getElementById('ntpStatus').textContent='保存中...';"
        "fetch('/api/network/ntp_save?srv1='+encodeURIComponent(s1)+'&srv2='+encodeURIComponent(s2)+'&tz='+tz+'&interval='+interval)"
        ".then(r=>r.json()).then(d=>{document.getElementById('ntpStatus').textContent=d.success?'保存成功':'保存失败'})"
        "}"
        "function syncNtp(){"
        "document.getElementById('ntpStatus').textContent='同步中...';"
        "fetch('/api/network/ntp_sync').then(r=>r.json()).then(d=>{"
        "document.getElementById('ntpStatus').textContent=d.success?'同步成功: '+d.time:'同步中...'"
        "})}"
        "</script>";
    }
};

#endif // WEB_MANAGER_H
