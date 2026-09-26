"""
ESP32-CAN 设备自动检测与驱动安装工具
"""
import sys
import subprocess
import os

def check_python_packages():
    """检查必要的 Python 包"""
    print("=" * 60)
    print("  ESP32-CAN 设备检测工具")
    print("=" * 60)
    print()

    # 检查 pyserial
    try:
        import serial.tools.list_ports
        print("✅ pyserial 已安装")
        return True
    except ImportError:
        print("❌ pyserial 未安装，正在安装...")
        subprocess.run([sys.executable, "-m", "pip", "install", "pyserial"])
        print("安装完成")
        return True


def detect_serial_ports():
    """检测所有串口设备"""
    import serial.tools.list_ports
    import serial

    print()
    print("=" * 60)
    print("  串口设备检测")
    print("=" * 60)
    print()

    ports = list(serial.tools.list_ports.comports())

    if not ports:
        print("❌ 未检测到任何串口设备")
        print()
        print("可能的原因：")
        print("  1. 设备未连接到电脑")
        print("  2. USB 线是充电线，不是数据线")
        print("  3. 驱动未正确安装")
        print("  4. USB 接口供电不足")
        print("  5. 设备未上电")
        return []

    print(f"检测到 {len(ports)} 个串口设备：")
    print()

    working_ports = []

    for i, p in enumerate(ports, 1):
        vid_pid = ""
        if hasattr(p, 'vid') and p.vid:
            vid_pid = f"VID:PID={p.vid:04X}:{p.pid:04X}"

        print(f"[{i}] {p.device}")
        print(f"    描述: {p.description}")
        print(f"    厂商: {p.manufacturer}")
        if vid_pid:
            print(f"    {vid_pid}")

        # 尝试打开
        try:
            s = serial.Serial(p.device, 115200, timeout=1)
            print(f"    状态: ✅ 正常")
            working_ports.append(p.device)
            s.close()
        except Exception as e:
            print(f"    状态: ❌ 异常")
            print(f"    错误: {e}")

        print()

    return working_ports


def identify_chip(vid, pid):
    """根据 VID:PID 识别芯片"""
    chips = {
        (0x1A86, 0x7523): "CH340/CH341 (南京沁恒)",
        (0x1A86, 0x5523): "CH340 (Type-C)",
        (0x10C4, 0xEA60): "CP2102 (Silicon Labs)",
        (0x10C4, 0xEA70): "CP2104 (Silicon Labs)",
        (0x0403, 0x6001): "FT232RL (FTDI)",
        (0x0403, 0x6014): "FT232H (FTDI)",
        (0x067B, 0x2303): "PL2303 (Prolific)",
        (0x303A, 0x1001): "ESP32-S2/S3 USB",
        (0x303A, 0x8000): "ESP32-C3 USB",
    }

    return chips.get((vid, pid), "未知芯片")


def suggest_driver(vid, pid):
    """根据芯片类型建议驱动"""
    chip = identify_chip(vid, pid)

    print()
    print("=" * 60)
    print("  驱动建议")
    print("=" * 60)
    print()
    print(f"芯片类型: {chip}")
    print(f"VID:PID:  {vid:04X}:{pid:04X}")
    print()

    if vid == 0x1A86:  # CH340
        print("✅ CH340 驱动检测...")
        driver_paths = [
            r"C:\Windows\System32\DriverStore\FileRepository\ch341ser.inf_amd64_*\ch341ser.inf",
        ]
        import glob
        found = False
        for pattern in driver_paths:
            matches = glob.glob(pattern)
            if matches:
                print(f"   已安装: {os.path.basename(os.path.dirname(matches[0]))}")
                found = True

        if not found:
            print("❌ 未检测到 CH340 驱动")
            print()
            print("安装方法：")
            print("  1. 下载地址: http://www.wch-ic.com/downloads/CH341SER_EXE.html")
            print("  2. 下载后运行 CH341SER.EXE 安装")
            print("  3. 安装完成后重新插拔设备")
        else:
            print("   驱动已安装，如果设备无法识别：")
            print("   1. 尝试重新插拔设备")
            print("   2. 换一个 USB 接口")
            print("   3. 检查 USB 线是否是数据线")

    elif vid == 0x10C4:  # CP2102
        print("ℹ️  CP2102 芯片")
        print("   下载地址: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers")

    elif vid == 0x0403:  # FTDI
        print("ℹ️  FTDI 芯片")
        print("   下载地址: https://ftdichip.com/drivers/")

    else:
        print("❓ 未知芯片，请根据设备型号查找对应驱动")


def main():
    check_python_packages()
    working = detect_serial_ports()

    # 如果有正常工作的端口，显示信息
    if working:
        print()
        print("=" * 60)
        print("  ✅ 检测完成，可用串口：", ", ".join(working))
        print("=" * 60)
    else:
        # 尝试通过 VID 检测芯片并给出驱动建议
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
        if ports and hasattr(ports[0], 'vid') and ports[0].vid:
            suggest_driver(ports[0].vid, ports[0].pid)
        else:
            print()
            print("=" * 60)
            print("  ❌ 未找到可用串口设备")
            print("=" * 60)
            print()
            print("请检查：")
            print("  1. 设备是否已连接")
            print("  2. USB 线是否是数据线")
            print("  3. 驱动是否已安装")
            print("  4. 换一个 USB 接口试试")


if __name__ == '__main__':
    main()
