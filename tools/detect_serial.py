"""
自动检测 ESP32-CAN 设备和串口状态
"""
import serial.tools.list_ports
import serial
import time

def detect_ports():
    ports = list(serial.tools.list_ports.comports())
    print(f"检测到 {len(ports)} 个串口设备：")
    print()

    for i, p in enumerate(ports):
        print(f"[{i+1}] {p.device}")
        print(f"    描述: {p.description}")
        print(f"    厂商: {p.manufacturer}")
        print(f"    硬件ID: {p.hwid}")
        
        # 尝试打开
        try:
            s = serial.Serial(p.device, 115200, timeout=2)
            print(f"    状态: ✅ 打开成功")
            time.sleep(0.5)
            data = s.read_all()
            if data:
                text = data[:150].decode('utf-8', errors='replace')
                print(f"    输出: {text}")
            else:
                print(f"    输出: (无数据)")
            s.close()
        except Exception as e:
            print(f"    状态: ❌ 打开失败")
            print(f"    错误: {e}")
        print()

    if not ports:
        print("❌ 未检测到任何串口设备")
        print()
        print("可能的原因：")
        print("  1. 设备未连接")
        print("  2. USB 线是充电线不是数据线")
        print("  3. 驱动未安装")
        print("  4. USB 口供电不足")

if __name__ == '__main__':
    detect_ports()
