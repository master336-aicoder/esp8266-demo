/*
 * DeviceStatusWeb - 设备状态监控Web服务
 * WiFi连接成功后启动，展示所有GPIO接口状态、系统信息和MQTT配置
 * 采用流式发送HTML，避免大String占用内存导致崩溃
 */

#ifndef DEVICE_STATUS_WEB_H
#define DEVICE_STATUS_WEB_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "MqttManager.h"
#include "TimeManager.h"

// ESP-01S: 不启用 DHT
// #define ENABLE_DHT

#ifdef ENABLE_DHT
#include "DHTManager.h"
#endif

// GPIO引脚定义（ESP8266可用引脚）
struct GpioInfo {
    uint8_t pin;
    const char* name;
    const char* label;
};

// ESP-01S 可用 GPIO 列表
// 注意：GPIO0 启动时必须为高电平，GPIO2 接板载 LED
// GPIO1(TX)/GPIO3(RX) 是串口引脚，不建议用作普通 IO
const GpioInfo GPIO_LIST[] = {
    { 0,  "GPIO0",  "GPIO0" },
    { 2,  "GPIO2",  "GPIO2 (LED)" },
};
const int GPIO_COUNT = sizeof(GPIO_LIST) / sizeof(GPIO_LIST[0]);

class DeviceStatusWeb {
public:
    DeviceStatusWeb(uint16_t port = 80) : _server(port), _mqttMgr(nullptr), _timeMgr(nullptr)
#ifdef ENABLE_DHT
        , _dhtMgr(nullptr)
#endif
    {}

    void setMqttManager(MqttManager* mqttMgr) {
        _mqttMgr = mqttMgr;
    }

    void setTimeManager(TimeManager* timeMgr) {
        _timeMgr = timeMgr;
    }

#ifdef ENABLE_DHT
    void setDHTManager(DHTManager* dhtMgr) {
        _dhtMgr = dhtMgr;
    }
#endif

    void begin() {
        _server.on("/", std::bind(&DeviceStatusWeb::handleRoot, this));
        _server.on("/mqtt", std::bind(&DeviceStatusWeb::handleMqttPage, this));
        _server.on("/ntp", std::bind(&DeviceStatusWeb::handleNtpPage, this));
        _server.on("/api/status", std::bind(&DeviceStatusWeb::handleApiStatus, this));
        _server.on("/api/gpio", std::bind(&DeviceStatusWeb::handleApiGpio, this));
        _server.on("/api/mqtt/status", std::bind(&DeviceStatusWeb::handleApiMqttStatus, this));
        _server.on("/api/mqtt/config", HTTP_POST, std::bind(&DeviceStatusWeb::handleApiMqttSave, this));
        _server.on("/api/mqtt/test", std::bind(&DeviceStatusWeb::handleApiMqttTest, this));
        _server.on("/api/mqtt/device-info", std::bind(&DeviceStatusWeb::handleApiMqttDeviceInfo, this));
        _server.on("/api/mqtt/log", std::bind(&DeviceStatusWeb::handleApiMqttLog, this));
        _server.on("/api/time", std::bind(&DeviceStatusWeb::handleApiTime, this));
        _server.on("/api/ntp/status", std::bind(&DeviceStatusWeb::handleApiNtpStatus, this));
        _server.on("/api/ntp/save", HTTP_POST, std::bind(&DeviceStatusWeb::handleApiNtpSave, this));
        _server.on("/api/ntp/sync", std::bind(&DeviceStatusWeb::handleApiNtpSync, this));
        _server.on("/api/gpio/control", HTTP_POST, std::bind(&DeviceStatusWeb::handleApiGpioControl, this));
        _server.onNotFound(std::bind(&DeviceStatusWeb::handleNotFound, this));

        _server.begin();
        DEBUG_PRINTLN("[状态页] 设备状态Web服务已启动");
        DEBUG_PRINTF("[状态页] 访问地址: http://%s\r\n", WiFi.localIP().toString().c_str());
    }

    void stop() {
        _server.stop();
        DEBUG_PRINTLN("[状态页] 设备状态Web服务已停止");
    }

    void handleClient() {
        _server.handleClient();
    }

private:
    ESP8266WebServer _server;
    MqttManager* _mqttMgr;
    TimeManager* _timeMgr;
#ifdef ENABLE_DHT
    DHTManager* _dhtMgr;
#endif

    // 获取GPIO模式字符串（中文）
    String getPinModeStr(uint8_t pin) {
        String mode = getGpioModeStr(pin);
        if (mode == "OUTPUT") return "输出";
        if (mode == "INPUT_PULLUP") return "输入上拉";
        return "输入";
    }

    // ========== 流式发送辅助 ==========
    void sendStart(const char* contentType) {
        _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        _server.send(200, contentType, "");
    }

    void sc(const String& chunk) {
        _server.sendContent(chunk);
        yield();
    }

    void sc(const char* chunk) {
        _server.sendContent(chunk);
        yield();
    }

    // ========== 页面处理 ==========

