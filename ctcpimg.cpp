#include "ctcpimg.h"
#include <QDebug>
#include <QtEndian>  // Qt 5.12字节序转换函数

/**
 * @brief CTCPImg构造函数
 * @param parent 父对象指针
 * 
 * 初始化TCP图像传输对象，设置默认参数和分配内存缓冲区
 */
CTCPImg::CTCPImg(QObject *parent)
: QObject(parent)
{
    // 初始化标志位，表示当前未开始刷新
    m_brefresh = false;
    
    // 初始化重连相关参数
    m_reconnectTimer = new QTimer(this);
    m_serverPort = 0;
    m_reconnectAttempts = 0;
    m_maxReconnectAttempts = 5;
    m_reconnectInterval = 3000;  // 3秒重连间隔
    m_autoReconnectEnabled = true;  // 默认启用自动重连
    
    // 使用默认的图像参数初始化
    m_imageWidth = WIDTH;
    m_imageHeight = HEIGHT;
    m_imageChannels = CHANLE;
    m_tapMode = 1;  // 默认1tap模式
    
    // 计算图像数据总大小：宽度 × 高度 × 通道数
    m_totalsize = m_imageWidth * m_imageHeight * m_imageChannels;
    
    // 为图像帧缓冲区分配内存并初始化为0
    frameBuffer = new char[m_totalsize];
    memset((void*)frameBuffer, 0, m_totalsize);

    // 初始化TCP套接字
    TCP_sendMesSocket = NULL;
    this->TCP_sendMesSocket = new QTcpSocket();
    TCP_sendMesSocket->abort();  // 中止任何现有连接

    // 连接信号槽，处理TCP连接的各种状态
    connect(TCP_sendMesSocket, SIGNAL(connected()), this, SLOT(slot_connected()));
    connect(TCP_sendMesSocket, SIGNAL(readyRead()), this, SLOT(slot_recvmessage()));
    connect(TCP_sendMesSocket, SIGNAL(disconnected()), this, SLOT(slot_disconnect()));
    // 添加错误处理信号连接 - Qt版本兼容处理
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15+ 和 Qt 6.x 使用 errorOccurred 信号
    connect(TCP_sendMesSocket, &QTcpSocket::errorOccurred, 
            this, &CTCPImg::slot_socketError);
#else
    // Qt 5.12-5.14 使用 error 信号
    connect(TCP_sendMesSocket, SIGNAL(error(QAbstractSocket::SocketError)), 
            this, SLOT(slot_socketError(QAbstractSocket::SocketError)));
#endif
    
    // 连接重连定时器信号
    connect(m_reconnectTimer, &QTimer::timeout, this, &CTCPImg::slot_reconnect);
    m_reconnectTimer->setSingleShot(true);  // 设置为单次触发
    
    qDebug() << "CTCPImg对象初始化完成，图像缓冲区大小：" << m_totalsize << "字节";
    qDebug() << "自动重连功能已启用，最大重连次数：" << m_maxReconnectAttempts << "，重连间隔：" << m_reconnectInterval << "ms";
}

/**
 * @brief CTCPImg析构函数
 * 
 * 清理资源，释放内存和关闭网络连接
 */
CTCPImg::~CTCPImg(void)
{
    // 停止重连定时器
    if (m_reconnectTimer) {
        m_reconnectTimer->stop();
    }
    
    // 修复：正确释放QTcpSocket对象
   if(NULL != TCP_sendMesSocket)
   {
        TCP_sendMesSocket->disconnectFromHost();  // 优雅断开连接
        TCP_sendMesSocket->deleteLater();         // 使用deleteLater()安全删除
       TCP_sendMesSocket = NULL;
   }

    // 修复：使用delete[]释放数组内存
    if (frameBuffer != NULL)
    {
        delete[] frameBuffer;  // 修复：使用delete[]而不是delete
        frameBuffer = NULL;
    }
    
    qDebug() << "CTCPImg对象销毁完成，资源已释放";
}

/**
 * @brief 获取图像帧缓冲区指针
 * @return 返回图像数据缓冲区的指针
 */
char *CTCPImg::getFrameBuffer()
{
    return frameBuffer;
}

/**
 * @brief 启动TCP连接
 * @param strAddr 服务器IP地址
 * @param port 服务器端口号
 * 
 * 连接到指定的TCP服务器，开始图像数据传输
 */
void CTCPImg::start(QString strAddr, int port)
{
    // 检查输入参数的有效性
    if (strAddr.isEmpty()) {
        qDebug() << "错误：IP地址不能为空";
        return;
    }
    
    if (port <= 0 || port > 65535) {
        qDebug() << "错误：端口号无效，有效范围：1-65535";
        return;
    }
    
    // 保存连接参数用于重连
    m_serverAddress = strAddr;
    m_serverPort = port;
    m_reconnectAttempts = 0;  // 重置重连计数
    
    // 停止任何正在进行的重连尝试
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
        qDebug() << "停止之前的重连尝试";
    }
    
    // 如果已经连接，先断开
    if (TCP_sendMesSocket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "检测到现有连接，正在断开...";
        TCP_sendMesSocket->disconnectFromHost();
    }
    
    // 禁用代理，避免系统代理设置干扰图像传输
    TCP_sendMesSocket->setProxy(QNetworkProxy::NoProxy);
    
    qDebug() << "开始连接到服务器：" << strAddr << ":" << port;
    qDebug() << "自动重连状态：" << (m_autoReconnectEnabled ? "启用" : "禁用");
    this->TCP_sendMesSocket->connectToHost(QHostAddress(strAddr), port);
}

