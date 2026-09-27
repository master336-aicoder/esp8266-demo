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
        _recorder(rec), _wifi(wifi), _timeMgr(timeMgr), _ledOn(false) {}

    void begin() {
        pinMode(LED_PIN, OUTPUT);
        digitalWrite(LED_PIN, LED_OFF);

        _server.on("/", std::bind(&WebManager::handleIndex, this));
        _server.on("/stream", std::bind(&WebManager::handleStream, this));
        _server.on("/capture", std::bind(&WebManager::handleCapture, this));
        _server.on("/capture_save", std::bind(&WebManager::handleCaptureSave, this));

        _server.on("/api/camera/status", std::bind(&WebManager::handleCamStatus, this));
        _server.on("/api/camera/set_res", std::bind(&WebManager::handleCamSetRes, this));
        _server.on("/api/camera/set_quality", std::bind(&WebManager::handleCamSetQuality, this));
        _server.on("/api/camera/set_effect", std::bind(&WebManager::handleCamSetEffect, this));
        _server.on("/api/camera/set_flip", std::bind(&WebManager::handleCamSetFlip, this));
        _server.on("/api/camera/set_mirror", std::bind(&WebManager::handleCamSetMirror, this));

        _server.on("/sd", std::bind(&WebManager::handleSdPage, this));
        _server.on("/api/sd/status", std::bind(&WebManager::handleSdStatus, this));
        _server.on("/api/sd/list", std::bind(&WebManager::handleSdList, this));
        _server.on("/api/sd/delete", std::bind(&WebManager::handleSdDelete, this));
        _server.on("/api/sd/download", std::bind(&WebManager::handleSdDownload, this));
        _server.on("/api/sd/format", std::bind(&WebManager::handleSdFormat, this));
        _server.on("/api/sd/upload", HTTP_POST,
            std::bind(&WebManager::handleSdUpload, this));

        _server.on("/api/record/start", std::bind(&WebManager::handleRecStart, this));
        _server.on("/api/record/stop", std::bind(&WebManager::handleRecStop, this));
        _server.on("/api/record/status", std::bind(&WebManager::handleRecStatus, this));

        _server.on("/settings", std::bind(&WebManager::handleSettings, this));
        _server.on("/api/network/status", std::bind(&WebManager::handleNetStatus, this));
        _server.on("/api/network/wifi_save", std::bind(&WebManager::handleWifiSave, this));
        _server.on("/api/network/ntp_save", std::bind(&WebManager::handleNtpSave, this));
        _server.on("/api/network/ntp_sync", std::bind(&WebManager::handleNtpSync, this));

        _server.on("/api/led/status", std::bind(&WebManager::handleLedStatus, this));
        _server.on("/api/led/on", std::bind(&WebManager::handleLedOn, this));
        _server.on("/api/led/off", std::bind(&WebManager::handleLedOff, this));

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
    bool _ledOn;

    // ========== CSS ==========

    void sendCss() {
        _server.sendContent(F(
            "*{margin:0;padding:0;box-sizing:border-box}"
            "body{font-family:system-ui,sans-serif;background:#f0f0f0;padding:16px}"
            ".nb{background:#6366f1;color:#fff;padding:12px 20px;border-radius:8px;margin-bottom:16px;display:flex;justify-content:space-between;align-items:center}"
            ".nb a{color:#fff;text-decoration:none;margin-left:12px;font-size:14px}"
            ".c{background:#fff;border-radius:8px;margin-bottom:12px;padding:16px;border:1px solid #ddd}"
            ".r{display:flex;gap:12px;flex-wrap:wrap}"
            ".cm{flex:1;min-width:280px}"
            ".cs{width:260px}"
            ".vb{background:#222;border-radius:6px;padding:6px;text-align:center;min-height:200px;display:flex;align-items:center;justify-content:center}"
            ".vb img{max-width:100%;max-height:380px;border-radius:4px}"
            "button{padding:8px 14px;border:none;border-radius:4px;cursor:pointer;background:#ddd;margin-right:6px;margin-top:8px;font-size:13px}"
            ".p{background:#6366f1;color:#fff}.s{background:#10b981;color:#fff}.w{background:#f59e0b;color:#fff}.d{background:#ef4444;color:#fff}"
            "input,select{padding:6px 10px;border:1px solid #ccc;border-radius:4px;margin:4px 0;font-size:13px}"
            ".sg{display:grid;grid-template-columns:repeat(2,1fr);gap:8px}"
            ".si{background:#f0f0f0;padding:8px;border-radius:6px}"
            ".sl{font-size:11px;color:#666}.sv{font-size:14px;font-weight:600}"
            ".fg{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px;margin-bottom:8px}"
            ".fl{border:1px solid #ddd;border-radius:6px;max-height:400px;overflow-y:auto;margin-top:8px}"
            ".fi{display:flex;justify-content:space-between;align-items:center;padding:8px 12px;border-bottom:1px solid #eee;font-size:13px}"
            "@media(max-width:768px){.r{flex-direction:column}.cs{width:100%}}"
        ));
    }

    void sendNav() {
        _server.sendContent(F(
            "<div class='nb'><b>ESP32-CAM</b><div>"
            "<a href='/'>监控</a>"
            "<a href='/sd'>SD卡</a>"
            "<a href='/settings'>设置</a>"
            "</div></div>"
        ));
    }

    // ========== 首页 ==========

    void handleIndex() {
        // 使用sendContent流式发送避免String过大截断
        _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        _server.send(200, "text/html; charset=utf-8", "");

        _server.sendContent(F(
            "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>ESP32-CAM 监控</title><style>"
        ));
        sendCss();
        _server.sendContent(F("</style></head><body>"));
        sendNav();

        // 左侧：实时画面
        _server.sendContent(F(
            "<div class='r'><div class='cm'>"
            "<div class='c'><b>实时画面</b>"
            "<div class='vb'><img id='v' style='max-width:100%;max-height:380px;border-radius:4px'></div>"
            "<button class='p' onclick='ri()'>刷新</button>"
            "<button class='s' onclick='cs()'>拍照</button>"
            "<button class='w' id='rb' onclick='tr()'>录制</button>"
            "<button id='sb' onclick='ts()'>直播</button>"
            "</div></div>"
        ));

        // 右侧：状态
        _server.sendContent(F(
            "<div class='cs'>"
            "<div class='c'><b>系统状态</b><div class='sg'>"
            "<div class='si'><div class='sl'>WiFi</div><div class='sv' id='wf'>-</div></div>"
            "<div class='si'><div class='sl'>IP地址</div><div class='sv' id='ip'>-</div></div>"
            "<div class='si'><div class='sl'>信号强度</div><div class='sv' id='rs'>-</div></div>"
            "<div class='si'><div class='sl'>运行时间</div><div class='sv' id='up'>-</div></div>"
            "<div class='si'><div class='sl'>内存</div><div class='sv' id='mh'>-</div></div>"
            "<div class='si'><div class='sl'>时间</div><div class='sv' id='tm' style='font-size:12px'>-</div></div>"
            "</div></div>"
        ));

        // LED控制
        _server.sendContent(F(
            "<div class='c'><b>闪光灯</b><div style='margin-top:8px'>"
            "<button class='p' id='lb' onclick='tl()'>开灯</button>"
            "</div></div>"
        ));

        _server.sendContent(F("</div></div>")); // 关闭右侧和row

        // 摄像头设置
        _server.sendContent(F(
            "<div class='c'><b>摄像头设置</b><div class='fg'>"
            "<div><label>分辨率</label><br><select id='r1' onchange='sr()'>"
        ));
        for (int i = 0; i < 13; i++) {
            String opt = "<option value='" + String(i) + "'>" + CameraManager::getResolutionName(i) + "</option>";
            _server.sendContent(opt);
        }
        _server.sendContent(F("</select></div>"));
        _server.sendContent(F(
            "<div><label>JPEG质量 <span id='qv'>15</span></label><br>"
            "<input type='range' id='qr' min='4' max='63' oninput='sq()'></div>"
        ));
        _server.sendContent(F("<div><label>特效</label><br><select id='e1' onchange='se()'>"));
        for (int i = 0; i < 15; i++) {
            String opt = "<option value='" + String(i) + "'>" + CameraManager::getEffectName(i) + "</option>";
            _server.sendContent(opt);
        }
        _server.sendContent(F(
            "</select></div>"
            "<div><label>翻转</label><br>"
            "<input type='checkbox' id='f1' onchange='sf()'> 垂直 "
            "<input type='checkbox' id='m1' onchange='sm()'> 水平</div>"
            "</div></div>"
        ));

        // JavaScript
        _server.sendContent(F(
            "<script>\n"
            "var rc=0,st=0,si=null,led=0;\n"
            "function ri(){var im=document.getElementById('v');im.src='/capture?t='+Date.now()}\n"
            "function cs(){fetch('/capture_save').then(r=>r.json()).then(d=>alert(d.success?'已保存:'+d.file:'保存失败')).catch(function(){})}\n"
            "function tr(){var b=document.getElementById('rb');if(!rc){fetch('/api/record/start').then(r=>r.json()).then(d=>{if(d.success){rc=1;b.textContent='停止';b.className='d'}}).catch(function(){})}else{fetch('/api/record/stop').then(r=>r.json()).then(d=>{rc=0;b.textContent='录制';b.className='w'}).catch(function(){})}}\n"
            "function ts(){var b=document.getElementById('sb');var im=document.getElementById('v');if(!st){st=1;b.textContent='停止';b.className='d';si=setInterval(function(){im.src='/capture?t='+Date.now()},800)}else{st=0;b.textContent='直播';b.className='';clearInterval(si);si=null}}\n"
            "function sr(){fetch('/api/camera/set_res?value='+document.getElementById('r1').value).then(ri).catch(function(){})}\n"
            "function sq(){var v=document.getElementById('qr').value;document.getElementById('qv').textContent=v;fetch('/api/camera/set_quality?value='+v).catch(function(){})}\n"
            "function se(){fetch('/api/camera/set_effect?value='+document.getElementById('e1').value).then(ri).catch(function(){})}\n"
            "function sf(){fetch('/api/camera/set_flip?value='+(document.getElementById('f1').checked?1:0)).then(ri).catch(function(){})}\n"
            "function sm(){fetch('/api/camera/set_mirror?value='+(document.getElementById('m1').checked?1:0)).then(ri).catch(function(){})}\n"
            "function tl(){var b=document.getElementById('lb');if(!led){fetch('/api/led/on').then(r=>r.json()).then(d=>{if(d.success){led=1;b.textContent='关灯';b.className='d'}}).catch(function(){})}else{fetch('/api/led/off').then(r=>r.json()).then(d=>{if(d.success){led=0;b.textContent='开灯';b.className='p'}}).catch(function(){})}}\n"
            "function fb(b){if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';return(b/1048576).toFixed(1)+' MB'}\n"
            "setTimeout(function(){fetch('/api/system/status').then(r=>r.json()).then(d=>{document.getElementById('wf').textContent=d.wifi_connected?'已连接':'未连接';document.getElementById('ip').textContent=d.ip;document.getElementById('rs').textContent=d.rssi+'dBm';document.getElementById('up').textContent=d.uptime+'s';document.getElementById('mh').textContent=Math.round(d.free_heap/1024)+'KB';document.getElementById('tm').textContent=d.time||'未同步'}).catch(function(){})},1000)\n"
            "setTimeout(function(){fetch('/api/camera/status').then(r=>r.json()).then(d=>{document.getElementById('r1').value=d.resolution;document.getElementById('qr').value=d.quality;document.getElementById('qv').textContent=d.quality;document.getElementById('e1').value=d.effect;document.getElementById('f1').checked=d.flip;document.getElementById('m1').checked=d.mirror}).catch(function(){})},2000)\n"
            "setTimeout(function(){fetch('/api/led/status').then(r=>r.json()).then(d=>{if(d.led_on){led=1;var b=document.getElementById('lb');b.textContent='关灯';b.className='d'}}).catch(function(){})},2500)\n"
            "setTimeout(function(){ri()},3000)\n"
            "</script></body></html>"
        ));
    }

    // ========== SD卡页面 ==========

    void handleSdPage() {
        _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        _server.send(200, "text/html; charset=utf-8", "");

        _server.sendContent(F(
            "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>SD卡 - ESP32-CAM</title><style>"
        ));
        sendCss();
        _server.sendContent(F("</style></head><body>"));
        sendNav();

        _server.sendContent(F(
            "<div class='r'><div class='cm'>"
            "<div class='c'><b>文件列表</b>"
            "<div style='margin-top:8px'>"
            "<button class='p' onclick='rl()'>刷新</button>"
            "<button onclick='up()'>上级</button>"
            "<span id='cp' style='margin-left:12px'>/</span>"
            "</div>"
            "<div id='fl' class='fl'>加载中...</div>"
            "</div></div>"
        ));

        _server.sendContent(F(
            "<div class='cs'>"
            "<div class='c'><b>空间</b><div class='sg'>"
            "<div class='si'><div class='sl'>总容量</div><div class='sv' id='st'>-</div></div>"
            "<div class='si'><div class='sl'>已用</div><div class='sv' id='su'>-</div></div>"
            "<div class='si'><div class='sl'>可用</div><div class='sv' id='sf'>-</div></div>"
            "</div></div>"
            "<div class='c'><b>操作</b><div style='margin-top:8px'>"
            "<button class='d' onclick='fmt()'>格式化</button>"
            "</div></div>"
            "</div></div>"
        ));

        _server.sendContent(F(
            "<script>\n"
            "var cp='/';\n"
            "function fb(b){if(b<1024)return b+' B';if(b<1048576)return(b/1024).toFixed(1)+' KB';return(b/1048576).toFixed(1)+' MB'}\n"
            "function rl(){fetch('/api/sd/list?path='+encodeURIComponent(cp)).then(r=>r.json()).then(d=>{var h='';if(!d.length){h='<div style=\"padding:20px;text-align:center;color:#999\">空目录</div>'}else{d.forEach(function(f){h+='<div class=\"fi\"><div onclick=\"nv(\\''+f.path+'\\','+f.isDir+')\" style=\"cursor:pointer\">'+(f.isDir?'[目录]':'[文件]')+' '+f.name+'</div><div>';if(!f.isDir)h+='<span style=\"font-size:11px;color:#999\">'+fb(f.size)+'</span>';h+=' <button class=\"d\" onclick=\"dl(\\''+f.path+'\\')\">删</button></div></div>'})}document.getElementById('fl').innerHTML=h;document.getElementById('cp').textContent=cp}).catch(function(){})}\n"
            "function nv(p,d){if(d){cp=p;rl()}}\n"
            "function up(){if(cp==='/')return;var a=cp.split('/').filter(x=>x);a.pop();cp='/'+a.join('/');if(cp==='')cp='/';rl()}\n"
            "function dl(p){if(!confirm('确认删除?'))return;fetch('/api/sd/delete?path='+encodeURIComponent(p)).then(r=>r.json()).then(rl).catch(function(){})}\n"
            "function fmt(){if(!confirm('格式化将删除所有数据!'))return;fetch('/api/sd/format').then(r=>r.json()).then(d=>{alert(d.success?'格式化成功':'格式化失败');rl();fetchSd()}).catch(function(){})}\n"
            "function fetchSd(){fetch('/api/sd/status').then(r=>r.json()).then(d=>{if(d.ready){document.getElementById('st').textContent=fb(d.total);document.getElementById('su').textContent=fb(d.used);document.getElementById('sf').textContent=fb(d.free)}else{document.getElementById('st').textContent='未检测到'}}).catch(function(){})}\n"
            "fetchSd();\n"
            "rl();\n"
            "</script></body></html>"
        ));
    }

    // ========== 设置页面 ==========

    void handleSettings() {
        _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        _server.send(200, "text/html; charset=utf-8", "");

        _server.sendContent(F(
            "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>设置 - ESP32-CAM</title><style>"
        ));
        sendCss();
        _server.sendContent(F("</style></head><body>"));
        sendNav();

        // WiFi设置
        String wifiSsid = _wifi->getSsid();
        _server.sendContent(F(
            "<div class='c'><b>WiFi设置</b>"
            "<div class='fg' style='margin-top:8px'>"
            "<div><label>WiFi名称</label><br>"
        ));
        _server.sendContent("<input type='text' id='ws' placeholder='当前: " + wifiSsid + "'></div>");
        _server.sendContent(F(
            "<div><label>密码</label><br>"
            "<input type='password' id='wp' placeholder='输入新密码'></div>"
            "</div>"
            "<button class='p' onclick='sw()'>保存</button>"
            "<div id='wfs' style='margin-top:8px;font-size:13px'></div>"
            "</div>"
        ));

        // NTP设置
        String ntp1 = _timeMgr->getNtpServer1();
        String ntp2 = _timeMgr->getNtpServer2();
        int tz = _timeMgr->getTimezone();
        int interval = _timeMgr->getSyncInterval();

        _server.sendContent(F(
            "<div class='c'><b>NTP时间同步</b>"
            "<div class='fg' style='margin-top:8px'>"
            "<div><label>NTP服务器1</label><br>"
        ));
        _server.sendContent("<input type='text' id='n1' value='" + ntp1 + "'></div>");
        _server.sendContent(F("<div><label>NTP服务器2</label><br>"));
        _server.sendContent("<input type='text' id='n2' value='" + ntp2 + "'></div>");

        // 时区
        _server.sendContent(F("<div><label>时区</label><br><select id='tz'>"));
        for (int i = -12; i <= 12; i++) {
            String opt = "<option value='" + String(i) + "'";
            if (i == tz) opt += " selected";
            opt += ">UTC" + String(i >= 0 ? "+" : "") + String(i) + "</option>";
            _server.sendContent(opt);
        }
        _server.sendContent(F("</select></div>"));

        // 同步间隔
        _server.sendContent(F(
            "<div><label>同步间隔(秒)</label><br>"
        ));
        _server.sendContent("<input type='number' id='ni' value='" + String(interval) + "' min='60' max='86400'></div>");
        _server.sendContent(F("</div>"));

        _server.sendContent(F(
            "<button class='p' onclick='sn()'>保存</button>"
            "<button class='s' onclick='syn()'>立即同步</button>"
            "<div id='nfs' style='margin-top:8px;font-size:13px'></div>"
            "</div>"
        ));

        _server.sendContent(F(
            "<script>\n"
            "function sw(){var s=document.getElementById('ws').value;if(!s){alert('请输入WiFi名称');return}document.getElementById('wfs').textContent='保存中...';fetch('/api/network/wifi_save?ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(document.getElementById('wp').value)).then(r=>r.json()).then(d=>document.getElementById('wfs').textContent=d.msg||'已保存').catch(function(){document.getElementById('wfs').textContent='保存失败'})}\n"
            "function sn(){document.getElementById('nfs').textContent='保存中...';fetch('/api/network/ntp_save?srv1='+encodeURIComponent(document.getElementById('n1').value)+'&srv2='+encodeURIComponent(document.getElementById('n2').value)+'&tz='+document.getElementById('tz').value+'&interval='+document.getElementById('ni').value).then(r=>r.json()).then(d=>document.getElementById('nfs').textContent=d.success?'已保存':'保存失败').catch(function(){document.getElementById('nfs').textContent='保存失败'})}\n"
            "function syn(){document.getElementById('nfs').textContent='同步中...';fetch('/api/network/ntp_sync').then(r=>r.json()).then(d=>document.getElementById('nfs').textContent=d.success?'已同步: '+d.time:'同步失败').catch(function(){document.getElementById('nfs').textContent='同步失败'})}\n"
            "</script></body></html>"
        ));
    }

    // ========== 视频流 ==========

    void handleStream() {
        WiFiClient client = _server.client();
        if (!client.connected()) return;

        _server.sendContent(F(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\n"
        ));

        int frames = 0;
        const int MAX_FRAMES = 150;
        while (client.connected() && frames < MAX_FRAMES) {
            camera_fb_t* fb = _camera->capture();
            if (!fb) { delay(20); continue; }

            char fh[80];
            snprintf(fh, sizeof(fh), "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
            _server.sendContent(fh);
            client.write(fb->buf, fb->len);
            _server.sendContent(F("\r\n"));
            _camera->returnFrame(fb);

            frames++;
            if (frames % 20 == 0) yield();
            delay(100);
        }
    }

    void handleCapture() {
        camera_fb_t* fb = _camera->capture();
        if (!fb) {
            _server.send(500, "text/plain", "Capture failed");
            return;
        }
        _server.sendHeader("Cache-Control", "no-cache");
        String img;
        img.reserve(fb->len);
        img.concat((const char*)fb->buf, fb->len);
        _server.send(200, "image/jpeg", img);
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
            _server.send(200, "application/json", "{\"success\":true,\"file\":\"" + filename + "\"}");
        } else {
            _server.send(200, "application/json", "{\"success\":false,\"error\":\"SD卡未就绪\"}");
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
        _camera->setFrameSize(_server.arg("value").toInt());
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetQuality() {
        _camera->setQuality(_server.arg("value").toInt());
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetEffect() {
        _camera->setEffect(_server.arg("value").toInt());
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetFlip() {
        _camera->setVFlip(_server.arg("value") == "1");
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleCamSetMirror() {
        _camera->setHMirror(_server.arg("value") == "1");
        _server.send(200, "application/json", "{\"success\":true}");
    }

    // ========== SD卡 API ==========

    void handleSdStatus() {
        if (!_sd->isInitialized()) {
            _server.send(200, "application/json", "{\"ready\":false}");
            return;
        }
        String json = "{\"ready\":true,\"total\":" + String(_sd->getTotalBytes()) +
            ",\"used\":" + String(_sd->getUsedBytes()) +
            ",\"free\":" + String(_sd->getFreeBytes()) + "}";
        _server.send(200, "application/json", json);
    }

    void handleSdList() {
        String path = _server.arg("path");
        if (path.isEmpty()) path = "/";
        _server.send(200, "application/json", _sd->listDir(path));
    }

    void handleSdDelete() {
        String path = _server.arg("path");
        if (path.isEmpty()) {
            _server.send(400, "application/json", "{\"success\":false}");
            return;
        }
        bool ok = _sd->deleteFile(path);
        _server.send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + "}");
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
        _server.send(200, "application/octet-stream", "");
        uint8_t buf[1024];
        while (f.available()) {
            int n = f.read(buf, sizeof(buf));
            if (n > 0) _server.sendContent((const char*)buf, n);
        }
        f.close();
    }

    void handleSdFormat() {
        bool ok = _sd->format();
        _server.send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + "}");
    }

    void handleSdUpload() {
        _server.send(200, "application/json", "{\"success\":true}");
    }

    // ========== 录制 API ==========

    void handleRecStart() {
        if (_recorder->isRecording()) {
            _server.send(200, "application/json", "{\"success\":false}");
            return;
        }
        bool ok = _recorder->startRecording();
        _server.send(200, "application/json", String("{\"success\":") + (ok ? "true" : "false") + "}");
    }

    void handleRecStop() {
        if (!_recorder->isRecording()) {
            _server.send(200, "application/json", "{\"success\":false}");
            return;
        }
        _recorder->stopRecording();
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleRecStatus() {
        String json = "{\"recording\":" + String(_recorder->isRecording() ? "true" : "false") +
            ",\"frames\":" + String(_recorder->getFrameCount()) +
            ",\"duration_ms\":" + String(_recorder->getDurationMs()) +
            ",\"fps\":" + String(_recorder->getFps()) + "}";
        _server.send(200, "application/json", json);
    }

    // ========== LED API ==========

    void handleLedStatus() {
        String json = String("{\"led_on\":") + (_ledOn ? "true" : "false") + "}";
        _server.send(200, "application/json", json);
    }

    void handleLedOn() {
        _ledOn = true;
        digitalWrite(LED_PIN, LED_ON);
        _server.send(200, "application/json", "{\"success\":true,\"led_on\":true}");
    }

    void handleLedOff() {
        _ledOn = false;
        digitalWrite(LED_PIN, LED_OFF);
        _server.send(200, "application/json", "{\"success\":true,\"led_on\":false}");
    }

    // ========== 网络 API ==========

    void handleNetStatus() {
        String json = "{\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") +
            ",\"ssid\":\"" + WiFi.SSID() + "\"" +
            ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
            ",\"rssi\":" + String(WiFi.RSSI()) +
            ",\"time_synced\":" + String(_timeMgr->isSynced() ? "true" : "false") +
            ",\"time\":\"" + _timeMgr->getDateTimeStr() + "\"}";
        _server.send(200, "application/json", json);
    }

    void handleWifiSave() {
        String ssid = _server.arg("ssid");
        if (ssid.isEmpty()) {
            _server.send(400, "application/json", "{\"success\":false}");
            return;
        }
        _server.send(200, "application/json", "{\"success\":true,\"msg\":\"重启后生效\"}");
    }

    void handleNtpSave() {
        _timeMgr->setNtpServers(_server.arg("srv1"), _server.arg("srv2"));
        _timeMgr->setTimezone(_server.arg("tz").toInt());
        _timeMgr->setSyncInterval(_server.arg("interval").toInt());
        _server.send(200, "application/json", "{\"success\":true}");
    }

    void handleNtpSync() {
        _timeMgr->sync();
        delay(2000);
        _server.send(200, "application/json",
            String("{\"success\":") + (_timeMgr->isSynced() ? "true" : "false") +
            ",\"time\":\"" + _timeMgr->getDateTimeStr() + "\"}");
    }

    // ========== 系统状态 API ==========

    void handleSysStatus() {
        String json = "{\"wifi_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") +
            ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
            ",\"rssi\":" + String(WiFi.RSSI()) +
            ",\"sd_ready\":" + String(_sd->isInitialized() ? "true" : "false") +
            ",\"sd_total\":" + String(_sd->getTotalBytes()) +
            ",\"sd_free\":" + String(_sd->getFreeBytes()) +
            ",\"uptime\":" + String(millis() / 1000) +
            ",\"free_heap\":" + String(ESP.getFreeHeap()) +
            ",\"led_on\":" + String(_ledOn ? "true" : "false") +
            ",\"recording\":" + String(_recorder->isRecording() ? "true" : "false") +
            ",\"rec_frames\":" + String(_recorder->getFrameCount()) +
            ",\"time_synced\":" + String(_timeMgr->isSynced() ? "true" : "false") +
            ",\"time\":\"" + _timeMgr->getDateTimeStr() + "\"}";
        _server.send(200, "application/json", json);
    }

    void handleNotFound() {
        _server.send(404, "text/plain", "Not Found");
    }
};

#endif // WEB_MANAGER_H