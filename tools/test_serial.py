import serial
import serial.tools.list_ports
import time

print("=== 可用串口 ===")
ports = list(serial.tools.list_ports.comports())
for p in ports:
    vid = p.vid if p.vid else "N/A"
    pid = p.pid if p.pid else "N/A"
    print(f"  {p.device}: {p.description} (VID:PID={vid}:{pid})")

print()
print("=== 尝试打开 COM4 ===")
try:
    s = serial.Serial("COM4", 115200, timeout=1)
    print("COM4 打开成功!")
    s.close()
except Exception as e:
    print(f"COM4 失败: {e}")
    print()
    print("=== 尝试低波特率 + 无流控 ===")
    for baud in [9600, 19200, 38400, 57600, 115200]:
        try:
            s = serial.Serial()
            s.port = "COM4"
            s.baudrate = baud
            s.bytesize = 8
            s.parity = serial.PARITY_NONE
            s.stopbits = serial.STOPBITS_ONE
            s.xonxoff = False
            s.rtscts = False
            s.dsrdtr = False
            s.timeout = 1
            s.open()
            print(f"  {baud} -> OK!")
            s.close()
            break
        except Exception as e2:
            print(f"  {baud} -> FAIL")
