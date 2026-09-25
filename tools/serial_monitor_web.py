"""
ESP8266 Web串口调试工具
基于 Web 的串口监视器，通过浏览器访问，支持实时查看串口数据
"""

import serial
import serial.tools.list_ports
from flask import Flask, render_template_string, jsonify, request
from datetime import datetime
import threading
import time
import queue

app = Flask(__name__)

# 全局变量
ser = None
is_connected = False
serial_buffer = queue.Queue(maxsize=1000)
rx_count = 0
tx_count = 0
read_thread = None
stop_reading = False

# HTML 模板
HTML_TEMPLATE = """
<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP8266 串口调试工具</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: #1e1e1e;
            color: #d4d4d4;
            min-height: 100vh;
            padding: 20px;
        }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 {
            text-align: center;
            margin-bottom: 20px;
            color: #569cd6;
            font-size: 24px;
        }
        .toolbar {
            background: #252526;
            padding: 15px;
            border-radius: 8px;
            margin-bottom: 15px;
            display: flex;
            flex-wrap: wrap;
            gap: 10px;
            align-items: center;
        }
        .toolbar label { color: #999; font-size: 13px; }
        select, input[type="text"] {
            background: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555;
            padding: 8px 12px;
            border-radius: 5px;
            font-size: 13px;
        }
        select:focus, input:focus { outline: none; border-color: #007acc; }
        button {
            padding: 8px 16px;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            font-size: 13px;
            font-weight: 500;
            transition: all 0.2s;
        }
        .btn-primary { background: #0e639c; color: white; }
        .btn-primary:hover { background: #1177bb; }
        .btn-danger { background: #c62828; color: white; }
        .btn-danger:hover { background: #e53935; }
        .btn-default { background: #3c3c3c; color: #d4d4d4; }
        .btn-default:hover { background: #4a4a4a; }
        .status {
            display: flex;
            align-items: center;
            gap: 8px;
            margin-left: auto;
        }
        .status-dot {
            width: 12px;
            height: 12px;
            border-radius: 50%;
            background: #666;
        }
        .status-dot.connected { background: #4caf50; box-shadow: 0 0 8px #4caf50; }
        .status-text { font-size: 13px; color: #999; }
        .options {
            background: #252526;
            padding: 10px 15px;
            border-radius: 8px;
            margin-bottom: 15px;
            display: flex;
            gap: 20px;
            flex-wrap: wrap;
        }
        .options label {
            display: flex;
            align-items: center;
            gap: 5px;
            font-size: 13px;
            color: #999;
            cursor: pointer;
        }
        .options input[type="checkbox"] { cursor: pointer; }
        .terminal {
            background: #1e1e1e;
            border: 1px solid #3c3c3c;
            border-radius: 8px;
            padding: 15px;
            height: 500px;
            overflow-y: auto;
            font-family: 'Consolas', 'Monaco', monospace;
            font-size: 13px;
            line-height: 1.5;
            margin-bottom: 15px;
        }
        .terminal-line { white-space: pre-wrap; word-break: break-all; }
        .timestamp { color: #666; }
        .data { color: #d4d4d4; }
        .send-data { color: #4ec9b0; }
        .system { color: #569cd6; }
        .error { color: #f44747; }
        .send-area {
            background: #252526;
            padding: 15px;
            border-radius: 8px;
            display: flex;
            gap: 10px;
        }
        .send-area input {
            flex: 1;
            background: #3c3c3c;
            color: #d4d4d4;
            border: 1px solid #555;
            padding: 10px 15px;
            border-radius: 5px;
            font-size: 13px;
        }
        .stats {
            margin-top: 10px;
            text-align: center;
            font-size: 12px;
            color: #666;
        }
        .stats span { margin: 0 15px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>&#128225; ESP8266 串口调试工具</h1>

        <div class="toolbar">
            <label>端口:</label>
            <select id="portSelect">
                <option value="">加载中...</option>
            </select>

            <button class="btn-default" onclick="refreshPorts()">刷新</button>

            <label>波特率:</label>
            <select id="baudSelect">
                <option value="9600">9600</option>
                <option value="19200">19200</option>
                <option value="38400">38400</option>
                <option value="57600">57600</option>
                <option value="115200" selected>115200</option>
                <option value="230400">230400</option>
                <option value="256000">256000</option>
                <option value="460800">460800</option>
                <option value="921600">921600</option>
            </select>

            <button id="connectBtn" class="btn-primary" onclick="toggleConnect()">连接</button>

            <div class="status">
                <div id="statusDot" class="status-dot"></div>
                <span id="statusText" class="status-text">未连接</span>
            </div>
        </div>

        <div class="options">
            <label><input type="checkbox" id="showTimestamp" checked> 显示时间戳</label>
            <label><input type="checkbox" id="autoScroll" checked> 自动滚动</label>
            <label><input type="checkbox" id="hexDisplay"> HEX显示</label>
            <button class="btn-default" style="padding: 4px 12px; font-size: 12px;" onclick="clearTerminal()">清除</button>
        </div>

        <div id="terminal" class="terminal"></div>

        <div class="send-area">
            <input type="text" id="sendInput" placeholder="输入要发送的数据，按回车发送..." onkeypress="handleKeyPress(event)">
            <button class="btn-primary" onclick="sendData()">发送</button>
        </div>

        <div class="stats">
            <span>接收: <span id="rxCount">0</span> 字节</span>
            <span>发送: <span id="txCount">0</span> 字节</span>
        </div>
    </div>

    <script>
        let isConnected = false;
        let pollInterval = null;
        let lastLineId = 0;

        // 页面加载时刷新端口列表
        window.onload = function() {
            refreshPorts();
        };

        // 刷新端口列表
        function refreshPorts() {
            fetch('/api/ports')
                .then(r => r.json())
                .then(data => {
                    const select = document.getElementById('portSelect');
                    select.innerHTML = '';
                    if (data.ports.length === 0) {
                        select.innerHTML = '<option value="">未发现串口</option>';
                    } else {
                        data.ports.forEach(port => {
                            const option = document.createElement('option');
                            option.value = port;
                            option.textContent = port;
                            select.appendChild(option);
                        });
                    }
                });
        }

        // 连接/断开
        function toggleConnect() {
            if (isConnected) {
                disconnect();
            } else {
                connect();
            }
        }

        // 连接
        function connect() {
            const port = document.getElementById('portSelect').value;
            const baud = document.getElementById('baudSelect').value;

            if (!port) {
                alert('请选择串口！');
                return;
            }

            fetch('/api/connect', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ port, baud: parseInt(baud) })
            })
            .then(r => r.json())
            .then(data => {
                if (data.success) {
                    isConnected = true;
                    updateStatus(true, port + ' @ ' + baud);
                    document.getElementById('connectBtn').textContent = '断开';
                    document.getElementById('connectBtn').className = 'btn-danger';
                    document.getElementById('portSelect').disabled = true;
                    document.getElementById('baudSelect').disabled = true;
                    addLine('[系统] 已连接', 'system');
                    startPolling();
                } else {
                    alert('连接失败: ' + data.error);
                }
            });
        }

        // 断开
        function disconnect() {
            fetch('/api/disconnect', { method: 'POST' })
            .then(r => r.json())
            .then(data => {
                isConnected = false;
                stopPolling();
                updateStatus(false, '未连接');
                document.getElementById('connectBtn').textContent = '连接';
                document.getElementById('connectBtn').className = 'btn-primary';
                document.getElementById('portSelect').disabled = false;
                document.getElementById('baudSelect').disabled = false;
                addLine('[系统] 已断开', 'system');
            });
        }

        // 更新状态显示
        function updateStatus(connected, text) {
            const dot = document.getElementById('statusDot');
            const label = document.getElementById('statusText');
            dot.className = 'status-dot' + (connected ? ' connected' : '');
            label.textContent = text;
        }

        // 开始轮询数据
        function startPolling() {
            pollInterval = setInterval(pollData, 100);
        }

        // 停止轮询
        function stopPolling() {
            if (pollInterval) {
                clearInterval(pollInterval);
                pollInterval = null;
            }
        }

        // 轮询数据
        function pollData() {
            fetch('/api/data')
                .then(r => r.json())
                .then(data => {
                    if (data.lines && data.lines.length > 0) {
                        data.lines.forEach(line => {
                            addLine(line.text, line.type);
                        });
                    }
                    document.getElementById('rxCount').textContent = data.rx_count;
                    document.getElementById('txCount').textContent = data.tx_count;
                })
                .catch(err => {
                    // 忽略轮询错误
                });
        }

        // 添加一行到终端
        function addLine(text, type) {
            const terminal = document.getElementById('terminal');
            const div = document.createElement('div');
            div.className = 'terminal-line';

            const showTimestamp = document.getElementById('showTimestamp').checked;
            if (showTimestamp && text.trim()) {
                const now = new Date();
                const ts = now.toTimeString().split(' ')[0] + '.' +
                          String(now.getMilliseconds()).padStart(3, '0');
                div.innerHTML = '<span class="timestamp">[' + ts + ']</span> ';
            }

            div.innerHTML += '<span class="' + type + '">' + escapeHtml(text) + '</span>';
            terminal.appendChild(div);

            // 自动滚动
            if (document.getElementById('autoScroll').checked) {
                terminal.scrollTop = terminal.scrollHeight;
            }
        }

        // HTML转义
        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        // 发送数据
        function sendData() {
            if (!isConnected) {
                alert('请先连接串口！');
                return;
            }

            const input = document.getElementById('sendInput');
            const data = input.value;
            if (!data) return;

            fetch('/api/send', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ data })
            })
            .then(r => r.json())
            .then(result => {
                if (result.success) {
                    addLine('>>> ' + data, 'send-data');
                    input.value = '';
                }
            });
        }

        // 回车发送
        function handleKeyPress(e) {
            if (e.key === 'Enter') {
                sendData();
            }
        }

        // 清除终端
        function clearTerminal() {
            document.getElementById('terminal').innerHTML = '';
        }
    </script>
</body>
</html>
"""


