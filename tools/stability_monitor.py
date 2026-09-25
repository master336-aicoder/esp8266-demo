"""
ESP8266 串口稳定性监控
- 监控指定时长（默认1小时）
- 检测崩溃/重启/异常
- 结果写入状态文件
- 异常立即写入错误文件
"""

import serial
import time
import sys
import os
from datetime import datetime

SERIAL_PORT = 'COM4'
BAUD_RATE = 115200
DURATION_MINUTES = 60  # 监控时长（分钟）
LOG_DIR = r'd:\traecode\esp8266\tools'
LOG_FILE = os.path.join(LOG_DIR, 'monitor_log.txt')
STATUS_FILE = os.path.join(LOG_DIR, 'monitor_status.txt')
ERROR_FILE = os.path.join(LOG_DIR, 'monitor_errors.txt')

# 异常关键词（崩溃/重启标志）
ERROR_KEYWORDS = [
    'Exception (',
    'CUT HERE FOR EXCEPTION',
    'epc1=0x',
    'Soft WDT reset',
    'Hardware Watchdog',
    'rst cause:',
    'boot mode:(',
    'Fatal exception',
    'fatal exception',
    'StoreProhibited',
    'LoadProhibited',
    '>>>stack>>>',
    'wdt reset',
    'WDT reset',
]

def write_status(status, error_count, elapsed_min, line_count):
    """写入状态文件"""
    with open(STATUS_FILE, 'w', encoding='utf-8') as f:
        f.write(f"status={status}\n")
        f.write(f"errors={error_count}\n")
        f.write(f"elapsed_min={elapsed_min}\n")
        f.write(f"lines={line_count}\n")
        f.write(f"updated={datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")

def has_error(line):
    for kw in ERROR_KEYWORDS:
        if kw in line:
            return True
    return False

def main():
    error_count = 0
    line_count = 0
    start_time = time.time()
    duration_sec = DURATION_MINUTES * 60

    print("=" * 60)
    print(f"  ESP8266 稳定性监控 ({DURATION_MINUTES}分钟)")
    print("=" * 60)

    # 清空状态文件
    write_status("running", 0, 0, 0)

    # 清空错误文件
    with open(ERROR_FILE, 'w', encoding='utf-8') as f:
        f.write(f"监控开始: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")

    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"[OK] 串口 {SERIAL_PORT} 已连接")
    except Exception as e:
        print(f"[ERR] 串口打开失败: {e}")
        write_status("serial_error", 0, 0, 0)
        sys.exit(1)

    last_status_update = 0

    with open(LOG_FILE, 'a', encoding='utf-8') as logf:
        logf.write(f"\n{'='*60}\n")
        logf.write(f"监控开始: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        logf.write(f"{'='*60}\n")

        while True:
            elapsed = time.time() - start_time

            # 达到时长，正常结束
            if elapsed >= duration_sec:
                print(f"\n[完成] {DURATION_MINUTES}分钟监控结束，无异常！")
                write_status("completed", error_count, int(elapsed/60), line_count)
                break

            # 每30秒更新一次状态
            if time.time() - last_status_update > 30:
                write_status("running", error_count, int(elapsed/60), line_count)
                last_status_update = time.time()
                print(f"[运行中] {int(elapsed/60)}分/{DURATION_MINUTES}分, 行数:{line_count}, 异常:{error_count}")

            try:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if not line:
                    continue

                line_count += 1
                logf.write(f"[{datetime.now().strftime('%H:%M:%S')}] {line}\n")

                # 异常检测
                if has_error(line):
                    error_count += 1
                    err_msg = f"[异常 #{error_count}] {datetime.now().strftime('%H:%M:%S')} - {line}"
                    print(f"\n⚠️  {err_msg}")

                    # 读取后续堆栈信息（最多50行）
                    with open(ERROR_FILE, 'a', encoding='utf-8') as ef:
                        ef.write(err_msg + "\n")
                        stack_lines = 0
                        while stack_lines < 50:
                            try:
                                sl = ser.readline().decode('utf-8', errors='replace').strip()
                                if sl:
                                    ef.write(f"  {sl}\n")
                                    logf.write(f"[{datetime.now().strftime('%H:%M:%S')}] {sl}\n")
                                    stack_lines += 1
                                    line_count += 1
                                else:
                                    break
                            except:
                                break
                        ef.write("-" * 40 + "\n")

                    write_status("error", error_count, int(elapsed/60), line_count)

            except serial.SerialException as e:
                print(f"\n[警告] 串口异常: {e}")
                time.sleep(2)
                try:
                    ser.close()
                    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
                    print("[恢复] 串口已重连")
                except:
                    pass

    ser.close()
    print(f"\n最终结果: {line_count} 行, {error_count} 次异常")

if __name__ == '__main__':
    main()
