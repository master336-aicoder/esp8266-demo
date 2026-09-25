"""
ESP8266 串口调试工具
图形化串口监视器，支持自动滚动、时间戳、HEX显示等功能
"""

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox
import serial
import serial.tools.list_ports
import threading
import time
from datetime import datetime


class SerialMonitor:
    def __init__(self, root):
        self.root = root
        self.root.title("ESP8266 串口调试工具")
        self.root.geometry("800x600")
        self.root.minsize(600, 400)

        # 串口对象
        self.ser = None
        self.is_connected = False
        self.read_thread = None
        self.stop_reading = False

        # 配置变量
        self.port_var = tk.StringVar()
        self.baud_var = tk.StringVar(value="115200")
        self.timestamp_var = tk.BooleanVar(value=True)
        self.hex_var = tk.BooleanVar(value=False)
        self.auto_scroll_var = tk.BooleanVar(value=True)
        self.send_hex_var = tk.BooleanVar(value=False)

        self.setup_ui()
        self.refresh_ports()

    def setup_ui(self):
        # 顶部工具栏
        toolbar = ttk.Frame(self.root, padding="5")
        toolbar.pack(fill=tk.X)

        # 端口选择
        ttk.Label(toolbar, text="端口:").pack(side=tk.LEFT, padx=2)
        self.port_combo = ttk.Combobox(toolbar, textvariable=self.port_var, width=15, state="readonly")
        self.port_combo.pack(side=tk.LEFT, padx=2)

        ttk.Button(toolbar, text="刷新", width=6, command=self.refresh_ports).pack(side=tk.LEFT, padx=2)

        # 波特率
        ttk.Label(toolbar, text="波特率:").pack(side=tk.LEFT, padx=2)
        baud_rates = ["9600", "19200", "38400", "57600", "115200", "230400", "256000", "460800", "921600"]
        self.baud_combo = ttk.Combobox(toolbar, textvariable=self.baud_var, values=baud_rates, width=10, state="readonly")
        self.baud_combo.pack(side=tk.LEFT, padx=2)

        # 连接/断开按钮
        self.connect_btn = ttk.Button(toolbar, text="连接", width=8, command=self.toggle_connect)
        self.connect_btn.pack(side=tk.LEFT, padx=5)

        # 状态指示灯
        self.status_led = tk.Label(toolbar, text="●", fg="gray", font=("Arial", 14))
        self.status_led.pack(side=tk.LEFT, padx=5)
        self.status_label = ttk.Label(toolbar, text="未连接")
        self.status_label.pack(side=tk.LEFT)

        # 分隔线
        ttk.Separator(toolbar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=10)

        # 清除按钮
        ttk.Button(toolbar, text="清除", width=6, command=self.clear_output).pack(side=tk.LEFT, padx=2)

        # 选项区域
        options_frame = ttk.LabelFrame(self.root, text="显示选项", padding="5")
        options_frame.pack(fill=tk.X, padx=5, pady=2)

        ttk.Checkbutton(options_frame, text="显示时间戳", variable=self.timestamp_var).pack(side=tk.LEFT, padx=5)
        ttk.Checkbutton(options_frame, text="HEX显示", variable=self.hex_var).pack(side=tk.LEFT, padx=5)
        ttk.Checkbutton(options_frame, text="自动滚动", variable=self.auto_scroll_var).pack(side=tk.LEFT, padx=5)

        # 接收区域
        receive_frame = ttk.LabelFrame(self.root, text="接收数据", padding="5")
        receive_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=2)

        self.output_text = scrolledtext.ScrolledText(
            receive_frame,
            wrap=tk.WORD,
            font=("Consolas", 10),
            bg="#1e1e1e",
            fg="#d4d4d4",
            insertbackground="#d4d4d4",
            state=tk.DISABLED
        )
        self.output_text.pack(fill=tk.BOTH, expand=True)

        # 发送区域
        send_frame = ttk.LabelFrame(self.root, text="发送数据", padding="5")
        send_frame.pack(fill=tk.X, padx=5, pady=2)

        self.send_entry = ttk.Entry(send_frame)
        self.send_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 5))
        self.send_entry.bind("<Return>", lambda e: self.send_data())

        ttk.Checkbutton(send_frame, text="HEX发送", variable=self.send_hex_var).pack(side=tk.LEFT, padx=5)

        ttk.Button(send_frame, text="发送", width=8, command=self.send_data).pack(side=tk.LEFT, padx=2)

        # 底部状态栏
        self.statusbar = ttk.Frame(self.root, relief=tk.SUNKEN)
        self.statusbar.pack(fill=tk.X, side=tk.BOTTOM)

        self.rx_count = 0
        self.tx_count = 0
        self.rx_label = ttk.Label(self.statusbar, text="  接收: 0 字节")
        self.rx_label.pack(side=tk.LEFT, padx=5)

        self.tx_label = ttk.Label(self.statusbar, text="发送: 0 字节")
        self.tx_label.pack(side=tk.LEFT, padx=5)

        # 绑定窗口关闭事件
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def refresh_ports(self):
        """刷新串口列表"""
        ports = serial.tools.list_ports.comports()
        port_list = [port.device for port in ports]
        self.port_combo['values'] = port_list

        if port_list:
            # 自动选择最后一个端口（通常是最新插入的）
            if not self.port_var.get() or self.port_var.get() not in port_list:
                self.port_var.set(port_list[-1])
            self.append_output(f"[系统] 发现 {len(port_list)} 个串口: {', '.join(port_list)}\n", "system")
        else:
            self.port_var.set("")
            self.append_output("[系统] 未发现可用串口\n", "system")

    def toggle_connect(self):
        """连接/断开串口"""
        if self.is_connected:
            self.disconnect()
        else:
            self.connect()

    def connect(self):
        """连接串口"""
        port = self.port_var.get()
        if not port:
            messagebox.showwarning("警告", "请选择串口！")
            return

        baud = int(self.baud_var.get())

        try:
            self.ser = serial.Serial(
                port=port,
                baudrate=baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1
            )

            self.is_connected = True
            self.stop_reading = False

            # 启动读取线程
            self.read_thread = threading.Thread(target=self.read_loop, daemon=True)
            self.read_thread.start()

            # 更新UI
            self.connect_btn.config(text="断开")
            self.status_led.config(fg="#00ff00")
            self.status_label.config(text=f"已连接 {port} @ {baud}")
            self.port_combo.config(state=tk.DISABLED)
            self.baud_combo.config(state=tk.DISABLED)

            self.append_output(f"\n[系统] 已连接 {port} @ {baud} bps\n", "success")

        except Exception as e:
            messagebox.showerror("错误", f"连接失败: {str(e)}")
            self.append_output(f"[错误] 连接失败: {str(e)}\n", "error")

    def disconnect(self):
        """断开串口"""
        self.stop_reading = True

        if self.ser and self.ser.is_open:
            self.ser.close()

        self.is_connected = False

        # 更新UI
        self.connect_btn.config(text="连接")
        self.status_led.config(fg="gray")
        self.status_label.config(text="未连接")
        self.port_combo.config(state="readonly")
        self.baud_combo.config(state="readonly")

        self.append_output("\n[系统] 已断开连接\n", "system")

    def read_loop(self):
        """读取数据的线程"""
        buffer = bytearray()

        while not self.stop_reading and self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting > 0:
                    data = self.ser.read(self.ser.in_waiting)
                    if data:
                        self.rx_count += len(data)
                        self.root.after(0, self.update_rx_count)

                        # 处理数据
                        if self.hex_var.get():
                            # HEX显示
                            hex_str = ' '.join(f'{b:02X}' for b in data)
                            self.root.after(0, lambda: self.append_output(hex_str + ' ', "data"))
                        else:
                            # 文本显示
                            buffer.extend(data)
                            # 尝试按行处理
                            while b'\n' in buffer:
                                line, buffer = buffer.split(b'\n', 1)
                                try:
                                    text = line.decode('utf-8', errors='replace')
                                    self.root.after(0, lambda t=text: self.append_output(t + '\n', "data"))
                                except:
                                    pass

                            # 如果缓冲区太大就强制输出
                            if len(buffer) > 1024:
                                try:
                                    text = buffer.decode('utf-8', errors='replace')
                                    self.root.after(0, lambda t=text: self.append_output(t, "data"))
                                except:
                                    pass
                                buffer.clear()

                else:
                    time.sleep(0.01)

            except Exception as e:
                if not self.stop_reading:
                    self.root.after(0, lambda: self.append_output(f"\n[错误] 读取异常: {str(e)}\n", "error"))
                break

    def send_data(self):
        """发送数据"""
        if not self.is_connected or not self.ser:
            messagebox.showwarning("警告", "请先连接串口！")
            return

        data_str = self.send_entry.get()
        if not data_str:
            return

        try:
            if self.send_hex_var.get():
                # HEX发送
                hex_str = data_str.replace(' ', '').replace('\n', '')
                data = bytes.fromhex(hex_str)
            else:
                # 文本发送
                data = data_str.encode('utf-8') + b'\r\n'

            self.ser.write(data)
            self.tx_count += len(data)
            self.update_tx_count()

            # 显示发送的数据
            if self.hex_var.get():
                hex_display = ' '.join(f'{b:02X}' for b in data)
                self.append_output(f"\n>>> {hex_display}\n", "send")
            else:
                self.append_output(f"\n>>> {data_str}\n", "send")

            # 清空输入框
            self.send_entry.delete(0, tk.END)

        except Exception as e:
            messagebox.showerror("错误", f"发送失败: {str(e)}")

    def append_output(self, text, tag="data"):
        """追加输出文本"""
        self.output_text.config(state=tk.NORMAL)

        # 添加时间戳
        if self.timestamp_var.get() and text.strip():
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            self.output_text.insert(tk.END, f"[{timestamp}] ", "timestamp")

        self.output_text.insert(tk.END, text, tag)

        # 自动滚动
        if self.auto_scroll_var.get():
            self.output_text.see(tk.END)

        self.output_text.config(state=tk.DISABLED)

    def clear_output(self):
        """清空输出"""
        self.output_text.config(state=tk.NORMAL)
        self.output_text.delete(1.0, tk.END)
        self.output_text.config(state=tk.DISABLED)
        self.rx_count = 0
        self.tx_count = 0
        self.update_rx_count()
        self.update_tx_count()

    def update_rx_count(self):
        self.rx_label.config(text=f"  接收: {self.rx_count} 字节")

    def update_tx_count(self):
        self.tx_label.config(text=f"发送: {self.tx_count} 字节")

    def on_close(self):
        """窗口关闭时清理资源"""
        if self.is_connected:
            self.disconnect()
        self.root.destroy()


def main():
    # 设置样式
    root = tk.Tk()

    # 配置文本标签颜色
    style = ttk.Style()
    try:
        style.theme_use('clam')
    except:
        pass

    app = SerialMonitor(root)

    # 设置文本标签样式
    app.output_text.tag_config("timestamp", foreground="#888888")
    app.output_text.tag_config("data", foreground="#d4d4d4")
    app.output_text.tag_config("send", foreground="#4ec9b0")
    app.output_text.tag_config("system", foreground="#569cd6")
    app.output_text.tag_config("success", foreground="#6a9955")
    app.output_text.tag_config("error", foreground="#f44747")

    root.mainloop()


if __name__ == "__main__":
    main()
