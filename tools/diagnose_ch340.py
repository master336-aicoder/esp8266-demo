"""
诊断 CH340 串口问题
测试打开串口、设置 DTR/RTS 是否正常
"""
import serial
import time

PORT = "COM4"
BAUD = 115200

print(f"=== 诊断 {PORT} @ {BAUD} ===")
print()

# 测试1: 基本打开
print("1. 基本打开串口...", end=" ")
try:
    s = serial.Serial()
    s.port = PORT
    s.baudrate = BAUD
    s.timeout = 1
    s.open()
    print("OK")
    s.close()
except Exception as e:
    print(f"FAIL: {e}")
    print()
    print("尝试不同打开方式...")
    # 尝试不设置任何控制信号
    for dsrdtr in [False, True]:
        for rtscts in [False, True]:
            try:
                s = serial.Serial()
                s.port = PORT
                s.baudrate = BAUD
                s.timeout = 1
                s.dsrdtr = dsrdtr
                s.rtscts = rtscts
                s.xonxoff = False
                s.open()
                print(f"  dsrdtr={dsrdtr}, rtscts={rtscts} -> OK")
                s.close()
            except Exception as e2:
                print(f"  dsrdtr={dsrdtr}, rtscts={rtscts} -> FAIL")

print()

# 测试2: 打开后手动设置 DTR/RTS
print("2. 打开后设置 DTR/RTS...")
try:
    s = serial.Serial()
    s.port = PORT
    s.baudrate = BAUD
    s.timeout = 1
    s.open()
    print("  打开成功")

    print("  设置 dtr=True...", end=" ")
    try:
        s.dtr = True
        time.sleep(0.1)
        print("OK")
    except Exception as e:
        print(f"FAIL: {e}")

    print("  设置 rts=True...", end=" ")
    try:
        s.rts = True
        time.sleep(0.1)
        print("OK")
    except Exception as e:
        print(f"FAIL: {e}")

    print("  读取 dtr/rts 状态...", end=" ")
    try:
        dtr_state = s.dtr
        rts_state = s.rts
        print(f"dtr={dtr_state}, rts={rts_state}")
    except Exception as e:
        print(f"FAIL: {e}")

    s.close()
    print("  关闭成功")

except Exception as e:
    print(f"  打开失败: {e}")

print()

# 测试3: 检查是否有数据可读（说明设备在运行）
print("3. 读取串口数据（3秒）...")
try:
    s = serial.Serial(PORT, BAUD, timeout=1)
    data = b""
    start = time.time()
    while time.time() - start < 3:
        chunk = s.read(64)
        if chunk:
            data += chunk
    s.close()

    if data:
        print(f"  收到 {len(data)} 字节数据")
        text = data.decode('utf-8', errors='replace')
        lines = text.split('\n')
        print(f"  前5行:")
        for line in lines[:5]:
            if line.strip():
                print(f"    {line.strip()[:100]}")
    else:
        print("  无数据")

except Exception as e:
    print(f"  失败: {e}")

print()
print("=== 诊断完成 ===")
