"""
读取 ESP8266 串口输出 10 秒
"""
import serial
import time

try:
    s = serial.Serial('COM4', 115200, timeout=1)
    print("=== 串口已连接，等待输出... ===")
    print()
    
    start = time.time()
    while time.time() - start < 10:
        try:
            line = s.readline().decode('utf-8', errors='replace').strip()
            if line:
                print(line)
        except Exception as e:
            pass
    
    s.close()
    print()
    print("=== 读取结束 ===")
    
except Exception as e:
    print(f"错误: {e}")