    // 主页 - 设备状态（流式发送）
    void handleRoot() {
        sendStart("text/html");
        sendPageHeader("ESP8266 设备状态", 0); // 0=状态页激活

        // 当前时间卡片
        sc("<div class='card'><div class='card-h'>&#128339; 当前时间</div><div class='card-b'>");
        sc("<div class='time-disp'>");
        if (_timeMgr && _timeMgr->isSynced()) {
            sc("<div class='time-big' id='ct'>" + _timeMgr->getTimeStr() + "</div>");
            sc("<div class='date-txt' id='cd'>" + _timeMgr->getDateStr() + "</div>");
            sc("<div class='time-st ok'>&#9989; 网络时间已同步</div>");
        } else {
            sc("<div class='time-big' id='ct'>--:--:--</div>");
            sc("<div class='date-txt' id='cd'>同步中...</div>");
            sc("<div class='time-st'>正在同步网络时间</div>");
        }
        sc("</div></div></div>");

        // 系统信息卡片
        sc("<div class='card'><div class='card-h'>&#128187; 系统信息</div><div class='card-b'>");
        sc("<div class='info-g'>");
        sc("<div class='info-i'><div class='info-l'>芯片ID</div><div class='info-v'>0x" + String(ESP.getChipId(), HEX) + "</div></div>");
        sc("<div class='info-i'><div class='info-l'>CPU频率</div><div class='info-v'>" + String(ESP.getCpuFreqMHz()) + " MHz</div></div>");
        sc("<div class='info-i'><div class='info-l'>Flash</div><div class='info-v'>" + String(ESP.getFlashChipSize() / 1024) + " KB</div></div>");
        sc("<div class='info-i'><div class='info-l'>可用内存</div><div class='info-v'>" + String(ESP.getFreeHeap()) + " B</div></div>");
        sc("<div class='info-i'><div class='info-l'>运行时间</div><div class='info-v'>" + formatUptime(millis() / 1000) + "</div></div>");
        sc("</div></div></div>");

        // WiFi状态卡片
        int rssi = WiFi.RSSI();
        int sigPct = (rssi + 100) * 2;
        if (sigPct > 100) sigPct = 100;
        if (sigPct < 0) sigPct = 0;
        sc("<div class='card'><div class='card-h'>&#128246; WiFi 状态</div><div class='card-b'>");
        sc("<div class='info-g'>");
        sc("<div class='info-i'><div class='info-l'>SSID</div><div class='info-v'>" + String(WiFi.SSID()) + "</div></div>");
        sc("<div class='info-i'><div class='info-l'>IP地址</div><div class='info-v'>" + WiFi.localIP().toString() + "</div></div>");
        sc("<div class='info-i'><div class='info-l'>信号强度</div><div class='info-v'>" + String(rssi) + " dBm");
        sc("<div class='rssi-bar'><div class='rssi-fill' style='width:" + String(sigPct) + "%'></div></div>");
        sc("</div></div>");
        sc("<div class='info-i'><div class='info-l'>信道</div><div class='info-v'>" + String(WiFi.channel()) + "</div></div>");
        sc("</div></div></div>");

#ifdef ENABLE_DHT
        // 温湿度卡片（DHT11）
        sc("<div class='card'><div class='card-h'>&#127777; 温湿度 (DHT11)</div><div class='card-b'>");
        if (_dhtMgr && _dhtMgr->isValid()) {
            float temp = _dhtMgr->getTemperature();
            float hum = _dhtMgr->getHumidity();
            sc("<div class='dht-row'>");
            sc("<div class='dht-item'>");
            sc("<div class='dht-icon'>&#127777;</div>");
            sc("<div class='dht-val'>" + String(temp, 1) + "°C</div>");
            sc("<div class='dht-label'>温度</div>");
            sc("</div>");
            sc("<div class='dht-item'>");
            sc("<div class='dht-icon'>&#128167;</div>");
            sc("<div class='dht-val'>" + String(hum, 1) + "%</div>");
            sc("<div class='dht-label'>湿度</div>");
            sc("</div>");
            sc("</div>");
            // 体感温度
            float feels = temp * 0.8 + hum * 0.01 * (temp - 14.3) + 46.3;
            sc("<div class='dht-info'>体感温度: " + String(feels, 1) + "°C | 引脚: D1 (GPIO5)</div>");
        } else {
            sc("<div class='dht-warn'>传感器未就绪或读取失败</div>");
            sc("<div class='dht-info'>请检查 DHT11 接线：D1(GPIO5) → DATA, VCC → 3.3V, GND → GND</div>");
        }
        sc("</div></div>");
#endif

        // GPIO控制卡片
        sc("<div class='card'><div class='card-h'>&#128268; GPIO 接口控制</div><div class='card-b'>");
        sc("<div class='gpio-g'>");
        for (int i = 0; i < GPIO_COUNT; i++) {
            uint8_t pin = GPIO_LIST[i].pin;
            int val = digitalRead(pin);
            String mode = getPinModeStr(pin);
            const char* valCls = val ? "hi" : "lo";
            const char* valTxt = val ? "HIGH" : "LOW";
            sc("<div class='gpio-it' id='g-" + String(pin) + "'>");
            sc("<div class='gpio-p'>" + String(GPIO_LIST[i].label) + "</div>");
            sc("<div class='gpio-v " + String(valCls) + "'>" + String(valTxt) + "</div>");
            sc("<div class='gpio-m'>" + mode + "</div>");
            sc("<div class='gpio-c'>");
            sc("<div class='btn-r'>");
            sc("<button class='b-in" + String(mode == "输入" ? " active" : "") + "' onclick='setMode(" + String(pin) + ",\"input\")'>输入</button>");
            sc("<button class='b-out" + String(mode == "输出" ? " active" : "") + "' onclick='setMode(" + String(pin) + ",\"output\")'>输出</button>");
            sc("</div>");
            sc("<div class='btn-r'>");
            sc("<button class='b-lo' onclick='setVal(" + String(pin) + ",0)'>LOW</button>");
            sc("<button class='b-hi' onclick='setVal(" + String(pin) + ",1)'>HIGH</button>");
            sc("</div></div></div>");
            yield();
        }
        sc("</div></div></div>");

        sc("<div class='ft'>ESP8266 AutoWifiConfig v1.3</div>");

        // JavaScript
        sc("<script>");
        sc("var _tOff=0,_tSync=false;");
        sc("function updT(){if(_tSync){var d=new Date(Date.now()+_tOff);");
        sc("var h=('0'+d.getHours()).slice(-2),m=('0'+d.getMinutes()).slice(-2),s=('0'+d.getSeconds()).slice(-2);");
        sc("document.getElementById('ct').textContent=h+':'+m+':'+s;");
        sc("var y=d.getFullYear(),mo=('0'+(d.getMonth()+1)).slice(-2),da=('0'+d.getDate()).slice(-2);");
        sc("document.getElementById('cd').textContent=y+'-'+mo+'-'+da;}");
        sc("else{fetch('/api/time').then(r=>r.json()).then(d=>{if(d.synced){_tSync=true;_tOff=d.timestamp*1000-Date.now();");
        sc("document.querySelector('.time-st').textContent='网络时间已同步';document.querySelector('.time-st').className='time-st ok';}");
        sc("else{setTimeout(updT,2000)}}).catch(e=>setTimeout(updT,3000))}}");
        sc("setInterval(updT,1000);updT();");
        sc("function setMode(p,m){var fd=new FormData();fd.append('pin',p);fd.append('mode',m);");
        sc("fetch('/api/gpio/control',{method:'POST',body:fd}).then(r=>r.json()).then(d=>{if(d.success){updG(d.pin,d.mode,d.value)}");
        sc("else{alert('失败:'+d.msg)}}).catch(e=>alert('错误:'+e.message))}");
        sc("function setVal(p,v){var fd=new FormData();fd.append('pin',p);fd.append('value',v);");
        sc("fetch('/api/gpio/control',{method:'POST',body:fd}).then(r=>r.json()).then(d=>{if(d.success){updG(d.pin,d.mode,d.value)}");
        sc("else{alert('失败:'+d.msg)}}).catch(e=>alert('错误:'+e.message))}");
        sc("function updG(p,m,v){var el=document.getElementById('g-'+p);if(!el)return;");
        sc("var ve=el.querySelector('.gpio-v');");
        sc("if(v){ve.textContent='HIGH';ve.className='gpio-v hi'}else{ve.textContent='LOW';ve.className='gpio-v lo'}");
        sc("var me=el.querySelector('.gpio-m');me.textContent=m==='OUTPUT'?'输出':'输入';");
        sc("var inBtn=el.querySelector('.b-in'),outBtn=el.querySelector('.b-out');");
        sc("if(m==='OUTPUT'){inBtn.classList.remove('active');outBtn.classList.add('active')}");
        sc("else{inBtn.classList.add('active');outBtn.classList.remove('active')}}");
        sc("</script>");

        sendPageFooter();
    }

