/*
 * ConfigPortal - 配网门户模块
 * 提供Web配网页面，让用户选择WiFi并输入密码
 */

#ifndef CONFIG_PORTAL_H
#define CONFIG_PORTAL_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "WifiManager.h"
#include "LedStatus.h"

// 配网结果回调函数类型
typedef std::function<void(const char* ssid, const char* password)> ConfigCallback;

class ConfigPortal {
public:
    ConfigPortal(WifiManager& wifiMgr, LedStatus& led)
        : _wifiMgr(wifiMgr), _led(led), _server(WEB_SERVER_PORT), _configCallback(nullptr) {}

    void begin(ConfigCallback callback) {
        _configCallback = callback;

        // 启动AP模式
        startAP();

        // 配置Web服务器路由
        _server.on("/", std::bind(&ConfigPortal::handleRoot, this));
        _server.on("/scan", std::bind(&ConfigPortal::handleScan, this));
        _server.on("/scan-result", std::bind(&ConfigPortal::handleScanResult, this));
        _server.on("/connect", HTTP_POST, std::bind(&ConfigPortal::handleConnect, this));
        _server.on("/status", std::bind(&ConfigPortal::handleStatus, this));
        _server.onNotFound(std::bind(&ConfigPortal::handleNotFound, this));

        _server.begin();
        DEBUG_PRINTLN("[Portal] 配网Web服务器已启动");
        DEBUG_PRINTF("[Portal] 请连接WiFi: %s\r\n", AP_SSID);
        DEBUG_PRINTLN("[Portal] 然后访问: http://192.168.4.1");
    }

    void stop() {
        _server.stop();
        WiFi.softAPdisconnect(true);
        DEBUG_PRINTLN("[Portal] 配网服务器已停止");
    }

    // 在loop中调用处理客户端请求
    void handleClient() {
        _server.handleClient();
    }

private:
    ESP8266WebServer _server;
    WifiManager& _wifiMgr;
    LedStatus& _led;
    ConfigCallback _configCallback;

    // 启动AP模式
    void startAP() {
        DEBUG_PRINTLN("[Portal] 启动AP模式...");

        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        delay(200);

        // 使用AP+STA模式（兼容性更好）
        WiFi.mode(WIFI_AP_STA);
        delay(100);

        WiFi.setOutputPower(20.5);

        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        delay(100);

        const char* pwd = (strlen(AP_PASSWORD) > 0) ? AP_PASSWORD : "";
        bool success = WiFi.softAP(AP_SSID, pwd, 6, 0, AP_MAX_CONNECTIONS);

        if (success) {
            delay(500);
            DEBUG_PRINTF("[Portal] AP启动成功: %s\r\n", AP_SSID);
            DEBUG_PRINTF("[Portal] AP IP地址: %s\r\n", WiFi.softAPIP().toString().c_str());
            DEBUG_PRINTF("[Portal] AP信道: 6, 模式: AP+STA\r\n");
        } else {
            DEBUG_PRINTLN("[Portal] AP启动失败！");
        }
    }

    // 主页
    void handleRoot() {
        DEBUG_PRINTLN("[Portal] 访问配网主页");
        String html = getConfigPageHTML();
        _server.send(200, "text/html", html);
    }

    // 启动扫描（异步）
    void handleScan() {
        DEBUG_PRINTLN("[Portal] 收到扫描请求");
        
        // 启动异步扫描
        WiFi.scanDelete();
        delay(10);
        bool ok = WiFi.scanNetworks(true, false); // true=异步
        
        if (ok) {
            DEBUG_PRINTLN("[Portal] 异步扫描已启动");
            _server.send(200, "application/json", "{\"ok\":true}");
        } else {
            DEBUG_PRINTLN("[Portal] 扫描启动失败");
            _server.send(200, "application/json", "{\"ok\":false}");
        }
    }

    // 获取扫描结果
    void handleScanResult() {
        int n = WiFi.scanComplete();
        
        String json = "{";
        json += "\"status\":" + String(n) + ",";
        
        if (n >= 0) {
            // 扫描完成
            DEBUG_PRINTF("[Portal] 扫描完成，发现%d个WiFi\r\n", n);
            
            // 构建WiFi列表JSON
            json += "\"networks\":[";
            for (int i = 0; i < n; i++) {
                if (i > 0) json += ",";
                String ssid = WiFi.SSID(i);
                ssid.replace("\"", "\\\"");
                int rssi = WiFi.RSSI(i);
                int enc = WiFi.encryptionType(i);
                json += "{\"ssid\":\"" + ssid + "\",\"rssi\":" + String(rssi) + ",\"enc\":" + String(enc) + "}";
            }
            json += "]";
        } else if (n == -1) {
            json += "\"msg\":\"scanning\"";
        } else {
            json += "\"msg\":\"idle\"";
        }
        
        json += "}";
        _server.send(200, "application/json", json);
    }