/**
 * @brief TCP连接建立成功的槽函数
 * 
 * 当TCP连接成功建立时被调用，设置相关状态标志
 */
void CTCPImg::slot_connected()
{
    m_brefresh = true;
    pictmp.clear();  // 清空接收缓冲区
    
    qDebug() << "✅ [连接调试] TCP连接建立成功，准备接收图像数据";
    qDebug() << "✅ [连接调试] 连接到服务器：" << m_serverAddress << ":" << m_serverPort;
    qDebug() << "✅ [连接调试] 套接字状态：" << TCP_sendMesSocket->state();
    qDebug() << "✅ [连接调试] 本地地址：" << TCP_sendMesSocket->localAddress().toString() << ":" << TCP_sendMesSocket->localPort();
    qDebug() << "✅ [连接调试] 远程地址：" << TCP_sendMesSocket->peerAddress().toString() << ":" << TCP_sendMesSocket->peerPort();
    
    // 连接成功，重置重连计数
    int previousAttempts = m_reconnectAttempts;
    m_reconnectAttempts = 0;
    
    if (m_reconnectTimer->isActive()) {
        qDebug() << "✅ [连接调试] 停止重连定时器";
        m_reconnectTimer->stop();
    }
    
    if (previousAttempts > 0) {
        qDebug() << QString("✅ [连接调试] 重连成功！经过 %1 次尝试后连接建立").arg(previousAttempts);
    } else {
        qDebug() << "✅ [连接调试] 首次连接成功";
    }
    
    qDebug() << "🔄 重连计数已重置，当前连接状态：已连接";
}

/**
 * @brief 发送消息的槽函数（预留接口）
 * 
 * 当前版本未实现具体功能，预留用于后续扩展
 */
void CTCPImg::slot_sendmessage()
{
    // 预留接口，用于发送消息给服务器
    qDebug() << "发送消息接口调用（当前未实现具体功能）";
}

/**
 * @brief 接收TCP消息的核心处理函数
 * 
 * 处理从服务器接收到的数据，包括：
 * 1. 解析数据大小信息
 * 2. 累积图像数据
 * 3. 数据完整性检查
 * 4. 发送确认消息
 */
