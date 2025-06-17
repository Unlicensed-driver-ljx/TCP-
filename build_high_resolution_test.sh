#!/bin/bash

# 千兆网高分辨率图像接收测试编译脚本
# 用于编译1280×1024×8bit×2tap×20fps性能测试程序

echo "🚀 千兆网高分辨率图像接收测试编译器"
echo "============================================"

# 检查Qt版本
echo "🔍 检查Qt环境..."
qt_version=$(qmake --version | grep "Qt version" | awk '{print $4}')
if [ -z "$qt_version" ]; then
    echo "❌ 错误：未找到Qt环境，请安装Qt开发包"
    echo "   Ubuntu/Debian: sudo apt-get install qt5-default qtbase5-dev"
    echo "   CentOS/Fedora: sudo yum install qt5-qtbase-devel"
    exit 1
fi

echo "✅ Qt版本：$qt_version"

# 检查必需的源文件
echo "🔍 检查源文件..."
required_files=("ctcpimg.h" "ctcpimg.cpp" "sysdefine.h" "test_high_resolution.cpp")
for file in "${required_files[@]}"; do
    if [ ! -f "$file" ]; then
        echo "❌ 错误：缺少必需文件 $file"
        exit 1
    fi
done
echo "✅ 所有源文件检查完成"

# 创建测试专用的项目文件
echo "📝 生成测试项目配置..."
cat > test_high_resolution.pro << 'EOF'
# 千兆网高分辨率图像接收测试项目配置
QT += core network
QT -= gui

TARGET = test_high_resolution
CONFIG += console
CONFIG -= app_bundle
CONFIG += c++11

# 输出目录
DESTDIR = ./

# 源文件
SOURCES += \
    test_high_resolution.cpp \
    ctcpimg.cpp

# 头文件
HEADERS += \
    ctcpimg.h \
    sysdefine.h

# 编译选项
QMAKE_CXXFLAGS += -O2 -Wall

# Qt版本兼容性
lessThan(QT_MAJOR_VERSION, 6) {
    message("编译目标：Qt 5.x (兼容模式)")
    DEFINES += QT_NO_FOREACH
} else {
    message("编译目标：Qt 6.x")
}

# 平台特定设置
unix {
    QMAKE_LFLAGS += -Wl,-rpath=.
}

message("项目：千兆网高分辨率图像接收测试")
message("目标：1280×1024×8bit×2tap×20fps性能验证")
EOF

# 创建构建目录
echo "📁 准备构建环境..."
BUILD_DIR="build_test"
if [ -d "$BUILD_DIR" ]; then
    echo "🧹 清理旧的构建目录..."
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

# 进入构建目录
cd "$BUILD_DIR"

# 运行qmake
echo "⚙️  配置项目..."
qmake ../test_high_resolution.pro
if [ $? -ne 0 ]; then
    echo "❌ qmake配置失败"
    exit 1
fi

# 编译项目
echo "🔨 编译测试程序..."
cpu_cores=$(nproc 2>/dev/null || echo "1")
echo "🚀 使用 $cpu_cores 个CPU核心进行编译"

make -j$cpu_cores
if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi

# 检查编译结果
if [ -f "test_high_resolution" ]; then
    echo "✅ 编译成功！"
    echo ""
    echo "📊 测试程序信息："
    echo "   - 可执行文件：./build_test/test_high_resolution"
    echo "   - 测试场景：1280×1024×8bit×2tap×20fps"
    echo "   - 数据流量：50MB/秒 (400Mbps)"
    echo "   - 千兆网利用率：~40%"
    echo ""
    echo "🚀 使用方法："
    echo "   ./build_test/test_high_resolution [服务器IP] [端口]"
    echo ""
    echo "📝 示例："
    echo "   ./build_test/test_high_resolution 192.168.1.100 8080"
    echo ""
    echo "💡 提示："
    echo "   - 确保服务器正在发送1280×1024×2通道图像数据"
    echo "   - 使用千兆网卡和千兆交换机"
    echo "   - 推荐CAT6网线，长度<100米"
    echo "   - 测试期间监控CPU和内存使用率"
    echo ""
    echo "🔍 性能监控："
    echo "   - 程序会每5秒输出性能统计"
    echo "   - 显示帧率、吞吐量、带宽利用率"
    echo "   - 自动评估传输质量"
    
    # 创建快速启动脚本
    cd ..
    cat > run_test.sh << 'EOF'
#!/bin/bash
echo "🎯 千兆网高分辨率图像接收测试"
echo "================================"
echo "测试参数：1280×1024×8bit×2tap×20fps"
echo ""

if [ $# -lt 2 ]; then
    echo "用法: $0 <服务器IP> <端口>"
    echo "示例: $0 192.168.1.100 8080"
    exit 1
fi

SERVER_IP=$1
SERVER_PORT=$2

echo "连接到服务器：$SERVER_IP:$SERVER_PORT"
echo "启动性能测试..."
echo ""

./build_test/test_high_resolution $SERVER_IP $SERVER_PORT
EOF
    chmod +x run_test.sh
    
    echo "🎯 快速启动脚本已创建：./run_test.sh"
    echo ""
    
else
    echo "❌ 编译失败：未找到可执行文件"
    exit 1
fi

echo "🎉 千兆网高分辨率测试程序编译完成！"
echo "   准备开始您的400Mbps高速图像传输测试吧！" 