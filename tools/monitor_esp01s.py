"""
ESP-01S 稳定性监控
监控1小时，检测崩溃/重启/异常
"""

import serial
import time
import sys
import os
from datetime import datetime

PORT = 'COM6'
BAUD = 115200
DURATION = 3600  # 1小时
LOG_FILE = r'd:\traecode\esp8266\esp01s_stability.log'
STATUS_FILE = r'd:\traecode\esp8266\esp01s_status.txt'
ERROR_FILE = r'd:\traecode\esp8266\esp01s_errors.txt'

ERROR_KEYWORDS = [
    'Exception (',
    'Fatal exception',
    'rst cause',
    'wdt reset',
    'Soft WDT',
    'Panic',
    'Guru Meditation',
    'crash',
    'restarting',
    'reboot',
    'CUT HERE FOR EXCEPTION',
]

print('=' * 60)
print('  ESP-01S 稳定性监控 (60分钟)')
print('=' * 60)
print(f'端口: {PORT} @ {BAUD}')
print(f'时长: {DURATION//60} 分钟')
print()

# 打开串口
try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print('[OK] 串口连接成功')
    print()
except Exception as e:
    print(f'[ERR] 串口打开失败: {e}')
    sys.exit(1)

start_time = time.time()
line_count = 0
error_count = 0
errors = []
last_report = 0

with open(LOG_FILE, 'w', encoding='utf-8') as logf:
    logf.write(f'=== 稳定性监控开始: {datetime.now()} ===\n')
    logf.flush()

    while time.time() - start_time < DURATION:
        try:
            line = ser.readline().decode('utf-8', errors='replace').strip()
            if line:
                line_count += 1
                elapsed = int(time.time() - start_time)
                logf.write(f'[{elapsed}s] {line}\n')

                # 检测异常
                for kw in ERROR_KEYWORDS:
                    if kw.lower() in line.lower():
                        error_count += 1
                        errors.append(line)
                        with open(ERROR_FILE, 'a', encoding='utf-8') as ef:
                            ef.write(f'[{datetime.now()}] {line}\n')
                        print(f'[!!] 检测到异常: {line[:100]}')
                        break

                # 每5分钟报告一次
                elapsed_min = (time.time() - start_time) / 60
                if int(elapsed_min) > last_report and int(elapsed_min) % 5 == 0:
                    last_report = int(elapsed_min)
                    print(f'[进度] 已运行 {elapsed_min:.0f} 分钟, {line_count} 行, {error_count} 个异常')
                    logf.flush()

        except Exception as e:
            print(f'[ERR] 读取错误: {e}')
            time.sleep(1)

    elapsed = time.time() - start_time
    logf.write(f'\n=== 监控结束: {datetime.now()} ===\n')
    logf.write(f'运行时长: {elapsed/60:.1f} 分钟\n')
    logf.write(f'日志行数: {line_count}\n')
    logf.write(f'异常数量: {error_count}\n')

ser.close()

print()
print('=' * 60)
print('  监控完成')
print('=' * 60)
print(f'运行时长: {elapsed/60:.1f} 分钟')
print(f'日志行数: {line_count}')
print(f'异常数量: {error_count}')

# 写入状态文件
with open(STATUS_FILE, 'w', encoding='utf-8') as sf:
    sf.write(f'completed={error_count == 0}\n')
    sf.write(f'duration={elapsed:.0f}\n')
    sf.write(f'lines={line_count}\n')
    sf.write(f'errors={error_count}\n')

if error_count == 0:
    print('状态: ✅ 稳定（无异常）')
    sys.exit(0)
else:
    print('状态: ❌ 检测到异常')
    for e in errors[:10]:
        print(f'  - {e[:120]}')
    sys.exit(1)