void CTCPImg::slot_recvmessage()
{
    // 检查连接状态
    if (!m_brefresh || TCP_sendMesSocket->state() != QAbstractSocket::ConnectedState) {
        qDebug() << "警告：连接状态异常，忽略接收到的数据";
        return;
    }
    
    QByteArray byteArray = this->TCP_sendMesSocket->readAll();

    // 数据有效性检查
    if (byteArray.isEmpty()) {
        qDebug() << "警告：接收到空数据包";
        return;
    }

    // 第一阶段：解析数据大小信息
    if (byteArray.contains("size=")) {
        qDebug() << "🔢 接收到数据大小信息";
        qDebug() << "📊 size消息原始数据：" << formatDataForDebug(byteArray, byteArray.size());
        qDebug() << "📊 size消息内容：" << QString(byteArray);
        qDebug() << "📊 数据包大小：" << byteArray.size() << "字节";
        
        // 提取并解析数据大小
        byteArray = byteArray.replace("size=", "");
        bool conversionOk = false;
        int receivedSize = byteArray.toInt(&conversionOk);
        
        // 数据大小有效性检查
        if (!conversionOk) {
            qDebug() << "❌ 错误：无法解析数据大小信息";
            return;
        }
        
        if (receivedSize <= 0 || receivedSize > 10 * 1024 * 1024) {  // 限制最大10MB
            qDebug() << "❌ 错误：数据大小异常：" << receivedSize << "字节";
            return;
        }
        
        m_totalsize = receivedSize;
        pictmp.clear();  // 清空之前的数据
        
        qDebug() << "✅ 预期接收图像数据大小：" << m_totalsize << "字节";
        qDebug() << "📏 预期图像参数：" << m_imageWidth << "x" << m_imageHeight << "x" << m_imageChannels;
        qDebug() << "💾 预期图像大小：" << (m_imageWidth * m_imageHeight * m_imageChannels) << "字节";
        qDebug() << "🚀 开始接收图像数据...";
        qDebug() << "-" << QString().fill('-', 60);  // 分隔线
    }
    // 第二阶段：累积图像数据
    else
    {
        // 🔍 如果是第一次接收图像数据，打印帧头信息
        if (pictmp.isEmpty()) {
            qDebug() << "📷 开始接收新图像帧数据 [第一个数据包]";
            qDebug() << "📊 帧头数据前20字节：" << formatDataForDebug(byteArray, 20);
            qDebug() << "📊 第一个数据包大小：" << byteArray.size() << "字节";
            
            // 🎯 验证预期的帧头：7E 7E
            QByteArray expectedHeader;
            expectedHeader.append(static_cast<char>(0x7E));
            expectedHeader.append(static_cast<char>(0x7E));
            
            bool headerValid = validateFrameHeader(byteArray, expectedHeader);
            if (headerValid) {
                qDebug() << "✅ 帧头验证成功！检测到预期的帧头：7E 7E";
            } else {
                qDebug() << "❌ 帧头验证失败！未检测到预期的帧头：7E 7E";
                
                // 🔍 尝试在数据中搜索帧头
                int headerPos = findFrameHeader(byteArray, expectedHeader);
                if (headerPos >= 0) {
                    qDebug() << QString("🔍 在偏移位置 %1 找到帧头 7E 7E").arg(headerPos);
                } else {
                    qDebug() << "🔍 在整个数据包中未找到帧头 7E 7E";
                }
                if (byteArray.size() >= 2) {
                    const unsigned char* data = reinterpret_cast<const unsigned char*>(byteArray.constData());
                    qDebug() << "❌ 实际帧头前2字节：" << QString("%1 %2")
                                .arg(data[0], 2, 16, QChar('0')).toUpper()
                                .arg(data[1], 2, 16, QChar('0')).toUpper();
                    
                    // 显示更多字节以便分析
                    if (byteArray.size() >= 8) {
                        qDebug() << "❌ 实际帧头前8字节：" << QString("%1 %2 %3 %4 %5 %6 %7 %8")
                                    .arg(data[0], 2, 16, QChar('0')).toUpper()
                                    .arg(data[1], 2, 16, QChar('0')).toUpper()
                                    .arg(data[2], 2, 16, QChar('0')).toUpper()
                                    .arg(data[3], 2, 16, QChar('0')).toUpper()
                                    .arg(data[4], 2, 16, QChar('0')).toUpper()
                                    .arg(data[5], 2, 16, QChar('0')).toUpper()
                                    .arg(data[6], 2, 16, QChar('0')).toUpper()
                                    .arg(data[7], 2, 16, QChar('0')).toUpper();
                    }
                }
            }
            
            // 检查是否有明显的数据模式
            if (byteArray.size() >= 4) {
                // Qt 5.12兼容的字节序转换
                const unsigned char* data = reinterpret_cast<const unsigned char*>(byteArray.constData());
                
                // 手动实现大端转换（网络字节序）
                quint32 firstFourBytes = (static_cast<quint32>(data[0]) << 24) |
                                        (static_cast<quint32>(data[1]) << 16) |
                                        (static_cast<quint32>(data[2]) << 8) |
                                        static_cast<quint32>(data[3]);
                qDebug() << "📊 前4字节作为大端整数：" << firstFourBytes;
                
                // 手动实现小端转换
                quint32 firstFourBytesLE = static_cast<quint32>(data[0]) |
                                          (static_cast<quint32>(data[1]) << 8) |
                                          (static_cast<quint32>(data[2]) << 16) |
                                          (static_cast<quint32>(data[3]) << 24);
                qDebug() << "📊 前4字节作为小端整数：" << firstFourBytesLE;
                
                // 额外显示前8个字节的详细信息
                if (byteArray.size() >= 8) {
                    QStringList byteValues;
                    for (int i = 0; i < 8; ++i) {
                        byteValues << QString("0x%1").arg(data[i], 2, 16, QChar('0')).toUpper();
                    }
                    qDebug() << "📊 前8字节详细：" << byteValues.join(" ");
                }
            }
        } else {
            qDebug() << "📦 继续接收数据包，大小：" << byteArray.size() << "字节";
        }
        
        pictmp.append(byteArray);
        int currentSize = pictmp.size();
        qDebug() << "累积接收数据：" << currentSize << "/" << m_totalsize << "字节" 
                 << "（进度：" << (currentSize * 100.0 / m_totalsize) << "%）";
        
        // 数据大小保护检查
        if (currentSize > m_totalsize + 1024) {  // 允许1KB的缓冲误差
            qDebug() << "错误：接收数据超出预期大小，可能存在数据传输错误";
            pictmp.clear();
            return;
        }
    }
    
    // 第三阶段：数据接收完成处理
    if (pictmp.length() == m_totalsize)
    {
        qDebug() << "🎯 完整图像帧接收完成！";
        qDebug() << "📊 完整帧头数据前20字节：" << formatDataForDebug(pictmp, 20);
        qDebug() << "📊 完整帧尾数据后20字节：" << formatDataForDebug(pictmp.right(20), 20);
        
                // 🎯 最终验证完整帧的帧头
        QByteArray expectedHeader;
        expectedHeader.append(static_cast<char>(0x7E));
        expectedHeader.append(static_cast<char>(0x7E));
        
        bool finalHeaderValid = validateFrameHeader(pictmp, expectedHeader);
        if (finalHeaderValid) {
            qDebug() << "✅ 完整帧头验证成功！确认帧头为：7E 7E";
            
            // 🔍 分析帧结构（7E 7E 后面的字节）
            if (pictmp.size() >= 8) {
                const unsigned char* data = reinterpret_cast<const unsigned char*>(pictmp.constData());
                qDebug() << "📊 帧结构分析：";
                qDebug() << "   - 帧头：7E 7E";
                qDebug() << QString("   - 第3字节：0x%1").arg(data[2], 2, 16, QChar('0')).toUpper();
                qDebug() << QString("   - 第4字节：0x%1").arg(data[3], 2, 16, QChar('0')).toUpper();
                qDebug() << QString("   - 第5字节：0x%1").arg(data[4], 2, 16, QChar('0')).toUpper();
                qDebug() << QString("   - 第6字节：0x%1").arg(data[5], 2, 16, QChar('0')).toUpper();
                qDebug() << QString("   - 第7字节：0x%1").arg(data[6], 2, 16, QChar('0')).toUpper();
                qDebug() << QString("   - 第8字节：0x%1").arg(data[7], 2, 16, QChar('0')).toUpper();
                
                // 分析可能的tap模式（假设在第3或第4字节）
                unsigned char byte3 = data[2];
                unsigned char byte4 = data[3];
                
                if (byte3 == 0x01 || byte4 == 0x01) {
                    qDebug() << "📊 可能的1tap模式标识";
                    m_tapMode = 1;
                } else if (byte3 == 0x02 || byte4 == 0x02) {
                    qDebug() << "📊 可能的2tap模式标识";
                    m_tapMode = 2;
                } else {
                    qDebug() << "📊 未识别的tap模式";
                    m_tapMode = 0;
                }
            }
        } else {
            qDebug() << "❌ 完整帧头验证失败！帧头不匹配：7E 7E";
        }
        
        qDebug() << "📊 图像数据统计信息：";
        qDebug() << "   - 总大小：" << m_totalsize << "字节";
        qDebug() << "   - 期望尺寸：" << m_imageWidth << "x" << m_imageHeight << "x" << m_imageChannels;
        qDebug() << "   - 期望大小：" << (m_imageWidth * m_imageHeight * m_imageChannels) << "字节";
        
        // 数据完整性的最终检查
        int expectedSize = m_imageWidth * m_imageHeight * m_imageChannels;
        if (m_totalsize > expectedSize) {
            qDebug() << "警告：接收数据大小超出当前图像缓冲区容量";
            // 只复制缓冲区容量范围内的数据
            memcpy(frameBuffer, pictmp.data(), expectedSize);
        } else {
            // 将接收到的图像数据复制到帧缓冲区
            memcpy(frameBuffer, pictmp.data(), m_totalsize);
        }
        
        // 发射图像数据就绪信号，通知界面更新
        emit tcpImgReadySig();
        
        // 向服务器发送确认消息
        if (TCP_sendMesSocket->state() == QAbstractSocket::ConnectedState) {
         //   TCP_sendMesSocket->write("OK");
            TCP_sendMesSocket->flush();  // 确保数据立即发送
        }
        
        qDebug() << "✅ 图像数据接收完成并已更新缓冲区，发送确认消息";
        qDebug() << "=" << QString().fill('=', 60);  // 分隔线
        
        // 清空临时缓冲区，准备接收下一帧
        pictmp.clear();
    }
}

