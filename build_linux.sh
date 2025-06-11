#!/bin/bash

# =================================================================
# TCPImg - Linux编译脚本 (Qt 5.12)
# =================================================================

echo "=== TCPImg Linux编译脚本 ==="
echo "适用于 Qt 5.12 版本"
echo ""

# 检查Qt版本
echo "检查Qt版本..."
qmake --version
echo ""

# 检查当前目录
if [ ! -f "TCPImg.pro" ]; then
    echo "错误：在当前目录中找不到 TCPImg.pro 文件"
    echo "请确保在项目根目录中运行此脚本"
    exit 1
fi

# 清理之前的构建
echo "清理之前的构建文件..."
rm -rf build/
rm -f Makefile
rm -f TCPImg
rm -f *.o
rm -f moc_*
rm -f ui_*

# 创建构建目录
mkdir -p build
cd build

echo "开始配置项目..."
qmake ../TCPImg.pro

if [ $? -ne 0 ]; then
    echo "错误：qmake配置失败"
    exit 1
fi

echo "开始编译..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "错误：编译失败"
    exit 1
fi

echo ""
echo "=== 编译完成！ ==="
echo "可执行文件位置：$(pwd)/TCPImg"
echo ""

# 检查可执行文件
if [ -f "TCPImg" ]; then
    echo "文件信息："
    ls -la TCPImg
    echo ""
    echo "运行程序："
    echo "cd $(pwd)"
    echo "./TCPImg"
else
    echo "警告：找不到可执行文件"
fi

echo ""
echo "=== 构建完成 ===" 