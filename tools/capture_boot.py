"""
在 esptool 连接过程中捕获 boot 信息
"""
import serial
import time
import threading

PORT = "COM4"

captured = b""
stop = False

def read_thread(baud):
    global captured, stop
    try:
        s = serial.Serial(PORT, baud, timeout=0.1)
        while not stop:
            try:
                data = s.read(64)
                if data:
                    captured += data
            except:
                pass
        s.close()
    except Exception as e:
        print(f"读取线程错误: {e}")

print("=== 捕获 ESP8266 boot 信息 ===")
print()

# 先用 74880 读，看看 boot 输出
for baud in [74880, 115200, 9600]:
    captured = b""
    stop = False
    print(f"尝试 {baud} 波特率...")

    t = threading.Thread(target=read_thread, args=(baud,))
    t.daemon = True
    t.start()

    time.sleep(0.5)

    # 手动触发复位（DTR/RTS）
    try:
        s = serial.Serial(PORT, baud, timeout=0.5)
        # 拉低 RST
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        # 释放 RST
        s.dtr = True
        s.rts = False
        time.sleep(1)
        s.close()
    except Exception as e:
        print(f"  复位失败: {e}")

    time.sleep(2)
    stop = True
    t.join(timeout=1)

    if captured:
        print(f"  收到 {len(captured)} 字节:")
        text = captured.decode('utf-8', errors='replace')
        for line in text.split('\n')[:10]:
            if line.strip():
                print(f"    {line.strip()[:100]}")
        print()
        break
    else:
        print(f"  无数据")
        print()

if not captured:
    print("❌ 所有波特率都没有数据")
    print()
    print("可能原因：")
    print("  1. ESP-01S 芯片没有供电（CH_PD 没接 3.3V）")
    print("  2. TX/RX 接反了")
    print("  3. D1 的 RST 没接 GND（D1 自己的 ESP8266 在干扰）")
    print("  4. 芯片损坏")