def format_timestamp():
    return datetime.now().strftime("%H:%M:%S.%f")[:-3]


def add_line(text, line_type="data"):
    """添加一行到缓冲区"""
    global serial_buffer
    try:
        serial_buffer.put_nowait({"text": text, "type": line_type, "ts": format_timestamp()})
    except queue.Full:
        # 缓冲区满，丢弃最旧的
        try:
            serial_buffer.get_nowait()
            serial_buffer.put_nowait({"text": text, "type": line_type, "ts": format_timestamp()})
        except:
            pass


def read_serial_loop():
    """读取串口数据的线程"""
    global ser, is_connected, rx_count, stop_reading

    buffer = bytearray()

    while not stop_reading and ser and ser.is_open:
        try:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                if data:
                    rx_count += len(data)

                    # 按行处理
                    buffer.extend(data)
                    while b'\n' in buffer:
                        line, buffer = buffer.split(b'\n', 1)
                        try:
                            text = line.decode('utf-8', errors='replace')
                            add_line(text + '\n', "data")
                        except:
                            pass

                    # 缓冲区太大就强制输出
                    if len(buffer) > 512:
                        try:
                            text = buffer.decode('utf-8', errors='replace')
                            add_line(text, "data")
                        except:
                            pass
                        buffer.clear()
            else:
                time.sleep(0.02)
        except Exception as e:
            if not stop_reading:
                add_line(f"\n[错误] 读取异常: {str(e)}\n", "error")
            break


