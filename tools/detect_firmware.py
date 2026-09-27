"""
检测 ESP-01S 当前运行的固件
试试不同波特率，看是不是 AT 固件
"""
import serial
import time

PORT = "COM6"

test_bauds = [115200, 9600, 57600, 38400, 19200, 74880]

print("=== 检测 ESP-01S 固件 ===")
print()

for baud in test_bauds:
    try:
        s = serial.Serial(PORT, baud, timeout=1)
        s.reset_input_buffer()
        
        # 发送 AT 命令
        s.write(b'AT\r\n')
        s.flush()
        time.sleep(0.5)
        
        resp = s.read(256)
        s.close()
        
        if resp:
            text = resp.decode('utf-8', errors='replace')
            # 判断是不是正常的 ASCII
            ascii_count = sum(1 for c in text if 32 <= ord(c) < 127 or c in '\r\n\t')
            ratio = ascii_count / max(len(text), 1)
            
            if ratio > 0.7:
                print(f"✅ {baud} 波特率 - 可读数据 (ASCII {ratio*100:.0f}%):")
                for line in text.split('\n')[:8]:
                    if line.strip():
                        print(f"   {line.strip()[:100]}")
                print()
                print("👉 芯片在正常运行，没有进入下载模式！")
                print("   请确保 GPIO0 接了 GND，然后重新上电")
                break
            else:
                print(f"⚠️  {baud} 波特率 - 乱码 (ASCII {ratio*100:.0f}%)")
        else:
            print(f"❌ {baud} 波特率 - 无响应")
            
    except Exception as e:
        print(f"❌ {baud} 波特率 - 错误: {e}")

print()
print("=== 结论 ===")
print()
print("如果能读到 'OK' 或 'AT' 字样，说明芯片在运行 AT 固件")
print("需要把 GPIO0 接 GND 后重新上电，才能进入下载模式")
