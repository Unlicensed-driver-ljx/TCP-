#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
自动切换字符显示功能测试
验证39字节指令的正确性
"""

def generate_39byte_command(year=2025, month=6, day=14, hour=18, minute=15, second=20, millisec=810, display_on=True):
    """生成39字节时间显示指令"""
    
    command = []
    
    # 字节0-3: 固定帧头 90 EB 64 00
    command.extend([0x90, 0xEB, 0x64, 0x00])
    
    # 字节4-5: 年份 (小端序：低字节在前)
    command.append(year & 0xFF)         # 年份低字节在前
    command.append((year >> 8) & 0xFF)  # 年份高字节在后
    
    # 字节6-7: 月和日 (十进制值直接编码)
    command.append(month)
    command.append(day)
    
    # 字节8: 0F (控制字符显示)
    command.append(0x0F)
    
    # 字节9: 00=打开显示, 01=关闭显示
    command.append(0x00 if display_on else 0x01)
    
    # 字节10-13: 时分秒毫秒
    command.append(hour)
    command.append(minute)
    command.append(second)
    command.append(millisec // 10)  # 毫秒/10，范围0-99
    
    # 字节14-37: 随意填充 (使用00，共24个字节)
    command.extend([0x00] * 24)
    
    # 字节38: 前38字节总和校验(低8位)
    checksum = sum(command[:38]) & 0xFF
    command.append(checksum)
    
    return command

def format_hex_string(data):
    """格式化为16进制字符串"""
    return ' '.join(f'{byte:02X}' for byte in data)

def simulate_auto_switch():
    """模拟自动切换功能"""
    print("🔄 模拟自动切换字符显示功能")
    print("=" * 60)
    
    # 模拟5秒的自动切换
    for i in range(5):
        display_on = (i % 2 == 0)  # 0,2,4秒开启，1,3秒关闭
        
        # 模拟不同的时间点
        second = 20 + i
        command = generate_39byte_command(2025, 6, 14, 18, 15, second, 810, display_on)
        
        action = "开启字符显示" if display_on else "关闭字符显示"
        icon = "⏰" if display_on else "🚫"
        
        print(f"第{i+1}秒: {icon} {action}")
        print(f"   HEX: {format_hex_string(command)}")
        print(f"   年份: 2025 (E9 07)")
        print(f"   月日: 6月14日 (06 0E)")
        print(f"   时间: 18:15:{second}.81")
        print(f"   显示: 第9字节 = {'00' if display_on else '01'}")
        print(f"   校验: 0x{command[38]:02X}")
        print()

def main():
    print("🧪 自动切换字符显示功能测试")
    print("验证修正后的39字节指令格式")
    print()
    
    # 测试开启指令
    print("📝 开启字符显示指令:")
    cmd_on = generate_39byte_command(2025, 6, 14, 18, 15, 20, 810, True)
    print(f"HEX: {format_hex_string(cmd_on)}")
    print(f"校验和: 0x{cmd_on[38]:02X}")
    print()
    
    # 测试关闭指令
    print("📝 关闭字符显示指令:")
    cmd_off = generate_39byte_command(2025, 6, 14, 18, 15, 20, 810, False)
    print(f"HEX: {format_hex_string(cmd_off)}")
    print(f"校验和: 0x{cmd_off[38]:02X}")
    print()
    
    # 验证校验和差异
    print("🔍 校验和差异验证:")
    print(f"开启指令校验和: 0x{cmd_on[38]:02X}")
    print(f"关闭指令校验和: 0x{cmd_off[38]:02X}")
    print(f"差异: {cmd_off[38] - cmd_on[38]} (关闭比开启大1，因为第9字节从00变为01)")
    print()
    
    # 模拟自动切换
    simulate_auto_switch()

if __name__ == "__main__":
    main() 