    // MQTT配置页面（流式发送）
    void handleMqttPage() {
        sendStart("text/html");
        sendPageHeader("MQTT 配置", 1); // 1=MQTT页激活

        MqttConfig cfg;
        bool connected = false;
        if (_mqttMgr) {
            cfg = _mqttMgr->getConfig();
            connected = _mqttMgr->isConnected();
        }

        // 连接状态卡片
        sc("<div class='card'><div class='card-h'>");
        sc("<span>MQTT 连接状态</span>");
        sc("<span class='badge " + String(connected ? "on" : "off") + "'>" + String(connected ? "已连接" : "未连接") + "</span>");
        sc("</div><div class='card-b'>");
        sc("<div id='msg' class='msg info'>修改配置后点击保存，设备将自动重连</div>");

        // 开关
        sc("<div class='sw-row'>");
        sc("<span style='font-weight:500'>启用MQTT上报</span>");
        sc("<label class='sw'><input type='checkbox' id='en' " + String(cfg.enabled ? "checked" : "") + "><span class='sl'></span></label>");
        sc("</div>");

        sc("<div class='fg'><label>服务器地址</label>");
        sc("<input type='text' id='host' value='" + String(cfg.host) + "'></div>");

        sc("<div class='row2'>");
        sc("<div class='fg'><label>端口</label>");
        sc("<input type='number' id='port' value='" + String(cfg.port) + "'></div>");
        sc("<div class='fg'><label>上报间隔(秒)</label>");
        sc("<input type='number' id='interval' value='" + String(cfg.interval / 1000) + "' min='1' max='3600'></div>");
        sc("</div>");

        sc("<div class='row2'>");
        sc("<div class='fg'><label>用户名</label>");
        sc("<input type='text' id='user' value='" + String(cfg.username) + "'></div>");
        sc("<div class='fg'><label>密码</label>");
        sc("<input type='password' id='pass' value='" + String(cfg.password) + "'></div>");
        sc("</div>");

        sc("<div class='fg'><label>状态上报主题</label>");
        sc("<input type='text' id='topic' value='" + String(cfg.topic) + "'></div>");

        sc("<div class='fg'><label>设备信息主题</label>");
        sc("<input type='text' id='itopic' value='" + String(cfg.infoTopic) + "'></div>");

        sc("<div class='fg'><label>客户端ID</label>");
        sc("<input type='text' id='cid' value='" + String(cfg.clientId) + "' placeholder='留空自动生成'>");
        sc("<div style='font-size:11px;color:#999;margin-top:4px'>留空则自动生成: ESP8266-芯片ID</div></div>");

        sc("<div class='fg'><label>上报GPIO引脚</label>");
        sc("<div class='pin-act'>");
        sc("<button type='button' class='bt' onclick='selAll()'>全选</button>");
        sc("<button type='button' class='bt' onclick='desAll()'>全不选</button>");
        sc("</div>");
        sc("<div class='pin-g'>");
        for (int i = 0; i < GPIO_COUNT; i++) {
            uint8_t pin = GPIO_LIST[i].pin;
            bool chk = (cfg.pinMask & (1UL << pin)) != 0;
            sc("<label class='pin-it'>");
            sc("<input type='checkbox' class='pcb' value='" + String(pin) + "' " + String(chk ? "checked" : "") + ">");
            sc("<span>" + String(GPIO_LIST[i].name) + "</span>");
            sc("</label>");
            yield();
        }
        sc("</div></div>");

        sc("<div class='btn-row2'>");
        sc("<button class='btn2' onclick='testMqtt()'>&#129510; 测试上报</button>");
        sc("<button class='btn2' onclick='repDev()'>&#128230; 设备信息</button>");
        sc("<button class='btn1' onclick='saveCfg()'>&#128190; 保存配置</button>");
        sc("</div>");

        sc("</div></div>");

        // 上报记录卡片
        sc("<div class='card'><div class='card-h' style='display:flex;justify-content:space-between;align-items:center'>");
        sc("<span>&#128203; 最近上报记录</span>");
        sc("<button onclick='refLog()' style='background:rgba(255,255,255,.2);color:#fff;border:none;padding:4px 12px;border-radius:6px;font-size:12px;cursor:pointer'>刷新</button>");
        sc("</div><div class='card-b' style='padding:0'>");
        sc("<div id='log' class='log-list'>");
        sc("<div style='padding:20px;text-align:center;color:#999;font-size:13px'>加载中...</div>");
        sc("</div></div></div>");

        // JavaScript
        sc("<script>");
        sc("function showMsg(t,ty){var m=document.getElementById('msg');m.className='msg '+ty+' show';m.textContent=t}");
        sc("function saveCfg(){");
        sc("var host=document.getElementById('host').value.trim();");
        sc("var port=document.getElementById('port').value;");
        sc("var user=document.getElementById('user').value;");
        sc("var pass=document.getElementById('pass').value;");
        sc("var topic=document.getElementById('topic').value.trim();");
        sc("var itopic=document.getElementById('itopic').value.trim();");
        sc("var cid=document.getElementById('cid').value.trim();");
        sc("var interval=document.getElementById('interval').value;");
        sc("var en=document.getElementById('en').checked?'1':'0';");
        sc("var pins=[];document.querySelectorAll('.pcb:checked').forEach(function(c){pins.push(c.value)});");
        sc("var fd=new FormData();fd.append('host',host);fd.append('port',port);");
        sc("fd.append('username',user);fd.append('password',pass);");
        sc("fd.append('topic',topic);fd.append('info_topic',itopic);fd.append('client_id',cid);");
        sc("fd.append('interval',parseInt(interval)*1000);fd.append('enabled',en);fd.append('pins',pins.join(','));");
        sc("showMsg('保存中...','info');");
        sc("fetch('/api/mqtt/config',{method:'POST',body:fd})");
        sc(".then(r=>r.json()).then(d=>{if(d.success){showMsg(d.msg,'success');setTimeout(function(){location.reload()},1500)}");
        sc("else{showMsg(d.msg,'error')}}).catch(e=>{showMsg('失败:'+e.message,'error')})}");
        sc("function testMqtt(){showMsg('上报中...','info');fetch('/api/mqtt/test')");
        sc(".then(r=>r.json()).then(d=>{if(d.success){showMsg(d.msg,'success')}else{showMsg(d.msg,'error')}})");
        sc(".catch(e=>{showMsg('失败:'+e.message,'error')})}");
        sc("function repDev(){showMsg('上报中...','info');fetch('/api/mqtt/device-info')");
        sc(".then(r=>r.json()).then(d=>{if(d.success){showMsg(d.msg,'success')}else{showMsg(d.msg,'error')}})");
        sc(".catch(e=>{showMsg('失败:'+e.message,'error')})}");
        sc("function selAll(){document.querySelectorAll('.pcb').forEach(function(c){c.checked=true})}");
        sc("function desAll(){document.querySelectorAll('.pcb').forEach(function(c){c.checked=false})}");
        sc("function refLog(){fetch('/api/mqtt/log').then(r=>r.json()).then(d=>{");
        sc("var el=document.getElementById('log');if(!el)return;");
        sc("if(!d||d.length===0){el.innerHTML='<div style=\\'padding:20px;text-align:center;color:#999;font-size:13px\\'>暂无记录</div>';return}");
        sc("var h='';d.forEach(function(it){h+='<div class=\\'log-it\\'><div class=\\'log-d '+(it.success?'ok':'fl')+'\\'></div>';");
        sc("h+='<div class=\\'log-t\\'>'+it.time+'</div>';");
        sc("h+='<div><div class=\\'log-m\\'>'+it.msg+'</div>';");
        sc("h+='<div class=\\'log-topic\\'>'+it.topic+'</div></div></div>'});");
        sc("el.innerHTML=h}).catch(e=>console.error(e))}");
        sc("refLog();");
        sc("</script>");

        sendPageFooter();
    }