/**
 * @brief TCP连接断开的槽函数
 * 
 * 当TCP连接断开时被调用，清理连接状态并启动自动重连（如果启用）
 */
void CTCPImg::slot_disconnect()
{
    m_brefresh = false;
    pictmp.clear();  // 清空接收缓冲区
    
    qDebug() << "❌ TCP连接已断开，清理连接状态";
    qDebug() << "🔄 [断开调试] 当前自动重连状态：" << (m_autoReconnectEnabled ? "启用" : "禁用");
    qDebug() << "🔄 [断开调试] 服务器地址：" << m_serverAddress;
    qDebug() << "🔄 [断开调试] 服务器端口：" << m_serverPort;
    qDebug() << "🔄 [断开调试] 当前重连尝试次数：" << m_reconnectAttempts;
    qDebug() << "🔄 [断开调试] 最大重连尝试次数：" << m_maxReconnectAttempts;
    
    // 安全关闭连接
    if (TCP_sendMesSocket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "🔄 [断开调试] 套接字状态不是未连接，执行close()";
    TCP_sendMesSocket->close();
    }
    
    // 触发重连逻辑
    triggerReconnectLogic("断开调试");
}

/**
 * @brief TCP套接字错误处理函数
 * @param error 套接字错误类型
 * 
 * 处理各种网络连接错误，提供详细的错误信息
 */
