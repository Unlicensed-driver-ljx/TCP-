/**
 * @file test_high_resolution.cpp
 * @brief 千兆网高分辨率图像接收测试
 * 
 * 测试1280×1024×8bit×2tap×20帧数据的接收能力
 * 验证千兆网络环境下的高速数据传输性能
 */

#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QElapsedTimer>
#include "ctcpimg.h"

class HighResolutionTest : public QObject
{
    Q_OBJECT

public:
    HighResolutionTest(QObject *parent = nullptr) : QObject(parent)
    {
        // 创建TCP图像接收器
        m_tcpImg = new CTCPImg(this);
        
        // 设置高分辨率参数：1280×1024×2通道（8bit深度）
        bool success = m_tcpImg->setImageResolution(1280, 1024, 2);
        if (!success) {
            qDebug() << "❌ 分辨率设置失败！";
            return;
        }
        
        qDebug() << "✅ 分辨率配置成功：1280×1024×2通道";
        qDebug() << "📊 单帧数据大小：" << (1280 * 1024 * 2) << "字节 ≈" << (1280 * 1024 * 2 / 1024.0 / 1024.0) << "MB";
        qDebug() << "🚀 20帧/秒数据流量：" << (1280 * 1024 * 2 * 20 / 1024.0 / 1024.0) << "MB/秒";
        qDebug() << "🌐 千兆网占用率：" << (1280 * 1024 * 2 * 20 * 8 / 1000000.0) << "Mbps（理论值400Mbps）";
        
        // 连接信号槽监控性能
        connect(m_tcpImg, &CTCPImg::tcpImgReadySig, this, &HighResolutionTest::onImageReceived);
        connect(m_tcpImg, &CTCPImg::signal_showframestruct, this, &HighResolutionTest::onFrameInfo);
        
        // 启用自动重连，网络波动时保持连接
        m_tcpImg->setAutoReconnect(true, 10, 2000);  // 最大10次重连，2秒间隔
        
        // 性能监控定时器
        m_performanceTimer = new QTimer(this);
        connect(m_performanceTimer, &QTimer::timeout, this, &HighResolutionTest::printPerformanceStats);
        m_performanceTimer->start(5000);  // 每5秒输出性能统计
        
        // 初始化性能计数器
        m_frameCount = 0;
        m_totalBytes = 0;
        m_startTime.start();
        
        qDebug() << "\n🔗 准备连接服务器，请确保：";
        qDebug() << "   1. 服务器正在发送1280×1024×2通道图像数据";
        qDebug() << "   2. 网络环境为千兆网（1Gbps）";
        qDebug() << "   3. 服务器帧率设置为20fps";
        qDebug() << "   4. 数据格式为8bit深度，2tap模式\n";
    }
    
public slots:
    /**
     * @brief 开始测试
     * @param serverIP 服务器IP地址
     * @param serverPort 服务器端口
     */
    void startTest(const QString &serverIP, int serverPort)
    {
        qDebug() << "🚀 开始高分辨率接收测试...";
        qDebug() << "📡 连接到服务器：" << serverIP << ":" << serverPort;
        
        m_tcpImg->start(serverIP, serverPort);
    }
    
private slots:
    /**
     * @brief 图像接收完成处理
     */
    void onImageReceived()
    {
        m_frameCount++;
        m_totalBytes += (1280 * 1024 * 2);  // 单帧数据大小
        
        // 每100帧输出一次即时状态
        if (m_frameCount % 100 == 0) {
            qDebug() << QString("📊 接收进度：第%1帧，累计%2MB")
                        .arg(m_frameCount)
                        .arg(m_totalBytes / 1024.0 / 1024.0, 0, 'f', 2);
        }
    }
    
    /**
     * @brief 帧结构信息处理
     */
    void onFrameInfo(const QString &info)
    {
        static int frameInfoCount = 0;
        frameInfoCount++;
        
        // 只显示前几帧的详细信息，避免日志过多
        if (frameInfoCount <= 5) {
            qDebug() << "🔍 帧结构：" << info;
        }
    }
    
