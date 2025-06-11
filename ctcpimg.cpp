#include "ctcpimg.h"
#include <QDebug>

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
    
    // 计算图像数据总大小：宽度 × 高度 × 通道数
    m_totalsize = WIDTH * HEIGHT * CHANLE;
    
    // 为图像帧缓冲区分配内存并初始化为0
    frameBuffer = new char[WIDTH * HEIGHT * CHANLE];
    memset((void*)frameBuffer, 0, WIDTH * HEIGHT * CHANLE);

    // 初始化TCP套接字
    TCP_sendMesSocket = NULL;
    this->TCP_sendMesSocket = new QTcpSocket();
    TCP_sendMesSocket->abort();  // 中止任何现有连接

    // 连接信号槽，处理TCP连接的各种状态
    connect(TCP_sendMesSocket, SIGNAL(connected()), this, SLOT(slot_connected()));
    connect(TCP_sendMesSocket, SIGNAL(readyRead()), this, SLOT(slot_recvmessage()));
    connect(TCP_sendMesSocket, SIGNAL(disconnected()), this, SLOT(slot_disconnect()));
    // 添加错误处理信号连接
    connect(TCP_sendMesSocket, SIGNAL(error(QAbstractSocket::SocketError)), 
            this, SLOT(slot_socketError(QAbstractSocket::SocketError)));
    
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
        qDebug() << "接收到数据大小信息，数据包大小：" << byteArray.size() << "字节";
        
        // 提取并解析数据大小
        byteArray = byteArray.replace("size=", "");
        bool conversionOk = false;
        int receivedSize = byteArray.toInt(&conversionOk);
        
        // 数据大小有效性检查
        if (!conversionOk) {
            qDebug() << "错误：无法解析数据大小信息";
            return;
        }
        
        if (receivedSize <= 0 || receivedSize > 10 * 1024 * 1024) {  // 限制最大10MB
            qDebug() << "错误：数据大小异常：" << receivedSize << "字节";
            return;
        }
        
        m_totalsize = receivedSize;
        pictmp.clear();  // 清空之前的数据
        
        qDebug() << "预期接收图像数据大小：" << m_totalsize << "字节";
    }
    // 第二阶段：累积图像数据
    else
    {
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
        // 数据完整性的最终检查
        if (m_totalsize > WIDTH * HEIGHT * CHANLE) {
            qDebug() << "警告：接收数据大小超出图像缓冲区容量";
            // 只复制缓冲区容量范围内的数据
            memcpy(frameBuffer, pictmp.data(), WIDTH * HEIGHT * CHANLE);
        } else {
            // 将接收到的图像数据复制到帧缓冲区
            memcpy(frameBuffer, pictmp.data(), m_totalsize);
        }
        
        // 发射图像数据就绪信号，通知界面更新
        emit tcpImgReadySig();
        
        // 向服务器发送确认消息
        if (TCP_sendMesSocket->state() == QAbstractSocket::ConnectedState) {
            TCP_sendMesSocket->write("OK");
            TCP_sendMesSocket->flush();  // 确保数据立即发送
        }
        
        qDebug() << "图像数据接收完成并已更新缓冲区，发送确认消息";
        
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