void CTCPImg::slot_socketError(QAbstractSocket::SocketError error)
{
    QString errorString;
    
    qDebug() << "❌ [错误调试] TCP套接字错误发生";
    qDebug() << "❌ [错误调试] 错误代码：" << error;
    qDebug() << "❌ [错误调试] 套接字状态：" << TCP_sendMesSocket->state();
    qDebug() << "❌ [错误调试] 尝试连接的服务器：" << m_serverAddress << ":" << m_serverPort;
    qDebug() << "❌ [错误调试] 当前重连尝试次数：" << m_reconnectAttempts;
    
    // 根据错误类型提供中文错误描述
    switch (error) {
        case QAbstractSocket::ConnectionRefusedError:
            errorString = "连接被拒绝：服务器未启动或端口被占用";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            errorString = "远程主机关闭了连接";
            break;
        case QAbstractSocket::HostNotFoundError:
            errorString = "找不到主机：请检查IP地址是否正确";
            break;
        case QAbstractSocket::SocketTimeoutError:
            errorString = "连接超时：网络可能不稳定";
            break;
        case QAbstractSocket::NetworkError:
            errorString = "网络错误：请检查网络连接";
            break;
        case QAbstractSocket::SocketResourceError:
            errorString = "套接字资源错误：系统资源不足";
            break;
        default:
            errorString = QString("未知网络错误（错误代码：%1）").arg(error);
            break;
    }
    
    qDebug() << "❌ [错误调试] TCP连接错误：" << errorString;
    qDebug() << "❌ [错误调试] 详细错误信息：" << TCP_sendMesSocket->errorString();
    
    // 重置连接状态
    m_brefresh = false;
    pictmp.clear();
    
    // 对于连接失败的错误，需要主动触发重连逻辑
    // 因为这些错误可能不会触发disconnected()信号
    bool shouldTriggerReconnect = false;
    switch (error) {
        case QAbstractSocket::ConnectionRefusedError:
        case QAbstractSocket::HostNotFoundError:
        case QAbstractSocket::SocketTimeoutError:
        case QAbstractSocket::NetworkError:
            shouldTriggerReconnect = true;
            qDebug() << "❌ [错误调试] 连接失败类型错误，需要主动触发重连";
            break;
        case QAbstractSocket::RemoteHostClosedError:
            // 这种情况通常会触发disconnected()信号，不需要主动重连
            qDebug() << "❌ [错误调试] 远程主机关闭连接，等待disconnect信号触发重连逻辑";
            break;
        default:
            qDebug() << "❌ [错误调试] 其他类型错误，等待disconnect信号触发重连逻辑";
            break;
    }
    
    // 如果需要主动触发重连
    if (shouldTriggerReconnect) {
        triggerReconnectLogic("错误调试");
    }
}

/**
 * @brief 设置图像分辨率参数
 * @param width 图像宽度
 * @param height 图像高度
 * @param channels 图像通道数
 * @return 成功返回true，失败返回false
 */
bool CTCPImg::setImageResolution(int width, int height, int channels)
{
    // 参数有效性检查
    if (width <= 0 || width > 8192) {
        qDebug() << "错误：图像宽度无效，有效范围：1-8192，当前值：" << width;
        return false;
    }
    
    if (height <= 0 || height > 8192) {
        qDebug() << "错误：图像高度无效，有效范围：1-8192，当前值：" << height;
        return false;
    }
    
    if (channels <= 0 || channels > 8) {
        qDebug() << "错误：图像通道数无效，有效范围：1-8，当前值：" << channels;
        return false;
    }
    
    // 检查内存大小限制（最大50MB）
    long long totalBytes = (long long)width * height * channels;
    if (totalBytes > 50 * 1024 * 1024) {
        qDebug() << "错误：图像数据太大，超过50MB限制：" << totalBytes << "字节";
        return false;
    }
    
    // 检查是否有连接正在进行
    if (TCP_sendMesSocket && TCP_sendMesSocket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "警告：检测到活动连接，建议先断开连接再修改分辨率";
    }
    
    // 更新图像参数
    m_imageWidth = width;
    m_imageHeight = height;
    m_imageChannels = channels;
    
    // 重新分配缓冲区
    if (!reallocateFrameBuffer()) {
        // 如果分配失败，恢复默认值
        m_imageWidth = WIDTH;
        m_imageHeight = HEIGHT;
        m_imageChannels = CHANLE;
        reallocateFrameBuffer();
        return false;
    }
    
    qDebug() << QString("图像分辨率已更新：%1x%2x%3，总大小：%4字节")
                .arg(m_imageWidth).arg(m_imageHeight).arg(m_imageChannels).arg(m_totalsize);
    
    return true;
}

/**
 * @brief 重新分配图像缓冲区
 * @return 成功返回true，失败返回false
 */
bool CTCPImg::reallocateFrameBuffer()
{
    try {
        // 释放旧的缓冲区
        if (frameBuffer != nullptr) {
            delete[] frameBuffer;
            frameBuffer = nullptr;
        }
        
        // 计算新的总大小
        m_totalsize = m_imageWidth * m_imageHeight * m_imageChannels;
        
        // 分配新的缓冲区
        frameBuffer = new char[m_totalsize];
        if (frameBuffer == nullptr) {
            qDebug() << "错误：内存分配失败";
            return false;
        }
        
        // 初始化缓冲区
        memset(frameBuffer, 0, m_totalsize);
        
        qDebug() << "图像缓冲区重新分配成功，大小：" << m_totalsize << "字节";
        return true;
        
    } catch (const std::bad_alloc& e) {
        qDebug() << "错误：内存分配异常：" << e.what();
        frameBuffer = nullptr;
        m_totalsize = 0;
        return false;
    } catch (...) {
        qDebug() << "错误：未知异常在内存分配过程中";
        frameBuffer = nullptr;
        m_totalsize = 0;
        return false;
    }
}

/**
 * @brief 格式化数据为十六进制字符串用于调试显示
 * @param data 原始数据
 * @param maxBytes 最大显示字节数
 * @return 格式化的十六进制字符串
 */
