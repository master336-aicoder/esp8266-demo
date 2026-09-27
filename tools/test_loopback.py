"""
测试 D1 开发板串口是否正常（自发自收）
需要先把 D1 上的 TX 和 RX 短接
"""
import serial
import time

PORT = "COM4"
BAUD = 115200

print(f"=== 串口自发自收测试 ({PORT} @ {BAUD}) ===")
print()
print("请确保 D1 的 TX 和 RX 短接了")
print("（这样发出去的数据会自己收回来）")
print()

try:
    s = serial.Serial(PORT, BAUD, timeout=1)
    print("串口打开成功")
    print()

    # 清空缓冲区
    s.reset_input_buffer()
    s.reset_output_buffer()

    # 发送测试数据
    test_data = b"HELLO_ESP8266_TEST_1234567890\r\n"
    print(f"发送: {test_data.decode()}")
    s.write(test_data)
    s.flush()
    time.sleep(0.1)

    # 读取返回
    received = s.read(len(test_data) + 10)
    if received:
        print(f"收到: {received.decode(errors='replace')}")
        if received == test_data:
            print()
            print("✅ 完全匹配！串口收发正常！")
        else:
            print()
            print("⚠️  收到数据但不匹配")
    else:
        print("❌ 没有收到数据")
        print()
        print("可能原因：")
        print("  1. TX 和 RX 没有短接")
        print("  2. D1 的 RST 没接 GND（D1 自己的 ESP8266 还在工作）")
        print("  3. 串口硬件有问题")

    s.close()

except Exception as e:
    print(f"错误: {e}")
