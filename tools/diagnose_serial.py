"""
ESP32-CAM 串口问题诊断工具
逐步排查串口无法打开的原因
"""
import serial.tools.list_ports
import serial
import time
import sys

def test_port_open(port):
    """测试串口能否打开"""
    try:
        # 尝试多种配置方式
        for dtr in [True, False]:
            for rts in [True, False]:
                try:
                    s = serial.Serial(
                        port=port,
                        baudrate=115200,
                        timeout=1,
                        dsrdtr=dtr,
                        rtscts=rts
                    )
                    s.close()
                    return True, f"打开成功 (DTR={dtr}, RTS={rts})"
                except:
                    continue
        return False, "所有配置方式都失败"
    except Exception as e:
        return False, str(e)


def main():
    print("=" * 60)
    print("  ESP32-CAM 串口诊断工具")
    print("=" * 60)
    print()

    ports = list(serial.tools.list_ports.comports())

    if not ports:
        print("❌ 未检测到任何串口设备")
        print()
        print("请检查：")
        print("  1. USB-TTL 下载器是否已连接")
        print("  2. USB 线是否是数据线")
        print("  3. 驱动是否已安装")
        return

    print(f"检测到 {len(ports)} 个串口：")
    print()

    for p in ports:
        print(f"📌 {p.device}: {p.description}")
        print(f"   厂商: {p.manufacturer}")

        # 测试打开
        ok, msg = test_port_open(p.device)
        if ok:
            print(f"   状态: ✅ 正常 - {msg}")

            # 尝试读取数据
            try:
                s = serial.Serial(p.device, 115200, timeout=2)
                time.sleep(1)
                data = s.read_all()
                s.close()
                if data:
                    text = data[:200].decode('utf-8', errors='replace')
                    print(f"   数据: {text[:100]}")
                else:
                    print(f"   数据: (无数据输出)")
            except:
                pass
        else:
            print(f"   状态: ❌ 异常 - {msg}")
        print()

    print("=" * 60)
    print("  诊断建议")
    print("=" * 60)
    print()
    print("🔍 排查步骤：")
    print()
    print("第1步：断开 ESP32-CAM，只保留 USB-TTL 下载器")
    print("  → 如果串口能打开：ESP32-CAM 接线有问题")
    print("  → 如果串口仍打不开：USB-TTL 下载器或驱动有问题")
    print()
    print("第2步：如果是 ESP32-CAM 问题，检查：")
    print("  ✅ 供电是否是 5V（不能是 3.3V）")
    print("  ✅ TX/RX 是否交叉接对了")
    print("  ✅ GND 是否共地")
    print("  ✅ 有没有短路（TX 碰 GND 等）")
    print("  ✅ ESP32-CAM 是否有虚焊")
    print()
    print("第3步：如果是下载器问题，检查：")
    print("  ✅ 换一个 USB 口")
    print("  ✅ 换一根 USB 线")
    print("  ✅ 重新安装驱动")
    print()


if __name__ == '__main__':
    main()