QString CTCPImg::formatDataForDebug(const QByteArray& data, int maxBytes)
{
    if (data.isEmpty()) {
        return "[空数据]";
    }
    
    int bytesToShow = qMin(data.size(), maxBytes);
    QStringList hexList;
    QStringList asciiList;
    
    for (int i = 0; i < bytesToShow; ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        
        // 添加十六进制表示
        hexList.append(QString("%1").arg(byte, 2, 16, QChar('0')).toUpper());
        
        // 添加ASCII字符（可打印字符）或点号
        if (byte >= 32 && byte <= 126) {
            asciiList.append(QString(QChar(byte)));
        } else {
            asciiList.append(".");
        }
    }
    
    QString result = QString("[%1] %2")
                    .arg(hexList.join(" "))
                    .arg(asciiList.join(""));
    
    if (data.size() > maxBytes) {
        result += QString(" ... (显示前%1/%2字节)").arg(bytesToShow).arg(data.size());
    }
    
    return result;
}

/**
 * @brief 验证帧头是否匹配预期的模式
 * @param data 要验证的数据
 * @param expectedHeader 预期的帧头字节序列
 * @return 如果帧头匹配返回true，否则返回false
 */
bool CTCPImg::validateFrameHeader(const QByteArray& data, const QByteArray& expectedHeader)
{
    if (data.size() < expectedHeader.size()) {
        qDebug() << "🔍 数据长度不足，无法验证帧头";
        return false;
    }
    
    // 比较前几个字节
    for (int i = 0; i < expectedHeader.size(); ++i) {
        if (data[i] != expectedHeader[i]) {
            qDebug() << QString("🔍 帧头验证失败：位置%1，期望0x%2，实际0x%3")
                        .arg(i)
                        .arg(static_cast<unsigned char>(expectedHeader[i]), 2, 16, QChar('0')).toUpper()
                        .arg(static_cast<unsigned char>(data[i]), 2, 16, QChar('0')).toUpper();
            return false;
        }
    }
    
    qDebug() << "🔍 帧头验证：所有字节匹配成功";
    return true;
}

/**
 * @brief 在数据中搜索帧头位置
 * @param data 要搜索的数据
 * @param header 要搜索的帧头
 * @return 帧头位置，-1表示未找到
 */
int CTCPImg::findFrameHeader(const QByteArray& data, const QByteArray& header)
{
    if (data.size() < header.size() || header.isEmpty()) {
        qDebug() << "🔍 搜索条件不满足：数据大小" << data.size() << "，帧头大小" << header.size();
        return -1;
    }
    
    // 在数据中搜索帧头模式
    for (int i = 0; i <= data.size() - header.size(); ++i) {
        bool found = true;
        for (int j = 0; j < header.size(); ++j) {
            if (data[i + j] != header[j]) {
                found = false;
                break;
            }
        }
        if (found) {
            qDebug() << QString("🔍 在位置 %1 找到帧头").arg(i);
            return i;  // 返回找到的位置
        }
    }
    
    qDebug() << "🔍 未找到帧头";
    return -1;  // 未找到
}

/**
 * @brief 自动重连槽函数
 * 在连接断开后尝试重新连接到服务器
 */
void CTCPImg::slot_reconnect()
{
    qDebug() << "🔄 [重连调试] slot_reconnect() 被调用";
    
    if (!m_autoReconnectEnabled) {
        qDebug() << "🔄 自动重连已禁用，停止重连尝试";
        return;
    }
    
    if (m_serverAddress.isEmpty() || m_serverPort <= 0) {
        qDebug() << "❌ 无效的服务器连接参数，无法重连";
        qDebug() << "   服务器地址：" << m_serverAddress;
        qDebug() << "   服务器端口：" << m_serverPort;
        return;
    }
    
    // 检查当前连接状态
    QAbstractSocket::SocketState currentState = TCP_sendMesSocket->state();
    qDebug() << "🔄 [重连调试] 当前套接字状态：" << currentState;
    
    if (currentState == QAbstractSocket::ConnectedState) {
        qDebug() << "✅ 连接已建立，取消重连";
        return;
    }
    
    qDebug() << QString("🔄 [重连调试] 开始第%1次重连尝试，连接到 %2:%3")
                .arg(m_reconnectAttempts)
                .arg(m_serverAddress)
                .arg(m_serverPort);
    
    // 确保套接字处于未连接状态
    if (currentState != QAbstractSocket::UnconnectedState) {
        qDebug() << "🔄 [重连调试] 套接字状态不是未连接，执行abort()";
        TCP_sendMesSocket->abort();
        qDebug() << "🔄 [重连调试] abort()后的套接字状态：" << TCP_sendMesSocket->state();
    }
    
    // 禁用代理
    TCP_sendMesSocket->setProxy(QNetworkProxy::NoProxy);
    
    qDebug() << "🔄 [重连调试] 正在调用 connectToHost()...";
    
    // 尝试重新连接
    TCP_sendMesSocket->connectToHost(QHostAddress(m_serverAddress), m_serverPort);
    
    qDebug() << "🔄 [重连调试] connectToHost() 调用完成";
    qDebug() << "🔄 [重连调试] 连接后的套接字状态：" << TCP_sendMesSocket->state();
}

/**
 * @brief 触发重连逻辑的内部函数
 * @param source 触发源（用于调试日志）
 * 
 * 统一的重连逻辑处理，避免代码重复
 */
