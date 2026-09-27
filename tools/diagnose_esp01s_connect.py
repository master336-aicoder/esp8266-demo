"""
诊断 ESP-01S 连接问题
1. 读取 boot 信息（74880 波特率）
2. 手动复位后同步
"""
import serial
import time

PORT = "COM6"

print("=== 诊断 ESP-01S 连接问题 ===")
print()

# 第一步：74880 波特率读 boot 信息
print("1. 读取 boot 信息 (74880 波特率)...")
try:
    s = serial.Serial(PORT, 74880, timeout=0.5)
    # 清空
    s.reset_input_buffer()
    
    # 手动复位
    try:
        s.dtr = False
        s.rts = True
        time.sleep(0.2)
        s.dtr = True
        s.rts = False
    except:
        print("   (无法控制 DTR/RTS，跳过自动复位)")
    
    # 读取 2 秒
    data = b""
    start = time.time()
    while time.time() - start < 2:
        chunk = s.read(64)
        if chunk:
            data += chunk
    s.close()
    
    if data:
        print(f"   收到 {len(data)} 字节:")
        text = data.decode('utf-8', errors='replace')
        for line in text.split('\n')[:10]:
            if line.strip():
                print(f"     {line.strip()[:120]}")
    else:
        print("   无数据")
        
except Exception as e:
    print(f"   错误: {e}")

print()

# 第二步：9600 波特率手动 sync
print("2. 手动发送 sync 命令 (9600 波特率)...")
try:
    s = serial.Serial(PORT, 9600, timeout=0.5)
    s.reset_input_buffer()
    
    # 发送 sync 命令
    sync_pkt = b'\x00' + b'\x07\x07\x12\x20' + b'\x55' * 32
    s.write(sync_pkt)
    s.flush()
    time.sleep(0.1)
    
    resp = s.read(10)
    s.close()
    
    if resp:
        print(f"   收到响应: {resp.hex()} ({len(resp)} 字节)")
        if resp[0] == 0x01:
            print("   ✅ 正确的 sync 响应！")
        else:
            print(f"   ⚠️  响应首字节: 0x{resp[0]:02x} (期望 0x01)")
    else:
        print("   无响应")
        
except Exception as e:
    print(f"   错误: {e}")

print()

# 第三步：检查是否有持续的乱码（说明波特率不对）
print("3. 检查是否有持续数据输出...")
try:
    s = serial.Serial(PORT, 9600, timeout=0.5)
    data = b""
    start = time.time()
    while time.time() - start < 2:
        chunk = s.read(64)
        if chunk:
            data += chunk
    s.close()
    
    if data:
        print(f"   有持续数据: {len(data)} 字节")
        # 检查是否是乱码（很多非 ASCII 字符）
        non_ascii = sum(1 for b in data if b < 32 or b > 126)
        print(f"   非 ASCII 字符比例: {non_ascii}/{len(data)} ({non_ascii*100//len(data)}%)")
        if non_ascii * 100 // len(data) > 50:
            print("   ⚠️  可能是波特率不对（乱码）")
    else:
        print("   无数据")
        
except Exception as e:
    print(f"   错误: {e}")

print()
print("=== 结论与建议 ===")
print()
print("如果 boot 信息也读不到：")
print("  → ESP-01S 芯片没启动")
print("  → 检查 CH_PD/EN 是否接了 3.3V")
print("  → 检查供电是否正常")
print()
print("如果 boot 信息能读到但 sync 失败：")
print("  → TX/RX 可能接反了")
print("  → 或 GPIO0 没接 GND（没进入下载模式）")