    /**
     * @brief 性能统计输出
     */
    void printPerformanceStats()
    {
        if (m_frameCount == 0) {
            qDebug() << "⏳ 等待数据接收...";
            return;
        }
        
        double elapsedSeconds = m_startTime.elapsed() / 1000.0;
        double avgFPS = m_frameCount / elapsedSeconds;
        double avgThroughput = (m_totalBytes / 1024.0 / 1024.0) / elapsedSeconds;  // MB/s
        double avgBandwidth = avgThroughput * 8;  // Mbps
        
        qDebug() << "\n📈 === 性能统计报告 ===";
        qDebug() << QString("⏱️  运行时间：%1秒").arg(elapsedSeconds, 0, 'f', 1);
        qDebug() << QString("🖼️  接收帧数：%1帧").arg(m_frameCount);
        qDebug() << QString("📊 平均帧率：%1 FPS").arg(avgFPS, 0, 'f', 2);
        qDebug() << QString("💾 数据吞吐：%1 MB/s").arg(avgThroughput, 0, 'f', 2);
        qDebug() << QString("🌐 网络带宽：%1 Mbps").arg(avgBandwidth, 0, 'f', 1);
        qDebug() << QString("📡 目标帧率：20 FPS，目标带宽：400 Mbps");
        
        // 性能评估
        if (avgFPS >= 18.0) {  // 90%目标帧率
            qDebug() << "✅ 帧率表现：优秀";
        } else if (avgFPS >= 15.0) {  // 75%目标帧率
            qDebug() << "⚠️  帧率表现：良好，可能有轻微网络延迟";
        } else {
            qDebug() << "❌ 帧率表现：需要优化，检查网络连接";
        }
        
        if (avgBandwidth >= 350) {  // 87.5%目标带宽
            qDebug() << "✅ 带宽利用：优秀";
        } else if (avgBandwidth >= 300) {  // 75%目标带宽
            qDebug() << "⚠️  带宽利用：良好";
        } else {
            qDebug() << "❌ 带宽利用：偏低，检查网络配置";
        }
        
        // 连接状态检查
        qDebug() << QString("🔗 连接状态：%1").arg(getConnectionStateString());
        qDebug() << "========================\n";
    }
    
private:
    /**
     * @brief 获取连接状态字符串
     */
    QString getConnectionStateString()
    {
        switch (m_tcpImg->getConnectionState()) {
            case QAbstractSocket::ConnectedState:
                return "已连接";
            case QAbstractSocket::ConnectingState:
                return "连接中";
            case QAbstractSocket::UnconnectedState:
                return "未连接";
            default:
                return "其他状态";
        }
    }
    
private:
    CTCPImg *m_tcpImg;
    QTimer *m_performanceTimer;
    QElapsedTimer m_startTime;
    
    // 性能统计变量
    int m_frameCount;
    qint64 m_totalBytes;
};

/**
 * @brief 主函数
 */
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "🎯 千兆网高分辨率图像接收测试";
    qDebug() << "📋 测试参数：1280×1024×8bit×2tap×20fps";
    qDebug() << "💡 确保网络环境：千兆网卡、千兆交换机、CAT6网线\n";
    
    HighResolutionTest test;
    
    // 根据命令行参数或使用默认值
    QString serverIP = "192.168.1.100";  // 默认服务器IP
    int serverPort = 8080;              // 默认服务器端口
    
    if (argc >= 3) {
        serverIP = argv[1];
        serverPort = QString(argv[2]).toInt();
    }
    
    // 延迟启动测试，给用户时间查看配置信息
    QTimer::singleShot(3000, [&test, serverIP, serverPort]() {
        test.startTest(serverIP, serverPort);
    });
    
    qDebug() << "⏳ 3秒后开始连接服务器...";
    qDebug() << "💡 使用方法：./test_high_resolution [服务器IP] [端口]";
    qDebug() << QString("📡 当前配置：%1:%2\n").arg(serverIP).arg(serverPort);
    
    return app.exec();
}

#include "test_high_resolution.moc" 