@app.route('/')
def index():
    return render_template_string(HTML_TEMPLATE)


@app.route('/api/ports')
def get_ports():
    ports = serial.tools.list_ports.comports()
    port_list = [port.device for port in ports]
    return jsonify({"ports": port_list})


@app.route('/api/connect', methods=['POST'])
def connect():
    global ser, is_connected, read_thread, stop_reading, rx_count, tx_count

    data = request.json
    port = data.get('port')
    baud = data.get('baud', 115200)

    if not port:
        return jsonify({"success": False, "error": "请选择串口"})

    try:
        # 断开现有连接
        if ser and ser.is_open:
            stop_reading = True
            time.sleep(0.2)
            ser.close()

        ser = serial.Serial(
            port=port,
            baudrate=baud,
            timeout=1
        )

        is_connected = True
        stop_reading = False
        rx_count = 0
        tx_count = 0

        # 清空缓冲区
        while not serial_buffer.empty():
            serial_buffer.get_nowait()

        # 启动读取线程
        read_thread = threading.Thread(target=read_serial_loop, daemon=True)
        read_thread.start()

        add_line(f"[系统] 已连接 {port} @ {baud} bps\n", "system")

        return jsonify({"success": True})

    except Exception as e:
        return jsonify({"success": False, "error": str(e)})


@app.route('/api/disconnect', methods=['POST'])
def disconnect():
    global ser, is_connected, stop_reading

    stop_reading = True

    if ser and ser.is_open:
        ser.close()

    is_connected = False
    add_line("[系统] 已断开连接\n", "system")

    return jsonify({"success": True})


@app.route('/api/data')
def get_data():
    """获取缓冲区中的数据"""
    global serial_buffer, rx_count, tx_count

    lines = []
    while not serial_buffer.empty() and len(lines) < 100:
        try:
            line = serial_buffer.get_nowait()
            lines.append(line)
        except queue.Empty:
            break

    return jsonify({
        "lines": lines,
        "rx_count": rx_count,
        "tx_count": tx_count
    })


@app.route('/api/send', methods=['POST'])
def send_data():
    global ser, tx_count

    if not ser or not ser.is_open:
        return jsonify({"success": False, "error": "未连接"})

    data = request.json.get('data', '')
    if not data:
        return jsonify({"success": False, "error": "数据为空"})

    try:
        # 发送数据（加回车换行）
        send_bytes = (data + '\r\n').encode('utf-8')
        ser.write(send_bytes)
        tx_count += len(send_bytes)
        return jsonify({"success": True})
    except Exception as e:
        return jsonify({"success": False, "error": str(e)})


def main():
    print("=" * 50)
    print("  ESP8266 Web串口调试工具")
    print("=" * 50)
    print()
    print("正在启动 Web 服务器...")
    print("请在浏览器中访问: http://localhost:5000")
    print()
    print("按 Ctrl+C 停止")
    print("=" * 50)
    print()

    try:
        app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)
    except KeyboardInterrupt:
        print("\n正在停止...")
        if ser and ser.is_open:
            ser.close()
        print("已停止")


if __name__ == "__main__":
    main()
