#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
验证修正后的39字节指令格式
"""

def generate_39byte_command(year=2025, month=6, day=14, hour=18, minute=15, second=20, millisec=810, display_on=True):
    """生成39字节时间显示指令 - 修正版"""
    
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

def parse_year(byte4, byte5):
    """解析年份（小端序）"""
    return (byte5 << 8) | byte4

def main():
    print("🔧 验证修正后的39字节指令格式")
    print("=" * 60)
    
    # 生成2025年6月14日的指令
    command = generate_39byte_command(2025, 6, 14, 18, 15, 20, 810, True)
    
    print(f"🔢 HEX: {format_hex_string(command)}")
    print()
    
    # 解析年份
    year_parsed = parse_year(command[4], command[5])
    print(f"📅 年份解析:")
    print(f"   字节4: 0x{command[4]:02X} ({command[4]})")
    print(f"   字节5: 0x{command[5]:02X} ({command[5]})")
    print(f"   解析结果: {year_parsed} {'✅' if year_parsed == 2025 else '❌'}")
    print()
    
    # 解析月日
    print(f"📆 月日解析:")
    print(f"   字节6 (月): {command[6]} ({'6月' if command[6] == 6 else '❌'})")
    print(f"   字节7 (日): {command[7]} ({'14日' if command[7] == 14 else '❌'})")
    print()
    
    # 时间解析
    print(f"⏰ 时间解析:")
    print(f"   字节10 (时): {command[10]} ({'18时' if command[10] == 18 else '❌'})")
    print(f"   字节11 (分): {command[11]} ({'15分' if command[11] == 15 else '❌'})")
    print(f"   字节12 (秒): {command[12]} ({'20秒' if command[12] == 20 else '❌'})")
    print(f"   字节13 (毫秒/10): {command[13]} ({'81' if command[13] == 81 else '❌'})")
    print()
    
    print(f"🔍 校验和: 0x{command[38]:02X}")
    print(f"📏 指令长度: {len(command)}字节")
    
    print()
    print("🎯 关闭显示指令:")
    command_off = generate_39byte_command(2025, 6, 14, 18, 15, 20, 810, False)
    print(f"🔢 HEX: {format_hex_string(command_off)}")
    print(f"🔍 校验和: 0x{command_off[38]:02X}")

if __name__ == "__main__":
    main() 