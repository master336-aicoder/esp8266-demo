"""
单线程捕获 boot 信息
"""
import serial
import time

PORT = "COM4"

print("=== 捕获 ESP8266 boot 信息 ===")
print()

for baud in [74880, 115200, 9600]:
    print(f"尝试 {baud} 波特率...")
    try:
        s = serial.Serial(PORT, baud, timeout=0.5)
        # 清空缓冲区
        s.reset_input_buffer()

        # 手动复位
        try:
            s.dtr = False
            s.rts = True
            time.sleep(0.2)
            s.dtr = True
            s.rts = False
        except:
            pass

        # 读取 2 秒
        data = b""
        start = time.time()
        while time.time() - start < 2:
            chunk = s.read(64)
            if chunk:
                data += chunk

        s.close()

        if data:
            print(f"  收到 {len(data)} 字节:")
            text = data.decode('utf-8', errors='replace')
            for line in text.split('\n')[:15]:
                if line.strip():
                    print(f"    {line.strip()[:120]}")
            print()
            break
        else:
            print(f"  无数据")
            print()

    except Exception as e:
        print(f"  错误: {e}")
        print()

print("=== 结论 ===")
print()
print("如果所有波特率都没数据，请检查：")
print("  1. D1 的 RST 是否接了 GND？（必须接，否则 D1 自己的 ESP8266 在干扰）")
print("  2. ESP-01S 的 CH_PD/EN 是否接了 3.3V？（必须接，否则芯片不启动）")
print("  3. ESP-01S 的 VCC 是否接了 3.3V？")
print("  4. ESP-01S 的 GND 是否接了 GND？")
print("  5. TX/RX 是否交叉连接？（D1 TX → ESP-01S RX, D1 RX → ESP-01S TX）")
