#!/bin/bash

echo "🔧 串口接收功能测试程序编译脚本"
echo "=================================="

# 检查Qt环境
if ! command -v qmake &> /dev/null; then
    echo "❌ 错误：未找到qmake命令，请确保Qt开发环境已安装"
    exit 1
fi

# 显示Qt版本
echo "📋 Qt版本信息："
qmake -version

# 创建临时项目文件
cat > test_serial_receive.pro << EOF
QT += core serialport
QT -= gui

CONFIG += c++11 console
CONFIG -= app_bundle

TARGET = test_serial_receive
TEMPLATE = app

SOURCES += test_serial_receive.cpp
EOF

echo ""
echo "🔨 开始编译串口测试程序..."

# 清理旧文件
rm -f test_serial_receive test_serial_receive.exe Makefile

# 生成Makefile
if qmake test_serial_receive.pro; then
    echo "✅ qmake执行成功"
else
    echo "❌ qmake执行失败"
    exit 1
fi

# 编译程序
if make; then
    echo ""
    echo "🎉 编译成功！"
    echo ""
    echo "📋 使用说明："
    echo "1. 运行程序：./test_serial_receive (Linux) 或 test_serial_receive.exe (Windows)"
    echo "2. 程序会列出可用的串口"
    echo "3. 输入要测试的串口名称（如 COM3 或 /dev/ttyUSB0）"
    echo "4. 输入波特率（默认9600）"
    echo "5. 程序将监听串口数据并显示"
    echo ""
    echo "🔍 测试建议："
    echo "- 使用串口助手或其他设备向该串口发送数据"
    echo "- 观察是否能正确接收和显示数据"
    echo "- 验证16进制和文本显示是否正确"
else
    echo "❌ 编译失败"
    exit 1
fi

# 清理临时文件
rm -f test_serial_receive.pro Makefile *.o

echo ""
echo "✨ 串口测试程序准备就绪！" 