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
        _server.on("/api/network/reset", std::bind(&WebManager::handleWifiReset, this));
        _server.on("/api/network/ntp_save", std::bind(&WebManager::handleNtpSave, this));
        _server.on("/api/network/ntp_sync", std::bind(&WebManager::handleNtpSync, this));

        _server.on("/api/led/status", std::bind(&WebManager::handleLedStatus, this));
        _server.on("/api/led/on", std::bind(&WebManager::handleLedOn, this));
        _server.on("/api/led/off", std::bind(&WebManager::handleLedOff, this));

        _server.on("/api/system/status", std::bind(&WebManager::handleSysStatus, this));
        _server.on("/api/system/reinit", std::bind(&WebManager::handleReinit, this));

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
            "<button class='s' onclick='cs()'>保存照片</button>"
            "<button onclick='dp()'>下载照片</button>"
            "<button class='w' id='rb' onclick='tr()'>开始录像</button>"
            "<select id='rf' title='录像帧率'>"
            "<option value='5'>5 帧/秒</option>"
            "<option value='10' selected>10 帧/秒</option>"
            "<option value='15'>15 帧/秒</option>"
            "</select>"
            "<button id='sb' onclick='ts()'>直播预览</button>"
            "<div id='recInfo' style='margin-top:6px;font-size:12px;color:#666'></div>"
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
        for (int i = 0; i < 16; i++) {
            char opt[96];
            snprintf(opt, sizeof(opt), "<option value='%d'>%s</option>",
                     i, CameraManager::getResolutionName(i));
            _server.sendContent(opt);
        }
        _server.sendContent(F("</select></div>"));
        _server.sendContent(F(
            "<div><label>JPEG质量 <span id='qv'>15</span></label><br>"
            "<input type='range' id='qr' min='4' max='63' oninput='sq()'></div>"
        ));
        _server.sendContent(F("<div><label>特效</label><br><select id='e1' onchange='se()'>"));
        for (int i = 0; i < 15; i++) {
            char opt[96];
            snprintf(opt, sizeof(opt), "<option value='%d'>%s</option>",
                     i, CameraManager::getEffectName(i));
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
            "function cs(){fetch('/capture_save').then(r=>r.json()).then(d=>alert(d.success?'已保存到SD卡: '+d.file:(d.error||'保存失败'))).catch(function(){})}\n"
            "function dp(){var a=document.createElement('a');a.href='/capture?dl=1&t='+Date.now();a.download='';document.body.appendChild(a);a.click();document.body.removeChild(a)}\n"
            "function tr(){var b=document.getElementById('rb');var i=document.getElementById('recInfo');if(!rc){fetch('/api/record/start?fps='+document.getElementById('rf').value).then(r=>r.json()).then(d=>{if(d.success){rc=1;b.textContent='停止录像';b.className='d';i.textContent='录制中: '+d.file}else{alert(d.error||'启动失败')}}).catch(function(){})}else{fetch('/api/record/stop').then(r=>r.json()).then(d=>{rc=0;b.textContent='开始录像';b.className='w';i.textContent=d.success?('已保存: '+d.file+' ('+d.frames+'帧)'):(d.error||'')}).catch(function(){})}}\n"
            "function ts(){var b=document.getElementById('sb');var im=document.getElementById('v');if(!st){st=1;b.textContent='停止';b.className='d';im.src='/stream?t='+Date.now()}else{st=0;b.textContent='直播预览';b.className='';im.removeAttribute('src');im.src='/capture?t='+Date.now()}}\n"
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
            "function rl(){fetch('/api/sd/list?path='+encodeURIComponent(cp)).then(r=>r.json()).then(d=>{var h='';if(!d.length){h='<div style=\"padding:20px;text-align:center;color:#999\">空目录</div>'}else{d.forEach(function(f){h+='<div class=\"fi\"><div onclick=\"nv(\\''+f.path+'\\','+f.isDir+')\" style=\"cursor:pointer\">'+(f.isDir?'[目录]':'[文件]')+' '+f.name+'</div><div>';if(!f.isDir){h+='<span style=\"font-size:11px;color:#999\">'+fb(f.size)+'</span> <a class=\"p\" style=\"padding:4px 10px;border-radius:4px;color:#fff;text-decoration:none;font-size:12px\" href=\"/api/sd/download?path='+encodeURIComponent(f.path)+'\">下载</a>'}h+=' <button class=\"d\" onclick=\"dl(\\''+f.path+'\\')\">删</button></div></div>'})}document.getElementById('fl').innerHTML=h;document.getElementById('cp').textContent=cp}).catch(function(){})}\n"
            "function nv(p,d){if(d){cp=p;rl()}}\n"
            "function up(){if(cp==='/')return;var a=cp.split('/').filter(x=>x);a.pop();cp='/'+a.join('/');if(cp==='')cp='/';rl()}\n"
            "function dl(p){if(!confirm('确认删除?'))return;fetch('/api/sd/delete?path='+encodeURIComponent(p)).then(r=>r.json()).then(rl).catch(function(){})}\n"
            "function fmt(){if(!confirm('格式化将删除SD卡上的所有照片和录像，确定继续?'))return;fetch('/api/sd/format').then(r=>r.json()).then(d=>{alert(d.success?('格式化成功，可用 '+fb(d.free)):'格式化失败');rl();fetchSd()}).catch(function(){})}\n"
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
            char opt[64];
            snprintf(opt, sizeof(opt), "<option value='%d'%s>UTC%s%d</option>",
                     i, (i == tz) ? " selected" : "",
                     (i >= 0) ? "+" : "", i);
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

        client.setNoDelay(true);   // 关闭 Nagle 合并，避免每帧额外等待约 40ms
        client.print(F(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n\r\n"
        ));

        int frames = 0;
        const int MAX_FRAMES = 1200;
        while (client.connected() && frames < MAX_FRAMES) {
            camera_fb_t* fb = _camera->capture();
            if (!fb) { delay(20); continue; }

            char fh[80];
            snprintf(fh, sizeof(fh), "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
            client.print(fh);
            size_t sent = client.write(fb->buf, fb->len);
            client.print("\r\n");
            _camera->returnFrame(fb);

            // 客户端已断开或发送失败时立即退出，释放 Web 服务
            if (sent != fb->len) break;

            frames++;
            delay(10);              // 帧率交给相机采集速度 + TCP 背压控制
        }
    }

    void handleCapture() {
        camera_fb_t* fb = _camera->capture();
        if (!fb) {
            _server.send(500, "text/plain", "Capture failed");
            return;
        }
        _server.client().setNoDelay(true);
        // 流式发送，避免大块 String 造成堆碎片
        _server.sendHeader("Cache-Control", "no-cache");
        if (_server.hasArg("dl")) {
            String name = "photo_" + _timeMgr->getFileTimestamp() + ".jpg";
            _server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
        }
        _server.setContentLength(fb->len);
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
        String json = "{\"ready\":true,\"type\":\"" + _sd->getCardTypeName() +
            "\",\"total\":" + String(_sd->getTotalBytes()) +
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

        String name = path.substring(path.lastIndexOf('/') + 1);
        _server.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
        _server.setContentLength(f.size());
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
        String json = String("{\"success\":") + (ok ? "true" : "false") +
            ",\"free\":" + String(_sd->getFreeBytes()) + "}";
        _server.send(200, "application/json", json);
    }

    void handleSdUpload() {
        _server.send(200, "application/json", "{\"success\":true}");
    }

    // ========== 录制 API ==========

    void handleRecStart() {
        if (_recorder->isRecording()) {
            _server.send(200, "application/json", "{\"success\":false,\"error\":\"正在录制中\"}");
            return;
        }
        if (!_sd->isInitialized()) {
            _server.send(200, "application/json", "{\"success\":false,\"error\":\"SD卡未就绪\"}");
            return;
        }
        if (!_camera->isInitialized()) {
            _server.send(200, "application/json", "{\"success\":false,\"error\":\"摄像头未就绪\"}");
            return;
        }

        int fps = _server.arg("fps").toInt();
        String filename = "/video_" + _timeMgr->getFileTimestamp() + ".avi";
        bool ok = _recorder->startRecording(filename, fps);
        String json = String("{\"success\":") + (ok ? "true" : "false") +
            ",\"file\":\"" + filename + "\"";
        if (!ok) json += ",\"error\":\"创建文件失败\"";
        json += "}";
        _server.send(200, "application/json", json);
    }

    void handleRecStop() {
        if (!_recorder->isRecording()) {
            _server.send(200, "application/json", "{\"success\":false,\"error\":\"未在录制\"}");
            return;
        }
        String filename = _recorder->getFilename();
        uint32_t frames = _recorder->getFrameCount();
        size_t size = _recorder->getFileSize();
        _recorder->stopRecording();
        String json = String("{\"success\":true,\"file\":\"") + filename +
            "\",\"frames\":" + String(frames) +
            ",\"size\":" + String((unsigned long)size) + "}";
        _server.send(200, "application/json", json);
    }

    void handleRecStatus() {
        String json = "{\"recording\":" + String(_recorder->isRecording() ? "true" : "false") +
            ",\"frames\":" + String(_recorder->getFrameCount()) +
            ",\"duration_ms\":" + String(_recorder->getDurationMs()) +
            ",\"fps\":" + String(_recorder->getFps()) +
            ",\"file\":\"" + _recorder->getFilename() + "\"" +
            ",\"size\":" + String((unsigned long)_recorder->getFileSize()) + "}";
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
        bool sta = WiFi.status() == WL_CONNECTED;
        String ssid = sta ? WiFi.SSID() : String(AP_SSID);
        String ip = sta ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
        String json = "{\"wifi_connected\":" + String(sta ? "true" : "false") +
            ",\"ap_mode\":" + String(sta ? "false" : "true") +
            ",\"ssid\":\"" + ssid + "\"" +
            ",\"ip\":\"" + ip + "\"" +
            ",\"rssi\":" + String(WiFi.RSSI()) +
            ",\"time_synced\":" + String(_timeMgr->isSynced() ? "true" : "false") +
            ",\"time\":\"" + _timeMgr->getDateTimeStr() + "\"}";
        _server.send(200, "application/json", json);
    }

    void handleWifiSave() {
        String ssid = _server.arg("ssid");
        String pass = _server.arg("pass");
        if (ssid.isEmpty()) {
            _server.send(400, "application/json", "{\"success\":false,\"error\":\"SSID不能为空\"}");
            return;
        }
        _wifi->saveCredentials(ssid, pass);
        _server.send(200, "application/json",
            "{\"success\":true,\"msg\":\"已保存，设备将在2秒后重启\"}");
        DEBUG_PRINTF("[WEB] WiFi已更新为 %s，准备重启\r\n", ssid.c_str());
        delay(2000);
        ESP.restart();
    }

    // 清除 WiFi 凭据并重启，下次启动自动进入配网门户
    void handleWifiReset() {
        _wifi->clearCredentials();
        _server.send(200, "application/json",
            "{\"success\":true,\"msg\":\"凭据已清除，设备将重启进入配网模式\"}");
        delay(1000);
        ESP.restart();
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
        char tm[32];
        _timeMgr->getDateTimeChars(tm, sizeof(tm));
        char json[96];
        snprintf(json, sizeof(json), "{\"success\":%s,\"time\":\"%s\"}",
            _timeMgr->isSynced() ? "true" : "false", tm);
        _server.send(200, "application/json", json);
    }

    // ========== 系统状态 API ==========

    void handleSysStatus() {
        bool sta = WiFi.status() == WL_CONNECTED;
        IPAddress a = sta ? WiFi.localIP() : WiFi.softAPIP();
        char tm[32];
        _timeMgr->getDateTimeChars(tm, sizeof(tm));

        // 单次栈缓冲拼装，避免多次 String 拼接产生堆碎片
        char json[384];
        snprintf(json, sizeof(json),
            "{\"wifi_connected\":%s,\"ip\":\"%u.%u.%u.%u\",\"rssi\":%d,"
            "\"sd_ready\":%s,\"sd_total\":%lu,\"sd_free\":%lu,"
            "\"uptime\":%lu,\"free_heap\":%lu,\"min_free_heap\":%lu,"
            "\"led_on\":%s,\"recording\":%s,\"rec_frames\":%lu,"
            "\"time_synced\":%s,\"time\":\"%s\"}",
            sta ? "true" : "false",
            a[0], a[1], a[2], a[3], WiFi.RSSI(),
            _sd->isInitialized() ? "true" : "false",
            (unsigned long)_sd->getTotalBytes(), (unsigned long)_sd->getFreeBytes(),
            millis() / 1000, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
            _ledOn ? "true" : "false",
            _recorder->isRecording() ? "true" : "false",
            (unsigned long)_recorder->getFrameCount(),
            _timeMgr->isSynced() ? "true" : "false",
            tm);
        _server.send(200, "application/json", json);
    }

    // 在线重新初始化摄像头 / SD 卡（无需重启，便于插拔卡或排线后恢复）
    // target=camera 只重试摄像头，target=sd 只重试 SD，缺省两者都试
    void handleReinit() {
        String target = _server.arg("target");

        bool camOk = _camera->isInitialized();
        bool sdOk = _sd->isInitialized();

        if (target != "sd" && !camOk) camOk = _camera->begin();
        if (target != "camera" && !sdOk) sdOk = _sd->begin();

        char json[160];
        snprintf(json, sizeof(json),
            "{\"camera\":%s,\"sd\":%s,\"target\":\"%s\",\"free_heap\":%lu}",
            camOk ? "true" : "false",
            sdOk ? "true" : "false",
            target.c_str(),
            (unsigned long)ESP.getFreeHeap());
        _server.send(200, "application/json", json);
    }

    void handleNotFound() {
        _server.send(404, "text/plain", "Not Found");
    }
};

#endif // WEB_MANAGER_H