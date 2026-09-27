# 自动检测 ESP32-CAN 设备和驱动

Write-Host "=== ESP32-CAN 设备自动检测 ===" -ForegroundColor Cyan
Write-Host ""

# 已知的 USB 串口芯片 VID
$vidTable = @{
    '1A86' = 'CH340/CH341 (南京沁恒)'
    '10C4' = 'CP2102/CP2104 (Silicon Labs)'
    '0403' = 'FTDI FT232/FT2232'
    '067B' = 'Prolific PL2303'
    '0483' = 'STM32 (STMicroelectronics)'
    '2A86' = 'CH340 (沁恒)'
    '303A' = 'Espressif (USB-JTAG)'
    '1A86' = 'CH340 (沁恒)'
}

Write-Host "--- 检测所有串口设备 ---" -ForegroundColor Yellow
$ports = Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue
if ($ports) {
    foreach ($p in $ports) {
        $statusIcon = if ($p.Status -eq 'OK') { '✅' } else { '⚠️ ' }
        Write-Host "  $statusIcon $($p.Name) - 状态: $($p.Status)"
        Write-Host "     DeviceID: $($p.DeviceID)"

        # 检查 VID
        if ($p.DeviceID -match 'VID_([0-9A-F]{4})') {
            $vid = $Matches[1].ToUpper()
            Write-Host "     VID: 0x$vid"
        }
        Write-Host ""
    }
} else {
    Write-Host "  未检测到串口设备" -ForegroundColor Red
}

Write-Host ""
Write-Host "--- 检测所有 USB 设备 ---" -ForegroundColor Yellow

$usbDevices = Get-PnpDevice -Class USB -ErrorAction SilentlyContinue
$foundSerialChips = @()

foreach ($dev in $usbDevices) {
    if ($dev.DeviceID -match 'VID_([0-9A-F]{4})&PID_([0-9A-F]{4})') {
        $vid = $Matches[1].ToUpper()
        $pid = $Matches[2].ToUpper()

        # 检查是否是已知串口芯片
        $isSerialChip = $false
        foreach ($knownVid in $vidTable.Keys) {
            if ($vid -eq $knownVid) {
                $foundSerialChips += "$vid`:$pid - $($vidTable[$knownVid])"
                $isSerialChip = $true
                break
            }
        }
    }
}

if ($foundSerialChips.Count -gt 0) {
    Write-Host "  发现的串口/USB芯片:"
    $foundSerialChips | Select-Object -Unique | ForEach-Object { Write-Host "    • $_" }
} else {
    Write-Host "  未发现常见的 USB 串口芯片"
}

Write-Host ""
Write-Host "--- 状态异常的 USB 设备 ---" -ForegroundColor Red
$badUsb = $usbDevices | Where-Object { $_.Status -ne 'OK' }
if ($badUsb) {
    $badUsb | Select-Object Name, DeviceID, Status | Format-Table -AutoSize
} else {
    Write-Host "  所有 USB 设备状态正常" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== 驱动状态检查 ===" -ForegroundColor Cyan

# 检查 CH340 驱动
$ch340drv = Get-ChildItem -Path "C:\Windows\System32\DriverStore\FileRepository" -Filter "ch34*" -Directory -ErrorAction SilentlyContinue
if ($ch340drv) {
    Write-Host "  ✅ CH340 驱动已安装: $($ch340drv.Name)" -ForegroundColor Green
} else {
    Write-Host "  ❌ CH340 驱动未安装" -ForegroundColor Red
}

# 检查 CP2102 驱动
$cp2102drv = Get-ChildItem -Path "C:\Windows\System32\DriverStore\FileRepository" -Filter "*silab*" -Directory -ErrorAction SilentlyContinue
if ($cp2102drv) {
    Write-Host "  ✅ CP2102 驱动已安装: $($cp2102drv.Name)" -ForegroundColor Green
} else {
    Write-Host "  ⚠️  CP2102 驱动未安装" -ForegroundColor Yellow
}

# 检查 FTDI 驱动
$ftdidrv = Get-ChildItem -Path "C:\Windows\System32\DriverStore\FileRepository" -Filter "*ftdi*" -Directory -ErrorAction SilentlyContinue
if ($ftdidrv) {
    Write-Host "  ✅ FTDI 驱动已安装: $($ftdidrv.Count) 个包" -ForegroundColor Green
} else {
    Write-Host "  ⚠️  FTDI 驱动未安装" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== 诊断建议 ===" -ForegroundColor Magenta

# 根据检测结果给出建议
if ($ports.Count -eq 0) {
    Write-Host "  ❌ 未检测到任何串口设备"
    Write-Host "     建议:"
    Write-Host "     1. 检查 USB 线是否是数据线（不是充电线）"
    Write-Host "     2. 换一个 USB 接口试试"
    Write-Host "     3. 确认设备已上电"
} elseif ($ports | Where-Object { $_.Status -ne 'OK' }) {
    Write-Host "  ⚠️  有串口设备但状态异常"
    Write-Host "     建议:"
    Write-Host "     1. 重新插拔设备"
    Write-Host "     2. 尝试重新安装驱动"
    Write-Host "     3. 换一个 USB 接口"
} else {
    Write-Host "  ✅ 检测到串口设备，状态正常"
}

Write-Host ""
Write-Host "检测完成！" -ForegroundColor Cyan