    // NTP时间设置页面（流式发送）
    void handleNtpPage() {
        if (!_timeMgr) {
            _server.send(500, "text/plain", "TimeManager not initialized");
            return;
        }
        NtpConfig cfg = _timeMgr->getConfig();

        sendStart("text/html");
        sendPageHeader("NTP 时间设置", 2); // 2=NTP页激活

        // 当前时间状态
        sc("<div class='card'><div class='card-h'>当前时间状态</div><div class='card-b'>");
        if (_timeMgr->isSynced()) {
            sc("<div class='time-st2 ok'>&#9989; 已同步<br><strong>" + _timeMgr->getDateTimeStr() + "</strong></div>");
        } else {
            sc("<div class='time-st2 pen'>&#9203; 同步中...<br>请稍候或点击下方手动同步</div>");
        }
        sc("<div class='btn-row2'>");
        sc("<button class='btn1' style='background:linear-gradient(135deg,#4caf50,#2e7d32)' onclick='syncNow()'>&#128260; 立即同步</button>");
        sc("</div>");
        sc("<div class='msg' id='msg'></div>");
        sc("</div></div>");

        // NTP服务器配置
        sc("<div class='card'><div class='card-h'>NTP服务器配置</div><div class='card-b'>");
        sc("<div class='fg'><label>NTP服务器1（主）</label>");
        sc("<input type='text' id='s1' value='" + String(cfg.server1) + "'></div>");
        sc("<div class='fg'><label>NTP服务器2（备用）</label>");
        sc("<input type='text' id='s2' value='" + String(cfg.server2) + "'></div>");
        sc("<div class='fg'><label>NTP服务器3（备用）</label>");
        sc("<input type='text' id='s3' value='" + String(cfg.server3) + "'></div>");
        sc("<div class='row2'>");
        sc("<div class='fg'><label>时区</label>");
        sc("<input type='number' id='tz' value='" + String(cfg.timezone) + "' min='-12' max='12'></div>");
        sc("<div class='fg'><label>同步间隔(分钟)</label>");
        sc("<input type='number' id='iv' value='" + String(cfg.interval / 60000) + "' min='5' max='1440'></div>");
        sc("</div>");
        sc("<div class='btn-row2'>");
        sc("<button class='btn1' onclick='saveCfg()'>&#128190; 保存配置</button>");
        sc("</div>");
        sc("</div></div>");

        // JavaScript
        sc("<script>");
        sc("function showMsg(t,ty){var m=document.getElementById('msg');m.textContent=t;m.className='msg show '+ty;");
        sc("setTimeout(function(){m.className='msg'},3000)}");
        sc("function saveCfg(){var fd=new FormData();");
        sc("fd.append('server1',document.getElementById('s1').value.trim());");
        sc("fd.append('server2',document.getElementById('s2').value.trim());");
        sc("fd.append('server3',document.getElementById('s3').value.trim());");
        sc("fd.append('timezone',document.getElementById('tz').value);");
        sc("fd.append('interval_min',document.getElementById('iv').value);");
        sc("showMsg('保存中...','info');");
        sc("fetch('/api/ntp/save',{method:'POST',body:fd})");
        sc(".then(r=>r.json()).then(d=>{if(d.success){showMsg(d.msg,'success');setTimeout(function(){location.reload()},1500)}");
        sc("else{showMsg(d.msg,'error')}}).catch(e=>{showMsg('失败:'+e.message,'error')})}");
        sc("function syncNow(){showMsg('同步中...','info');fetch('/api/ntp/sync')");
        sc(".then(r=>r.json()).then(d=>{if(d.success){showMsg(d.msg,'success');setTimeout(function(){location.reload()},1500)}");
        sc("else{showMsg(d.msg,'info');setTimeout(function(){location.reload()},3000)}})");
        sc(".catch(e=>{showMsg('失败:'+e.message,'error')})}");
        sc("</script>");

        sendPageFooter();
    }