void CTCPImg::triggerReconnectLogic(const QString& source)
{
    qDebug() << QString("🔄 [%1] 触发重连逻辑检查").arg(source);
    
    // 检查重连条件
    if (!m_autoReconnectEnabled) {
        qDebug() << QString("🔄 [%1] 自动重连已禁用").arg(source);
        return;
    }
    
    if (m_serverAddress.isEmpty() || m_serverPort <= 0) {
        qDebug() << QString("❌ [%1] 缺少有效的服务器连接参数，无法自动重连").arg(source);
        return;
    }
    
    if (m_reconnectAttempts >= m_maxReconnectAttempts) {
        qDebug() << QString("❌ [%1] 已达到最大重连次数 (%2次)，停止自动重连").arg(source).arg(m_maxReconnectAttempts);
        qDebug() << "🔍 开始执行服务端诊断检查...";
        
        // 执行服务端诊断
        performServerDiagnostics();
        return;
    }
    
    // 增加重连计数
    m_reconnectAttempts++;
    qDebug() << QString("🔄 [%1] 准备自动重连 (第%2/%3次尝试)，%4秒后开始...")
                .arg(source)
                .arg(m_reconnectAttempts)
                .arg(m_maxReconnectAttempts)
                .arg(m_reconnectInterval / 1000.0);
    
    // 检查并停止现有定时器
    if (m_reconnectTimer->isActive()) {
        qDebug() << QString("🔄 [%1] 重连定时器已经在运行，先停止").arg(source);
        m_reconnectTimer->stop();
    }
    
    qDebug() << QString("🔄 [%1] 启动重连定时器，间隔：%2ms").arg(source).arg(m_reconnectInterval);
    
    // 启动重连定时器
    m_reconnectTimer->start(m_reconnectInterval);
    
    qDebug() << QString("🔄 [%1] 重连定时器启动状态：%2").arg(source).arg(m_reconnectTimer->isActive() ? "成功" : "失败");
    qDebug() << QString("🔄 [%1] 定时器剩余时间：%2ms").arg(source).arg(m_reconnectTimer->remainingTime());
}

/**
 * @brief 执行服务端诊断检查
 * 当重连失败后，检查服务端状态和网络连通性
 */
