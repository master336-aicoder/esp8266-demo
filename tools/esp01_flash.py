"""
ESP-01S 烧录辅助脚本
手动控制 DTR/RTS 进入下载模式
"""
import serial
import time
import struct

PORT = "COM4"
BAUD = 9600

def try_connect():
    print(f"=== 尝试连接 ESP-01S ({PORT} @ {BAUD}) ===")
    print()

    try:
        s = serial.Serial()
        s.port = PORT
        s.baudrate = BAUD
        s.bytesize = serial.EIGHTBITS
        s.parity = serial.PARITY_NONE
        s.stopbits = serial.STOPBITS_ONE
        s.xonxoff = False
        s.rtscts = False
        s.dsrdtr = False
        s.timeout = 2

        print("1. 打开串口...", end=" ")
        s.open()
        print("OK")

        print("2. 设置 DTR=True, RTS=False...", end=" ")
        s.dtr = True
        s.rts = False
        time.sleep(0.1)
        print("OK")

        print("3. 发送复位命令 (GPIO0=LOW)...", end=" ")
        s.dtr = False
        s.rts = True
        time.sleep(0.1)
        s.dtr = True
        s.rts = False
        time.sleep(0.5)
        print("OK")

        print("4. 读取启动信息...", end=" ")
        data = b""
        start = time.time()
        while time.time() - start < 2:
            chunk = s.read(64)
            if chunk:
                data += chunk

        if data:
            text = data.decode('utf-8', errors='replace')
            print(f"收到 {len(data)} 字节")
            print(f"  内容: {text[:200]}")
        else:
            print("无数据")

        print()
        print("5. 发送同步命令 0x07...", end=" ")
        # ESP boot ROM sync command
        sync_cmd = b'\x07\x07\x12\x20' + b'\x55\x55\x55\x55\x55\x55\x55\x55' + b'\x55\x55\x55\x55\x55\x55\x55\x55' + b'\x55\x55\x55\x55\x55\x55\x55\x55' + b'\x55\x55\x55\x55\x55\x55\x55\x55' + b'\x55\x55\x55\x55\x55\x55\x55\x55' + b'\x00\x00\x08\x00\x07\x07\x12\x20' + b'\x00\x00\x00\x00\x00\x00\x00\x00'
        s.write(sync_cmd)
        time.sleep(1)

        resp = b""
        start = time.time()
        while time.time() - start < 2:
            chunk = s.read(64)
            if chunk:
                resp += chunk

        if resp:
            print(f"收到响应: {len(resp)} 字节")
            print(f"  hex: {resp[:32].hex()}")
        else:
            print("无响应")

        s.close()
        print()
        print("=== 测试完成 ===")

    except Exception as e:
        print(f"FAIL: {e}")

    print()

    # 尝试不同波特率
    print("=== 尝试不同波特率 ===")
    for baud in [9600, 19200, 38400, 57600, 74880, 115200]:
        try:
            s = serial.Serial()
            s.port = PORT
            s.baudrate = baud
            s.timeout = 1
            s.open()
            print(f"  {baud:6d} -> OK")
            s.close()
        except Exception as e:
            print(f"  {baud:6d} -> FAIL")

if __name__ == "__main__":
    try_connect()