    // ========== 公共HTML部分 ==========

    void sendPageHeader(const char* title, int activeTab) {
        sc("<!DOCTYPE html><html lang='zh-CN'><head>");
        sc("<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
        sc("<title>");
        sc(title);
        sc("</title>");
        sc("<style>");
        sc("*{box-sizing:border-box;margin:0;padding:0}");
        sc("body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#f0f2f5;color:#333;padding:16px}");
        sc(".container{max-width:900px;margin:0 auto}");
        sc("h1{text-align:center;color:#1a1a2e;margin-bottom:16px;font-size:22px}");
        sc(".nav{text-align:center;margin-bottom:16px}");
        sc(".nav a{display:inline-block;padding:8px 18px;margin:0 4px;background:#fff;color:#667eea;text-decoration:none;border-radius:8px;font-size:14px;font-weight:500;box-shadow:0 2px 8px rgba(0,0,0,.08)}");
        sc(".nav a.active{background:linear-gradient(135deg,#667eea,#764ba2);color:#fff}");
        sc(".card{background:#fff;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,.08);margin-bottom:16px;overflow:hidden}");
        sc(".card-h{background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;padding:14px 18px;font-size:15px;font-weight:600}");
        sc(".card-b{padding:16px}");
        sc(".info-g{display:grid;grid-template-columns:repeat(auto-fit,minmax(160px,1fr));gap:12px}");
        sc(".info-i{background:#f8f9fa;padding:10px 14px;border-radius:8px}");
        sc(".info-l{font-size:12px;color:#888;margin-bottom:4px}");
        sc(".info-v{font-size:14px;color:#333;font-weight:500;font-family:monospace;word-break:break-all}");
        sc(".time-disp{text-align:center;padding:8px 0}");
        sc(".time-big{font-size:36px;font-weight:700;color:#667eea;font-family:monospace;letter-spacing:2px}");
        sc(".date-txt{font-size:14px;color:#666;margin-top:4px}");
        sc(".time-st{font-size:12px;color:#999;margin-top:8px}");
        sc(".time-st.ok{color:#4caf50}");
        sc(".time-st2{padding:12px 16px;border-radius:8px;font-size:14px;text-align:center;margin-bottom:12px}");
        sc(".time-st2.ok{background:#e8f5e9;color:#2e7d32}");
        sc(".time-st2.pen{background:#fff3e0;color:#e65100}");
        sc(".gpio-g{display:grid;grid-template-columns:repeat(auto-fill,minmax(110px,1fr));gap:10px}");
        sc(".gpio-it{background:#f8f9fa;padding:12px;border-radius:10px;text-align:center}");
        sc(".gpio-p{font-size:11px;color:#888;margin-bottom:6px}");
        sc(".gpio-v{font-size:22px;font-weight:700;margin-bottom:4px}");
        sc(".gpio-v.hi{color:#4caf50}");
        sc(".gpio-v.lo{color:#f44336}");
        sc(".gpio-m{font-size:11px;color:#999;margin-bottom:8px}");
        sc(".gpio-c{padding-top:8px;border-top:1px solid #e0e0e0}");
        sc(".btn-r{display:flex;gap:4px;margin-bottom:6px}");
        sc(".btn-r:last-child{margin-bottom:0}");
        sc(".b-lo{flex:1;padding:5px 0;border:none;border-radius:4px;font-size:11px;font-weight:600;cursor:pointer;background:#ffebee;color:#c62828}");
        sc(".b-hi{flex:1;padding:5px 0;border:none;border-radius:4px;font-size:11px;font-weight:600;cursor:pointer;background:#e8f5e9;color:#2e7d32}");
        sc(".b-in{flex:1;padding:5px 0;border:1px solid #e0e0e0;border-radius:4px;font-size:11px;font-weight:600;cursor:pointer;background:#fff;color:#666}");
        sc(".b-in.active{background:#667eea;color:#fff;border-color:#667eea}");
        sc(".b-out{flex:1;padding:5px 0;border:1px solid #e0e0e0;border-radius:4px;font-size:11px;font-weight:600;cursor:pointer;background:#fff;color:#666}");
        sc(".b-out.active{background:#764ba2;color:#fff;border-color:#764ba2}");
        sc(".rssi-bar{height:6px;background:#e0e0e0;border-radius:3px;overflow:hidden;margin-top:6px}");
        sc(".rssi-fill{height:100%;background:linear-gradient(90deg,#f44336,#ff9800,#4caf50);border-radius:3px}");
        // DHT温湿度样式
        sc(".dht-row{display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-bottom:12px}");
        sc(".dht-item{text-align:center;padding:16px;background:linear-gradient(135deg,#667eea15,#764ba215);border-radius:12px}");
        sc(".dht-icon{font-size:32px;margin-bottom:8px}");
        sc(".dht-val{font-size:32px;font-weight:700;color:#333;line-height:1.2}");
        sc(".dht-label{font-size:13px;color:#666;margin-top:4px}");
        sc(".dht-info{font-size:12px;color:#888;text-align:center;padding-top:8px;border-top:1px solid #eee}");
        sc(".dht-warn{text-align:center;padding:20px;color:#ff9800;font-size:14px;font-weight:500}");
        sc(".badge{padding:4px 12px;border-radius:20px;font-size:12px;font-weight:600}");
        sc(".badge.on{background:#4caf50;color:#fff}");
        sc(".badge.off{background:#f44336;color:#fff}");
        sc(".fg{margin-bottom:14px}");
        sc(".fg label{display:block;margin-bottom:6px;color:#333;font-size:13px;font-weight:500}");
        sc(".fg input{width:100%;padding:10px 12px;border:2px solid #e0e0e0;border-radius:8px;font-size:14px}");
        sc(".fg input:focus{outline:none;border-color:#667eea}");
        sc(".row2{display:grid;grid-template-columns:1fr 1fr;gap:12px}");
        sc(".btn1{width:100%;padding:12px;background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;border:none;border-radius:10px;font-size:14px;font-weight:600;cursor:pointer}");
        sc(".btn2{padding:10px;background:#f0f0f0;color:#667eea;border:none;border-radius:8px;font-size:13px;font-weight:600;cursor:pointer}");
        sc(".btn-row2{display:grid;grid-template-columns:1fr 1fr 1fr;gap:10px;margin-top:10px}");
        sc(".pin-g{display:grid;grid-template-columns:repeat(auto-fill,minmax(90px,1fr));gap:8px;margin-top:8px}");
        sc(".pin-it{background:#f8f9fa;padding:8px;border-radius:8px;display:flex;align-items:center;gap:8px;cursor:pointer}");
        sc(".pin-it input{cursor:pointer}");
        sc(".pin-it span{font-size:13px;color:#333}");
        sc(".pin-act{margin-bottom:8px;display:flex;gap:8px}");
        sc(".bt{padding:5px 12px;border:none;border-radius:6px;background:#e8eaf6;color:#667eea;font-size:12px;font-weight:600;cursor:pointer}");
        sc(".bt:hover{background:#667eea;color:#fff}");
        sc(".msg{padding:10px 14px;border-radius:8px;margin-bottom:12px;font-size:13px;display:none}");
        sc(".msg.show{display:block}");
        sc(".msg.info{background:#e3f2fd;color:#1565c0}");
        sc(".msg.success{background:#e8f5e9;color:#2e7d32}");
        sc(".msg.error{background:#ffebee;color:#c62828}");
        sc(".sw-row{display:flex;align-items:center;justify-content:space-between;padding:12px;background:#f8f9fa;border-radius:8px;margin-bottom:14px}");
        sc(".sw{position:relative;width:48px;height:26px}");
        sc(".sw input{opacity:0;width:0;height:0}");
        sc(".sl{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#ccc;border-radius:26px;transition:.3s}");
        sc(".sl:before{position:absolute;content:'';height:20px;width:20px;left:3px;bottom:3px;background:white;border-radius:50%;transition:.3s}");
        sc("input:checked+.sl{background:#667eea}");
        sc("input:checked+.sl:before{transform:translateX(22px)}");
        sc(".log-list{max-height:400px;overflow-y:auto}");
        sc(".log-it{padding:10px 16px;border-bottom:1px solid #f0f0f0;font-size:12px;display:flex;gap:10px;align-items:flex-start}");
        sc(".log-it:last-child{border-bottom:none}");
        sc(".log-d{width:8px;height:8px;border-radius:50%;margin-top:5px;flex-shrink:0}");
        sc(".log-d.ok{background:#4caf50}");
        sc(".log-d.fl{background:#f44336}");
        sc(".log-t{color:#999;flex-shrink:0;font-family:monospace}");
        sc(".log-m{color:#333;word-break:break-all}");
        sc(".log-topic{color:#667eea;font-family:monospace;font-size:11px;margin-top:2px}");
        sc(".ft{text-align:center;color:#999;font-size:12px;margin-top:16px}");
        sc("</style></head><body>");
        sc("<div class='container'>");
        sc("<h1>&#128225; ESP8266 设备监控</h1>");
        sc("<div class='nav'>");
        sc("<a href='/'");
        if (activeTab == 0) sc(" class='active'");
        sc(">&#128225; 设备状态</a>");
        sc("<a href='/mqtt'");
        if (activeTab == 1) sc(" class='active'");
        sc(">&#128202; MQTT配置</a>");
        sc("<a href='/ntp'");
        if (activeTab == 2) sc(" class='active'");
        sc(">&#128339; 时间设置</a>");
        sc("</div>");
    }

    void sendPageFooter() {
        sc("</div></body></html>");
    }

    // ========== API处理 ==========

    // API: 完整状态
    void handleApiStatus() {
        String json;
        json.reserve(2048);
        json = "{";
        json += "\"system\":{";
        json += "\"chip_id\":\"0x" + String(ESP.getChipId(), HEX) + "\",";
        json += "\"cpu_freq\":" + String(ESP.getCpuFreqMHz()) + ",";
        json += "\"flash_size\":" + String(ESP.getFlashChipSize()) + ",";
        json += "\"free_heap\":" + String(ESP.getFreeHeap()) + ",";
        json += "\"uptime\":" + String(millis() / 1000);
        json += "},";
        json += "\"wifi\":{";
        json += "\"ssid\":\"" + String(WiFi.SSID()) + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
        json += "\"channel\":" + String(WiFi.channel());
        json += "},";
#ifdef ENABLE_DHT
        json += "\"dht\":{";
        if (_dhtMgr && _dhtMgr->isValid()) {
            json += "\"valid\":true,";
            json += "\"temperature\":" + String(_dhtMgr->getTemperature(), 1) + ",";
            json += "\"humidity\":" + String(_dhtMgr->getHumidity(), 1);
        } else {
            json += "\"valid\":false,";
            json += "\"temperature\":0,";
            json += "\"humidity\":0";
        }
        json += "},";
#endif
        json += "\"gpio\":[";
        for (int i = 0; i < GPIO_COUNT; i++) {
            if (i > 0) json += ",";
            int val = digitalRead(GPIO_LIST[i].pin);
            String mode = getPinModeStr(GPIO_LIST[i].pin);
            json += "{\"pin\":" + String(GPIO_LIST[i].pin) + ",";
            json += "\"name\":\"" + String(GPIO_LIST[i].name) + "\",";
            json += "\"label\":\"" + String(GPIO_LIST[i].label) + "\",";
            json += "\"value\":" + String(val) + ",";
            json += "\"mode\":\"" + mode + "\"}";
            yield();
        }
        json += "]";
        json += "}";
        _server.send(200, "application/json", json);
    }

    // API: GPIO状态
    void handleApiGpio() {
        String json;
        json.reserve(512);
        json = "[";
        for (int i = 0; i < GPIO_COUNT; i++) {
            if (i > 0) json += ",";
            int val = digitalRead(GPIO_LIST[i].pin);
            json += "{\"pin\":" + String(GPIO_LIST[i].pin) + ",";
            json += "\"value\":" + String(val) + "}";
        }
        json += "]";
        _server.send(200, "application/json", json);
    }

    // API: MQTT状态
    void handleApiMqttStatus() {
        if (!_mqttMgr) {
            _server.send(500, "application/json", "{\"error\":\"MQTT not initialized\"}");
            return;
        }
        MqttConfig cfg = _mqttMgr->getConfig();
        String json;
        json.reserve(512);
        json = "{";
        json += "\"connected\":" + String(_mqttMgr->isConnected() ? "true" : "false") + ",";
        json += "\"state\":" + String(_mqttMgr->getState()) + ",";
        json += "\"enabled\":" + String(cfg.enabled ? "true" : "false") + ",";
        json += "\"host\":\"" + String(cfg.host) + "\",";
        json += "\"port\":" + String(cfg.port) + ",";
        json += "\"username\":\"" + String(cfg.username) + "\",";
        json += "\"topic\":\"" + String(cfg.topic) + "\",";
        json += "\"client_id\":\"" + String(cfg.clientId) + "\",";
        json += "\"info_topic\":\"" + String(cfg.infoTopic) + "\",";
        json += "\"interval\":" + String(cfg.interval) + ",";
        json += "\"pin_mask\":" + String(cfg.pinMask);
        json += "}";
        _server.send(200, "application/json", json);
    }

    // API: 保存MQTT配置
    void handleApiMqttSave() {
        if (!_mqttMgr) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"MQTT not initialized\"}");
            return;
        }
        MqttConfig cfg = _mqttMgr->getConfig();
        if (_server.hasArg("host")) {
            String host = _server.arg("host");
            host.toCharArray(cfg.host, MQTT_HOST_MAX_LENGTH);
        }
        if (_server.hasArg("port")) {
            cfg.port = _server.arg("port").toInt();
        }
        if (_server.hasArg("username")) {
            String user = _server.arg("username");
            user.toCharArray(cfg.username, MQTT_USER_MAX_LENGTH);
        }
        if (_server.hasArg("password")) {
            String pass = _server.arg("password");
            pass.toCharArray(cfg.password, MQTT_PASS_MAX_LENGTH);
        }
        if (_server.hasArg("topic")) {
            String topic = _server.arg("topic");
            topic.toCharArray(cfg.topic, MQTT_TOPIC_MAX_LENGTH);
        }
        if (_server.hasArg("client_id")) {
            String clientId = _server.arg("client_id");
            clientId.toCharArray(cfg.clientId, MQTT_CLIENTID_MAX_LENGTH);
        }
        if (_server.hasArg("info_topic")) {
            String infoTopic = _server.arg("info_topic");
            infoTopic.toCharArray(cfg.infoTopic, MQTT_TOPIC_MAX_LENGTH);
        }
        if (_server.hasArg("interval")) {
            cfg.interval = _server.arg("interval").toInt();
        }
        if (_server.hasArg("enabled")) {
            cfg.enabled = _server.arg("enabled") == "1" || _server.arg("enabled") == "true";
        }
        if (_server.hasArg("pins")) {
            cfg.pinMask = 0;
            String pinsStr = _server.arg("pins");
            int idx = 0;
            while (idx < (int)pinsStr.length()) {
                int comma = pinsStr.indexOf(',', idx);
                String pinStr;
                if (comma == -1) {
                    pinStr = pinsStr.substring(idx);
                } else {
                    pinStr = pinsStr.substring(idx, comma);
                    idx = comma + 1;
                }
                int pin = pinStr.toInt();
                if (pin >= 0 && pin <= 16) {
                    cfg.pinMask |= (1UL << pin);
                }
                if (comma == -1) break;
            }
        }
        _mqttMgr->saveConfig(cfg);
        _server.send(200, "application/json", "{\"success\":true,\"msg\":\"配置已保存\"}");
    }

    // API: 测试MQTT
    void handleApiMqttTest() {
        if (!_mqttMgr) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"MQTT not initialized\"}");
            return;
        }
        if (_mqttMgr->isConnected()) {
            _mqttMgr->publishStatus();
            _server.send(200, "application/json", "{\"success\":true,\"msg\":\"状态上报已发送\"}");
        } else {
            String err = "MQTT未连接 (" + String(_mqttMgr->getState()) + ")";
            _server.send(200, "application/json", "{\"success\":false,\"msg\":\"" + err + "\"}");
        }
    }

    // API: 上报设备信息
    void handleApiMqttDeviceInfo() {
        if (!_mqttMgr) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"MQTT not initialized\"}");
            return;
        }
        if (_mqttMgr->isConnected()) {
            _mqttMgr->publishDeviceInfo();
            _server.send(200, "application/json", "{\"success\":true,\"msg\":\"设备信息已上报\"}");
        } else {
            String err = "MQTT未连接 (" + String(_mqttMgr->getState()) + ")";
            _server.send(200, "application/json", "{\"success\":false,\"msg\":\"" + err + "\"}");
        }
    }

    // API: MQTT上报日志
    void handleApiMqttLog() {
        if (!_mqttMgr) {
            _server.send(500, "application/json", "{\"error\":\"MQTT not initialized\"}");
            return;
        }
        String json;
        json.reserve(3072);
        json = "[";
        int count = _mqttMgr->getLogCount();
        for (int i = 0; i < count; i++) {
            if (i > 0) json += ",";
            MqttLogEntry entry = _mqttMgr->getLog(i);
            json += "{";
            json += "\"timestamp\":" + String(entry.timestamp) + ",";
            json += "\"time\":\"" + formatLogTime(entry.timestamp) + "\",";
            json += "\"success\":" + String(entry.success ? "true" : "false") + ",";
            json += "\"topic\":\"" + String(entry.topic) + "\",";
            json += "\"msg\":\"" + String(entry.message) + "\"";
            json += "}";
            yield();
        }
        json += "]";
        _server.send(200, "application/json", json);
    }

    // 格式化日志时间
    String formatLogTime(uint32_t uptimeSec) {
        if (_timeMgr && _timeMgr->isSynced()) {
            uint32_t nowSec = millis() / 1000;
            uint32_t diff = (nowSec >= uptimeSec) ? (nowSec - uptimeSec) : 0;
            uint32_t ts = _timeMgr->getTimestamp();
            if (ts > diff) {
                return _timeMgr->formatTime(ts - diff);
            }
            return String("计算中");
        } else {
            uint32_t s = uptimeSec;
            uint32_t h = s / 3600;
            s %= 3600;
            uint32_t m = s / 60;
            s %= 60;
            char buf[16];
            snprintf(buf, sizeof(buf), "+%02u:%02u:%02u",
                     (unsigned)h, (unsigned)m, (unsigned)s);
            return String(buf);
        }
    }

    // API: GPIO控制
    void handleApiGpioControl() {
        if (!_server.hasArg("pin")) {
            _server.send(400, "application/json", "{\"success\":false,\"msg\":\"缺少pin参数\"}");
            return;
        }
        int pin = _server.arg("pin").toInt();
        if (pin < 0 || pin > 16 || pin == 6 || pin == 7 || pin == 8 || pin == 11) {
            _server.send(400, "application/json", "{\"success\":false,\"msg\":\"无效的引脚\"}");
            return;
        }
        if (_server.hasArg("mode")) {
            String mode = _server.arg("mode");
            if (mode == "output") {
                setGpioMode(pin, OUTPUT);
                DEBUG_PRINTF("[GPIO] GPIO%d 设置为输出模式\r\n", pin);
            } else if (mode == "input") {
                setGpioMode(pin, INPUT);
                DEBUG_PRINTF("[GPIO] GPIO%d 设置为输入模式\r\n", pin);
            } else if (mode == "input_pullup") {
                setGpioMode(pin, INPUT_PULLUP);
                DEBUG_PRINTF("[GPIO] GPIO%d 设置为输入上拉模式\r\n", pin);
            }
        }
        if (_server.hasArg("value")) {
            int val = _server.arg("value").toInt();
            digitalWrite(pin, val ? HIGH : LOW);
            DEBUG_PRINTF("[GPIO] GPIO%d 输出 %s\r\n", pin, val ? "高电平" : "低电平");
        }
        int val = digitalRead(pin);
        String modeStr = getPinModeStr(pin);
        String json = "{\"success\":true,\"pin\":" + String(pin) + ",\"mode\":\"" + modeStr + "\",\"value\":" + String(val) + "}";
        _server.send(200, "application/json", json);
    }

    // API: 获取当前时间
    void handleApiTime() {
        String json;
        json.reserve(256);
        json = "{";
        if (_timeMgr && _timeMgr->isSynced()) {
            json += "\"synced\":true,";
            json += "\"datetime\":\"" + _timeMgr->getDateTimeStr() + "\",";
            json += "\"date\":\"" + _timeMgr->getDateStr() + "\",";
            json += "\"time\":\"" + _timeMgr->getTimeStr() + "\",";
            json += "\"timestamp\":" + String(_timeMgr->getTimestamp());
        } else {
            json += "\"synced\":false,";
            json += "\"datetime\":\"未同步\",";
            json += "\"date\":\"\",";
            json += "\"time\":\"\",";
            json += "\"timestamp\":0";
        }
        json += "}";
        _server.send(200, "application/json", json);
    }

    // API: 获取NTP配置状态
    void handleApiNtpStatus() {
        if (!_timeMgr) {
            _server.send(500, "application/json", "{\"error\":\"TimeManager not initialized\"}");
            return;
        }
        NtpConfig cfg = _timeMgr->getConfig();
        String json;
        json.reserve(256);
        json = "{";
        json += "\"synced\":" + String(_timeMgr->isSynced() ? "true" : "false") + ",";
        json += "\"server1\":\"" + String(cfg.server1) + "\",";
        json += "\"server2\":\"" + String(cfg.server2) + "\",";
        json += "\"server3\":\"" + String(cfg.server3) + "\",";
        json += "\"timezone\":" + String(cfg.timezone) + ",";
        json += "\"interval_min\":" + String(cfg.interval / 60000);
        json += "}";
        _server.send(200, "application/json", json);
    }

    // API: 保存NTP配置
    void handleApiNtpSave() {
        if (!_timeMgr) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"TimeManager not initialized\"}");
            return;
        }
        NtpConfig cfg = _timeMgr->getConfig();
        if (_server.hasArg("server1")) {
            String s = _server.arg("server1");
            if (s.length() > 0 && s.length() < 32) {
                s.toCharArray(cfg.server1, 32);
            }
        }
        if (_server.hasArg("server2")) {
            String s = _server.arg("server2");
            if (s.length() < 24) {
                memset(cfg.server2, 0, sizeof(cfg.server2));
                if (s.length() > 0) s.toCharArray(cfg.server2, 24);
            }
        }
        if (_server.hasArg("server3")) {
            String s = _server.arg("server3");
            if (s.length() < 24) {
                memset(cfg.server3, 0, sizeof(cfg.server3));
                if (s.length() > 0) s.toCharArray(cfg.server3, 24);
            }
        }
        if (_server.hasArg("timezone")) {
            int tz = _server.arg("timezone").toInt();
            if (tz >= -12 && tz <= 12) {
                cfg.timezone = (int8_t)tz;
            }
        }
        if (_server.hasArg("interval_min")) {
            int mins = _server.arg("interval_min").toInt();
            if (mins >= 5 && mins <= 1440) {
                cfg.interval = (uint32_t)mins * 60000;
            }
        }
        _timeMgr->saveConfig(cfg);
        _server.send(200, "application/json", "{\"success\":true,\"msg\":\"NTP配置已保存\"}");
    }

    // API: 手动同步时间
    void handleApiNtpSync() {
        if (!_timeMgr) {
            _server.send(500, "application/json", "{\"success\":false,\"msg\":\"TimeManager not initialized\"}");
            return;
        }
        bool ok = _timeMgr->forceSync();
        if (ok) {
            _server.send(200, "application/json", "{\"success\":true,\"msg\":\"同步成功：" + _timeMgr->getDateTimeStr() + "\"}");
        } else {
            _server.send(200, "application/json", "{\"success\":false,\"msg\":\"同步中，请稍后刷新查看\"}");
        }
    }

    // 404
    void handleNotFound() {
        _server.send(404, "text/plain", "Not Found");
    }

    // 格式化运行时间
    String formatUptime(unsigned long seconds) {
        unsigned long days = seconds / 86400;
        unsigned long hours = (seconds % 86400) / 3600;
        unsigned long mins = (seconds % 3600) / 60;
        unsigned long secs = seconds % 60;
        String result;
        if (days > 0) result += String(days) + "天 ";
        if (hours > 0 || days > 0) result += String(hours) + "时";
        result += String(mins) + "分" + String(secs) + "秒";
        return result;
    }
};

#endif // DEVICE_STATUS_WEB_H