    // 连接WiFi
    void handleConnect() {
        DEBUG_PRINTLN("[Portal] 收到连接请求");

        if (!_server.hasArg("ssid")) {
            _server.send(400, "application/json", "{\"success\":false,\"message\":\"SSID不能为空\"}");
            return;
        }

        String ssid = _server.arg("ssid");
        String password = _server.arg("password");

        DEBUG_PRINTF("[Portal] 尝试连接: %s\r\n", ssid.c_str());

        _server.send(200, "application/json", "{\"success\":true,\"message\":\"正在连接...\"}");
        delay(100);

        if (_configCallback) {
            _configCallback(ssid.c_str(), password.c_str());
        }
    }

    // 查询连接状态
    void handleStatus() {
        bool connected = _wifiMgr.isConnected();
        String ip = _wifiMgr.getIP();

        String json = "{";
        json += "\"connected\":" + String(connected ? "true" : "false") + ",";
        json += "\"ip\":\"" + ip + "\"";
        json += "}";

        _server.send(200, "application/json", json);
    }

    // 404
    void handleNotFound() {
        _server.send(404, "text/plain", "Not Found");
    }

    // 配网页面HTML
    String getConfigPageHTML() {
        String html = "<!DOCTYPE html><html lang='zh-CN'><head>";
        html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
        html += "<title>ESP8266 WiFi配网</title>";
        html += "<style>";
        html += "*{box-sizing:border-box;margin:0;padding:0}";
        html += "body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea,#764ba2);min-height:100vh;display:flex;justify-content:center;align-items:center;padding:20px}";
        html += ".card{background:#fff;border-radius:16px;box-shadow:0 10px 40px rgba(0,0,0,.3);width:100%;max-width:420px;overflow:hidden}";
        html += ".head{background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;padding:24px;text-align:center}";
        html += ".head h1{font-size:22px;margin-bottom:4px}";
        html += ".head p{opacity:.85;font-size:13px}";
        html += ".body{padding:20px}";
        html += ".msg{padding:10px 14px;border-radius:8px;margin-bottom:14px;font-size:13px;background:#e3f2fd;color:#1565c0}";
        html += ".msg.err{background:#ffebee;color:#c62828}";
        html += ".msg.ok{background:#e8f5e9;color:#2e7d32}";
        html += ".btn{width:100%;padding:12px;background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;border:none;border-radius:10px;font-size:15px;font-weight:600;cursor:pointer}";
        html += ".btn:disabled{opacity:.5;cursor:not-allowed}";
        html += ".btn-scan{background:#f0f0f0;color:#667eea;margin-bottom:12px}";
        html += ".list{max-height:220px;overflow-y:auto;border:1px solid #e0e0e0;border-radius:10px;margin-bottom:14px}";
        html += ".item{padding:10px 14px;border-bottom:1px solid #f0f0f0;cursor:pointer;display:flex;justify-content:space-between;align-items:center;font-size:14px}";
        html += ".item:hover{background:#f8f9ff}";
        html += ".item:last-child{border-bottom:none}";
        html += ".name{color:#333;font-weight:500}";
        html += ".sig{color:#999;font-size:12px}";
        html += ".lock{margin-left:6px}";
        html += ".field{margin-bottom:14px}";
        html += ".field label{display:block;margin-bottom:6px;color:#333;font-size:13px;font-weight:500}";
        html += ".field input{width:100%;padding:10px 12px;border:2px solid #e0e0e0;border-radius:8px;font-size:14px}";
        html += ".field input:focus{outline:none;border-color:#667eea}";
        html += ".loading{text-align:center;padding:20px;color:#999}";
        html += ".spin{display:inline-block;width:28px;height:28px;border:3px solid #f3f3f3;border-top-color:#667eea;border-radius:50%;animation:spin 1s linear infinite;margin-bottom:8px}";
        html += "@keyframes spin{to{transform:rotate(360deg)}}";
        html += ".success{text-align:center;display:none}";
        html += ".success .icon{font-size:48px;margin-bottom:8px}";
        html += ".success h3{color:#2e7d32;margin-bottom:6px}";
        html += ".success p{color:#666;margin-bottom:10px;font-size:14px}";
        html += ".ip{background:#f8f9ff;padding:8px;border-radius:6px;font-family:monospace;color:#667eea;font-size:15px}";
        html += "</style></head><body>";
        html += "<div class='card'><div class='head'><h1>&#128246; WiFi 配网</h1><p>ESP8266 智能配置</p></div>";
        html += "<div class='body'>";
        html += "<div id='msg' class='msg'>请选择要连接的WiFi网络</div>";
        html += "<button class='btn btn-scan' onclick='scanWifi()' id='scanBtn'>&#128269; 扫描WiFi网络</button>";
        html += "<div class='list' id='wifiList'><div class='loading'>点击上方按钮扫描WiFi</div></div>";
        html += "<div class='field'><label>WiFi名称</label><input type='text' id='ssid' placeholder='输入或选择WiFi名称'></div>";
        html += "<div class='field'><label>WiFi密码</label><input type='password' id='password' placeholder='输入WiFi密码'></div>";
        html += "<button class='btn' onclick='connectWifi()' id='connectBtn'>&#128279; 连接WiFi</button>";
        html += "<div id='loading' class='loading' style='display:none'><div class='spin'></div>正在连接，请稍候...</div>";
        html += "<div id='success' class='success'><div class='icon'>&#9989;</div><h3>连接成功！</h3><p>设备已成功连接到WiFi</p><div class='ip' id='ipAddr'>-</div></div>";
        html += "</div></div>";
        
        html += "<script>";
        html += "function showMsg(text,type){";
        html += "var m=document.getElementById('msg');m.className='msg'+(type?' '+type:'');m.textContent=text}";
        html += "";
        html += "function selectWifi(ssid,enc){";
        html += "document.getElementById('ssid').value=ssid;";
        html += "if(enc)document.getElementById('password').focus();";
        html += "showMsg('已选择: '+ssid,'')}";
        html += "";
        html += "var scanTimer=null;";
        html += "var scanTries=0;";
        html += "";
        html += "function scanWifi(){";
        html += "var btn=document.getElementById('scanBtn');";
        html += "var list=document.getElementById('wifiList');";
        html += "btn.disabled=true;btn.textContent='扫描中...';";
        html += "list.innerHTML=\"<div class='loading'><div class='spin'></div>正在扫描WiFi网络...</div>\";";
        html += "scanTries=0;";
        html += "fetch('/scan').then(r=>r.json()).then(d=>{";
        html += "if(d.ok){pollScan()}else{showMsg('扫描启动失败','err');btn.disabled=false;btn.textContent='重新扫描'}";
        html += "}).catch(e=>{showMsg('请求失败','err');btn.disabled=false;btn.textContent='重新扫描'})";
        html += "}";
        html += "";
        html += "function pollScan(){";
        html += "scanTries++;";
        html += "if(scanTries>30){document.getElementById('wifiList').innerHTML=\"<div class='loading'>扫描超时</div>\";document.getElementById('scanBtn').disabled=false;document.getElementById('scanBtn').textContent='重新扫描';return}";
        html += "fetch('/scan-result').then(r=>r.json()).then(d=>{";
        html += "if(d.status>=0){renderList(d.networks);document.getElementById('scanBtn').disabled=false;document.getElementById('scanBtn').textContent='重新扫描';showMsg('发现 '+d.status+' 个WiFi','')}";
        html += "else{setTimeout(pollScan,500)}}).catch(e=>{setTimeout(pollScan,500)})";
        html += "}";
        html += "";
        html += "function renderList(nets){";
        html += "var list=document.getElementById('wifiList');";
        html += "if(!nets||nets.length==0){list.innerHTML=\"<div class='loading'>未扫描到WiFi</div>\";return}";
        html += "nets.sort(function(a,b){return b.rssi-a.rssi});";
        html += "var html='';";
        html += "for(var i=0;i<nets.length;i++){";
        html += "var n=nets[i];";
        html += "var locked=n.enc!=0;";
        html += "html+=\"<div class='item' onclick=selectWifi('\"+n.ssid+\"',\"+locked+\")><span class='name'>\"+n.ssid+\"</span><span class='sig'>\"+n.rssi+' dBm'+(locked?' &#128274;':'')+\"</span></div>\";}";
        html += "list.innerHTML=html}";
        html += "";
        html += "function connectWifi(){";
        html += "var ssid=document.getElementById('ssid').value.trim();";
        html += "var pwd=document.getElementById('password').value;";
        html += "if(!ssid){showMsg('请输入WiFi名称','err');return}";
        html += "document.getElementById('loading').style.display='block';";
        html += "document.getElementById('success').style.display='none';";
        html += "document.getElementById('connectBtn').disabled=true;";
        html += "showMsg('正在连接到 '+ssid+'...','');";
        html += "var fd=new FormData();fd.append('ssid',ssid);fd.append('password',pwd);";
        html += "fetch('/connect',{method:'POST',body:fd}).then(r=>r.json()).then(d=>{";
        html += "if(d.success){pollConn()}else{document.getElementById('loading').style.display='none';document.getElementById('connectBtn').disabled=false;showMsg(d.message,'err')}";
        html += "}).catch(e=>{document.getElementById('loading').style.display='none';document.getElementById('connectBtn').disabled=false;showMsg('请求失败','err')})";
        html += "}";
        html += "";
        html += "var connTries=0;";
        html += "function pollConn(){";
        html += "connTries++;if(connTries>30){document.getElementById('loading').style.display='none';document.getElementById('connectBtn').disabled=false;showMsg('连接超时，请检查密码','err');connTries=0;return}";
        html += "fetch('/status').then(r=>r.json()).then(d=>{";
        html += "if(d.connected){document.getElementById('loading').style.display='none';document.getElementById('success').style.display='block';document.getElementById('ipAddr').textContent=d.ip;showMsg('连接成功！','ok');connTries=0}else{setTimeout(pollConn,1000)}}).catch(e=>{setTimeout(pollConn,1000)})";
        html += "}";
        html += "</script>";
        html += "</body></html>";

        return html;
    }
};

#endif // CONFIG_PORTAL_H
