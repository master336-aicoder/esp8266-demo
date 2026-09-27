"""
ESP-01S 启动信息检测
读取启动时的串口输出，判断芯片是否正常工作
"""
import serial
import time

PORT = "COM4"
BAUD = 74880  # ESP8266 启动默认波特率

print(f"=== 检测 ESP-01S 启动信息 ({PORT} @ {BAUD}) ===")
print()

try:
    s = serial.Serial(PORT, BAUD, timeout=2)
    print("串口已打开")
    print()

    # 读取 5 秒
    print("等待启动信息（5秒）...")
    data = b""
    start = time.time()
    while time.time() - start < 5:
        chunk = s.read(128)
        if chunk:
            data += chunk

    s.close()

    if data:
        print(f"收到 {len(data)} 字节")
        print()
        text = data.decode('utf-8', errors='replace')
        lines = text.split('\n')
        for line in lines[:20]:
            if line.strip():
                print(f"  {line.strip()[:120]}")
        print()

        # 判断是否是 boot 信息
        if "ets" in text or "boot" in text or "SPI" in text:
            print("✅ ESP-01S 正常启动，有 boot 信息")
        else:
            print("⚠️  有数据但不是标准 boot 信息")
    else:
        print("❌ 5秒内没有收到任何数据")
        print()
        print("可能原因：")
        print("  1. CH_PD/EN 没接 3.3V（芯片不启动）")
        print("  2. TX/RX 接反了")
        print("  3. 供电不足")
        print("  4. 芯片损坏")

    print()

    # 再试试 115200
    print("=== 尝试 115200 波特率 ===")
    s2 = serial.Serial(PORT, 115200, timeout=2)
    data2 = b""
    start = time.time()
    while time.time() - start < 3:
        chunk = s2.read(128)
        if chunk:
            data2 += chunk
    s2.close()

    if data2:
        print(f"收到 {len(data2)} 字节")
        text2 = data2.decode('utf-8', errors='replace')
        for line in text2.split('\n')[:10]:
            if line.strip():
                print(f"  {line.strip()[:100]}")
    else:
        print("无数据")

except Exception as e:
    print(f"错误: {e}")
