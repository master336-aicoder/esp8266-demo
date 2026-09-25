"""
ESP8266 串口日志监控脚本
持续读取串口，检测异常（崩溃、重启等），保存日志到文件
"""

import serial
import time
import sys
from datetime import datetime

SERIAL_PORT = 'COM4'
BAUD_RATE = 115200
LOG_FILE = r'd:\traecode\esp8266\tools\monitor_log.txt'

# 异常关键词
ERROR_KEYWORDS = [
    'Exception',
    'CUT HERE FOR EXCEPTION',
    'epc1=',
    'Soft WDT reset',
    'watchdog',
    'rst cause',
    'boot mode',
    'crash',
    'fatal',
    'Fatal',
]

def check_exception(line):
    """检查行中是否包含异常关键词"""
    for kw in ERROR_KEYWORDS:
        if kw in line:
            return True
    return False

def main():
    print("=" * 60)
    print("  ESP8266 串口日志监控")
    print("=" * 60)
    print(f"端口: {SERIAL_PORT} @ {BAUD_RATE}")
    print(f"日志文件: {LOG_FILE}")
    print("按 Ctrl+C 停止")
    print("=" * 60)

    error_count = 0
    line_count = 0
    start_time = time.time()

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"[监控] 已连接 {SERIAL_PORT}")
        print(f"[监控] 开始时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
        print()

        with open(LOG_FILE, 'a', encoding='utf-8') as logf:
            logf.write(f"\n{'='*60}\n")
            logf.write(f"监控开始: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            logf.write(f"{'='*60}\n")

            while True:
                try:
                    line = ser.readline().decode('utf-8', errors='replace').strip()
                    if line:
                        line_count += 1
                        timestamp = datetime.now().strftime('%H:%M:%S')
                        log_line = f"[{timestamp}] {line}"
                        logf.write(log_line + '\n')
                        logf.flush()

                        # 检测异常
                        if check_exception(line):
                            error_count += 1
                            print(f"\n⚠️  检测到异常 [{error_count}]: {line}")
                            print(f"   时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")

                        # 每1000行输出一次统计
                        if line_count % 1000 == 0:
                            elapsed = time.time() - start_time
                            print(f"[统计] 已读 {line_count} 行, 运行 {int(elapsed/60)} 分钟, 异常 {error_count} 次")

                except serial.SerialException as e:
                    print(f"\n❌ 串口错误: {e}")
                    time.sleep(2)
                    try:
                        ser.close()
                        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                        print("[监控] 串口已重连")
                    except:
                        print("[监控] 重连失败，2秒后重试...")

    except KeyboardInterrupt:
        print("\n\n" + "=" * 60)
        print("监控停止")
        elapsed = time.time() - start_time
        print(f"运行时间: {int(elapsed/60)} 分 {int(elapsed%60)} 秒")
        print(f"总行数: {line_count}")
        print(f"异常次数: {error_count}")
        print("=" * 60)
    except Exception as e:
        print(f"\n❌ 错误: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
