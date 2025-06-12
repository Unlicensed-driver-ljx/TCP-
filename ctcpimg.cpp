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
    
    qDebug() << "CTCPImg对象初始化完成，图像缓冲区大小：" << m_totalsize << "字节";
}

/**
 * @brief CTCPImg析构函数
 * 
 * 清理资源，释放内存和关闭网络连接
 */
CTCPImg::~CTCPImg(void)
{
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
    
    // 如果已经连接，先断开
    if (TCP_sendMesSocket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "检测到现有连接，正在断开...";
        TCP_sendMesSocket->disconnectFromHost();
    }
    
    // 禁用代理，避免系统代理设置干扰图像传输
    TCP_sendMesSocket->setProxy(QNetworkProxy::NoProxy);
    
    qDebug() << "开始连接到服务器：" << strAddr << ":" << port;
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
    qDebug() << "TCP连接建立成功，准备接收图像数据";
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
 * 处理TCP连接断开事件，清理相关状态
 */
void CTCPImg::slot_disconnect()
{
    m_brefresh = false;
    pictmp.clear();  // 清空接收缓冲区
    
    qDebug() << "TCP连接已断开，清理连接状态";
    
    // 安全关闭连接
    if (TCP_sendMesSocket->state() != QAbstractSocket::UnconnectedState) {
        TCP_sendMesSocket->close();
    }
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
    
    qDebug() << "TCP连接错误：" << errorString;
    qDebug() << "详细错误信息：" << TCP_sendMesSocket->errorString();
    
    // 重置连接状态
    m_brefresh = false;
    pictmp.clear();
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