void CTCPImg::performServerDiagnostics()
{
    QStringList diagnosticOutput;
    
    // 诊断标题
    diagnosticOutput << "🔍 ==================== 服务端诊断报告 ====================";
    diagnosticOutput << QString("🔍 连接目标：%1:%2").arg(m_serverAddress).arg(m_serverPort);
    diagnosticOutput << QString("🔍 重连尝试：%1/%2次").arg(m_reconnectAttempts).arg(m_maxReconnectAttempts);
    diagnosticOutput << QString("🔍 诊断时间：%1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
    diagnosticOutput << "";
    
    // 1. 网络连通性检查
    diagnosticOutput << "🔍 【步骤1】网络连通性检查";
    QString connectivityResult = checkNetworkConnectivity(m_serverAddress, m_serverPort);
    diagnosticOutput << QString("🔍 连通性结果：%1").arg(connectivityResult);
    diagnosticOutput << "";
    
    // 2. 服务端状态分析
    diagnosticOutput << "🔍 【步骤2】服务端状态分析";
    diagnosticOutput << "🔍 ✅ 请检查以下项目：";
    diagnosticOutput << QString("🔍    1. 服务端程序是否正在运行？");
    diagnosticOutput << QString("🔍    2. 服务端是否监听在端口%1？").arg(m_serverPort);
    diagnosticOutput << "🔍    3. 服务端是否有图像数据可发送？";
    diagnosticOutput << "🔍    4. 服务端网络配置是否正确？";
    diagnosticOutput << "";
    
    // 3. 采集端程序检查
    diagnosticOutput << "🔍 【步骤3】采集端程序检查";
    diagnosticOutput << "🔍 ✅ 请检查以下项目：";
    diagnosticOutput << "🔍    1. 图像采集设备是否正常连接？";
    diagnosticOutput << "🔍    2. 采集程序是否正常运行？";
    diagnosticOutput << "🔍    3. 采集程序是否有图像数据输出？";
    diagnosticOutput << "🔍    4. 采集程序网络发送是否正常？";
    diagnosticOutput << "";
    
    // 4. 网络环境检查
    diagnosticOutput << "🔍 【步骤4】网络环境检查";
    diagnosticOutput << "🔍 ✅ 请检查以下项目：";
    diagnosticOutput << "🔍    1. 客户端与服务端网络是否连通？";
    diagnosticOutput << QString("🔍    2. 防火墙是否阻止了端口%1？").arg(m_serverPort);
    diagnosticOutput << "🔍    3. 路由器/交换机配置是否正确？";
    diagnosticOutput << "🔍    4. 网络带宽是否足够传输图像数据？";
    diagnosticOutput << "";
    
    // 5. 生成完整诊断报告
    QString diagnosticReport = generateDiagnosticReport();
    diagnosticOutput << "🔍 【诊断总结】";
    diagnosticOutput << diagnosticReport;
    diagnosticOutput << "";
    
    // 6. 建议操作
    diagnosticOutput << "🔍 【建议操作】";
    diagnosticOutput << "🔍 💡 1. 手动重连：点击'立即重连'按钮重新尝试";
    diagnosticOutput << "🔍 💡 2. 检查服务端：确认服务端程序正在运行并监听端口";
    diagnosticOutput << "🔍 💡 3. 检查采集端：确认图像采集程序正常工作";
    diagnosticOutput << "🔍 💡 4. 网络测试：使用ping/telnet等工具测试网络连通性";
    diagnosticOutput << "🔍 💡 5. 重启服务：重启服务端和采集端程序";
    diagnosticOutput << "🔍 💡 6. 联系技术支持：如问题持续存在，请联系技术支持";
    diagnosticOutput << "";
    diagnosticOutput << "🔍 ========================================================";
    
    // 将诊断信息发送到界面显示
    QString fullDiagnosticText = diagnosticOutput.join("\n");
    emit signalDiagnosticInfo(fullDiagnosticText);
    
    // 同时输出到控制台（用于开发调试）
    for (const QString& line : diagnosticOutput) {
        qDebug() << line;
    }
}

/**
 * @brief 检查网络连通性
 * @param host 目标主机地址
 * @param port 目标端口
 * @return 连通性检查结果
 */
QString CTCPImg::checkNetworkConnectivity(const QString& host, int port)
{
    // 创建临时套接字进行连通性测试
    QTcpSocket testSocket;
    testSocket.setProxy(QNetworkProxy::NoProxy);
    
    // 设置较短的超时时间进行快速测试
    testSocket.connectToHost(QHostAddress(host), port);
    
    if (testSocket.waitForConnected(3000)) {  // 3秒超时
        testSocket.disconnectFromHost();
        return "✅ 网络连通正常，可以建立TCP连接";
    } else {
        QAbstractSocket::SocketError error = testSocket.error();
        QString errorString = testSocket.errorString();
        
        switch (error) {
            case QAbstractSocket::ConnectionRefusedError:
                return "❌ 连接被拒绝 - 服务端可能未启动或端口未监听";
            case QAbstractSocket::HostNotFoundError:
                return "❌ 主机未找到 - 请检查IP地址是否正确";
            case QAbstractSocket::SocketTimeoutError:
                return "❌ 连接超时 - 网络可能不通或服务端响应慢";
            case QAbstractSocket::NetworkError:
                return "❌ 网络错误 - 请检查网络连接";
            default:
                return QString("❌ 连接失败 - %1").arg(errorString);
        }
    }
}

/**
 * @brief 生成诊断报告
 * @return 详细的诊断报告字符串
 */
QString CTCPImg::generateDiagnosticReport()
{
    QStringList report;
    
    // 连接信息
    report << QString("📊 连接信息：%1:%2").arg(m_serverAddress).arg(m_serverPort);
    report << QString("📊 重连状态：已尝试%1次，均失败").arg(m_maxReconnectAttempts);
    report << QString("📊 自动重连：已禁用（达到最大尝试次数）");
    
    // 可能的问题分析
    report << "";
    report << "🔍 可能的问题原因：";
    report << "   • 服务端程序未运行或已崩溃";
    report << "   • 图像采集设备故障或断开";
    report << "   • 采集程序异常退出或挂起";
    report << "   • 网络连接中断或配置错误";
    report << "   • 防火墙阻止了网络连接";
    report << "   • 服务端资源不足或过载";
    
    // 解决建议
    report << "";
    report << "💡 解决建议：";
    report << "   1. 检查服务端：确认程序运行状态";
    report << "   2. 检查采集端：确认设备和程序正常";
    report << "   3. 测试网络：ping服务端IP地址";
    report << "   4. 检查端口：telnet服务端端口";
    report << "   5. 重启服务：重启相关程序和设备";
    
    return report.join("\n🔍 ");
}

/**
 * @brief 停止自动重连
 * 停止重连定时器，取消自动重连
 */
void CTCPImg::stopReconnect()
{
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
        qDebug() << "🛑 已停止自动重连";
    }
    m_reconnectAttempts = 0;  // 重置重连计数
}

/**
 * @brief 设置自动重连参数
 * @param enabled 是否启用自动重连
 * @param maxAttempts 最大重连尝试次数
 * @param interval 重连间隔时间（毫秒）
 */
void CTCPImg::setAutoReconnect(bool enabled, int maxAttempts, int interval)
{
    m_autoReconnectEnabled = enabled;
    m_maxReconnectAttempts = qMax(1, maxAttempts);  // 至少1次
    m_reconnectInterval = qMax(1000, interval);     // 至少1秒间隔
    
    qDebug() << QString("🔄 自动重连设置更新：%1，最大尝试次数：%2，间隔：%3ms")
                .arg(enabled ? "启用" : "禁用")
                .arg(m_maxReconnectAttempts)
                .arg(m_reconnectInterval);
    
    if (!enabled && m_reconnectTimer->isActive()) {
        stopReconnect();
    }
}

/**
 * @brief 获取当前连接状态
 * @return 连接状态
 */
QAbstractSocket::SocketState CTCPImg::getConnectionState() const
{
    return TCP_sendMesSocket ? TCP_sendMesSocket->state() : QAbstractSocket::UnconnectedState;
}

/**
 * @brief 手动触发重连
 * 立即尝试重新连接到服务器
 */
void CTCPImg::reconnectNow()
{
    if (m_serverAddress.isEmpty() || m_serverPort <= 0) {
        qDebug() << "❌ 无效的服务器连接参数，无法重连";
        return;
    }
    
    // 停止当前的重连定时器
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
    
    // 重置重连计数
    m_reconnectAttempts = 0;
    
    qDebug() << "🔄 手动触发重连";
    
    // 立即尝试重连
    slot_reconnect();
}


