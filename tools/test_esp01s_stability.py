"""
测试 ESP-01S 连接稳定性
多次尝试连接，看成功率
"""
import serial
import time
import sys

PORT = "COM6"

def test_connection(baud, attempts=5):
    print(f"=== 测试 {baud} 波特率，{attempts} 次 ===")
    success = 0
    for i in range(attempts):
        try:
            s = serial.Serial(PORT, baud, timeout=2)
            # 尝试发送 sync
            s.reset_input_buffer()
            s.write(b'\x07\x07\x12\x20' + b'\x55' * 36)
            s.flush()
            time.sleep(0.1)
            resp = s.read(10)
            s.close()
            if len(resp) > 0:
                success += 1
                print(f"  尝试 {i+1}: OK (收到 {len(resp)} 字节)")
            else:
                print(f"  尝试 {i+1}: 无响应")
        except Exception as e:
            print(f"  尝试 {i+1}: 失败 ({str(e)[:50]})")
    print(f"  成功率: {success}/{attempts}")
    print()
    return success

print("ESP-01S 连接稳定性测试")
print(f"端口: {PORT}")
print()

for baud in [9600, 19200, 38400, 57600, 115200]:
    ok = test_connection(baud, 3)
    if ok >= 2:
        print(f"✅ {baud} 波特率可用")
        break
else:
    print("❌ 所有波特率都不稳定")
    print()
    print("可能原因：")
    print("  1. 杜邦线接触不良 → 重新插拔，压紧")
    print("  2. 线太长 → 用短一点的线")
    print("  3. 供电不稳 → 检查 3.3V 供电")
    print("  4. 共地不好 → 确保 GND 连接可靠")
    print("  5. CH341A 模式不对 → 确认是 UART 模式")
    print("  6. TX/RX 接反 → 交换 TX/RX 试试")
