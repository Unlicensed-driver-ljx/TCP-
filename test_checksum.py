#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
30字节时间字符显示指令校验和测试脚本
用于验证C++实现的校验和计算是否正确
"""

import sys
from datetime import datetime

def calculate_checksum(data_bytes):
    """计算校验和（低8位）"""
    return sum(data_bytes) & 0xFF

def generate_test_command():
    """生成测试指令（30字节）"""
    command = []
    
    # 字节0-3: 90 EB 64 00 (固定帧头)
    command.extend([0x90, 0xEB, 0x64, 0x00])
    
    # 字节4-7: 年月日时间
    now = datetime.now()
    year = now.year
    command.extend([
        (year >> 8) & 0xFF,  # 年高字节
        year & 0xFF,         # 年低字节
        now.month,           # 月
        now.day              # 日
    ])
    
    # 字节8: 0F (控制字符显示)
    command.append(0x0F)
    
    # 字节9: 00=打开显示, 01=关闭显示
    command.append(0x00)  # 默认打开显示
    
    # 字节10-28: 填充时分秒等信息
    command.extend([
        now.hour,
        now.minute,
        now.second,
        now.microsecond // 1000 & 0xFF  # 毫秒低字节
    ])
    
    # 继续填充到29字节（索引0-28）
    while len(command) < 29:
        command.append(0x00)
    
    # 字节29: 校验和
    checksum = calculate_checksum(command)
    command.append(checksum)
    
    return command

def main():
    if len(sys.argv) < 2:
        print("测试30字节时间字符显示指令校验和计算")
        print("用法1: python test_checksum.py \"90 EB 64 00 ...\" (手动输入29字节)")
        print("用法2: python test_checksum.py auto (自动生成测试指令)")
        
        # 生成示例指令
        test_command = generate_test_command()
        hex_str = " ".join([f"{b:02X}" for b in test_command])
        print(f"\n示例30字节指令:")
        print(f"{hex_str}")
        print(f"校验和: 0x{test_command[-1]:02X}")
        return

    if sys.argv[1].lower() == "auto":
        # 自动生成测试指令
        command = generate_test_command()
        hex_str = " ".join([f"{b:02X}" for b in command])
        print(f"生成的30字节指令:")
        print(f"{hex_str}")
        print(f"校验和: 0x{command[-1]:02X}")
        
        # 验证校验和
        checksum_verify = calculate_checksum(command[:-1])
        print(f"验证校验和: 0x{checksum_verify:02X}")
        print(f"校验结果: {'✅ 正确' if command[-1] == checksum_verify else '❌ 错误'}")
        
    else:
        # 手动输入指令验证
        hex_input = sys.argv[1]
        try:
            data = [int(byte, 16) for byte in hex_input.strip().split()]
            
            if len(data) == 30:
                # 完整30字节，验证校验和
                payload = data[:-1]  # 前29字节
                received_checksum = data[-1]  # 最后1字节
                calculated_checksum = calculate_checksum(payload)
                
                print(f"输入的30字节指令:")
                print(f"{' '.join([f'{b:02X}' for b in data])}")
                print(f"接收到的校验和: 0x{received_checksum:02X}")
                print(f"计算出的校验和: 0x{calculated_checksum:02X}")
                print(f"校验结果: {'✅ 正确' if received_checksum == calculated_checksum else '❌ 错误'}")
                
            elif len(data) == 29:
                # 29字节，计算校验和
                checksum = calculate_checksum(data)
                print(f"输入的29字节数据:")
                print(f"{' '.join([f'{b:02X}' for b in data])}")
                print(f"计算出的校验和: 0x{checksum:02X}")
                print(f"完整30字节指令:")
                full_command = data + [checksum]
                print(f"{' '.join([f'{b:02X}' for b in full_command])}")
                
            else:
                print(f"错误: 期望29或30字节，实际收到{len(data)}字节")
                
        except ValueError:
            print("错误: 请确保输入的是有效的十六进制字节字符串")

if __name__ == "__main__":
    main() 