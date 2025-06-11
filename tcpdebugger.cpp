#include "tcpdebugger.h"
#include <QDebug>
#include <QApplication>

/**
 * @brief 构造函数
 * @param parent 父对象指针
 */
CTCPDebugger::CTCPDebugger(QObject *parent)
    : QObject(parent)
    , m_workMode(MODE_CLIENT)
    , m_connectionState(STATE_DISCONNECTED)
    , m_clientSocket(nullptr)
    , m_server(nullptr)
    , m_dataFormatter(nullptr)
    , m_displayFormat(CDataFormatter::FORMAT_RAW_TEXT)
    , m_showTimestamp(true)
    , m_currentPort(0)
    , m_totalBytesReceived(0)
    , m_totalBytesSent(0)
    , m_totalPacketsReceived(0)
    , m_totalPacketsSent(0)
    , m_statsTimer(nullptr)
{
    initializeComponents();
    qDebug() << "TCP网络调试器初始化完成";
}

/**
 * @brief 析构函数
 */
CTCPDebugger::~CTCPDebugger()
{
    stop();
    cleanupConnections();
    
    if (m_dataFormatter) {
        delete m_dataFormatter;
        m_dataFormatter = nullptr;
    }
    
    if (m_statsTimer) {
        delete m_statsTimer;
        m_statsTimer = nullptr;
    }
    
    qDebug() << "TCP网络调试器销毁完成";
}

/**
 * @brief 初始化组件
 */
void CTCPDebugger::initializeComponents()
{
    // 设置应用程序默认网络代理为无代理，避免系统代理设置干扰
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    
    // 创建数据格式化器
    m_dataFormatter = new CDataFormatter();
    
    // 创建统计定时器
    m_statsTimer = new QTimer(this);
    m_statsTimer->setInterval(1000);  // 每秒更新一次统计
    connect(m_statsTimer, &QTimer::timeout, this, &CTCPDebugger::updateStats);
    
    qDebug() << "TCP调试器组件初始化完成，已禁用网络代理";
}

/**
 * @brief 设置工作模式
 * @param mode 工作模式
 */
void CTCPDebugger::setWorkMode(WorkMode mode)
{
    if (m_connectionState != STATE_DISCONNECTED) {
        qDebug() << "警告：连接状态下无法切换工作模式";
        return;
    }
    
    m_workMode = mode;
    qDebug() << "工作模式设置为：" << (mode == MODE_CLIENT ? "客户端" : "服务器");
}

/**
 * @brief 获取当前工作模式
 * @return 当前工作模式
 */
CTCPDebugger::WorkMode CTCPDebugger::getWorkMode() const
{
    return m_workMode;
}

/**
 * @brief 启动客户端连接
 * @param host 目标主机地址
 * @param port 目标端口
 */
void CTCPDebugger::startClient(const QString& host, quint16 port)
{
    if (m_workMode != MODE_CLIENT) {
        qDebug() << "错误：当前不是客户端模式";
        return;
    }
    
    if (m_connectionState != STATE_DISCONNECTED) {
        qDebug() << "警告：已有活动连接，先停止当前连接";
        stop();
    }
    
    // 创建客户端套接字
    m_clientSocket = new QTcpSocket(this);
    
    // 禁用代理，避免代理设置导致的连接问题
    m_clientSocket->setProxy(QNetworkProxy::NoProxy);
    
    // 连接信号槽
    connect(m_clientSocket, &QTcpSocket::connected, this, &CTCPDebugger::onClientConnected);
    connect(m_clientSocket, &QTcpSocket::disconnected, this, &CTCPDebugger::onClientDisconnected);
    connect(m_clientSocket, &QTcpSocket::readyRead, this, &CTCPDebugger::onDataReceived);
    connect(m_clientSocket, &QAbstractSocket::errorOccurred,
            this, &CTCPDebugger::onSocketError);
    
    m_currentHost = host;
    m_currentPort = port;
    
    setConnectionState(STATE_CONNECTING, QString("正在连接到 %1:%2").arg(host).arg(port));
    
    qDebug() << "开始连接到" << host << ":" << port;
    m_clientSocket->connectToHost(host, port);
}

/**
 * @brief 启动服务器监听
 * @param port 监听端口
 * @param bindAddress 绑定地址
 */
void CTCPDebugger::startServer(quint16 port, const QHostAddress& bindAddress)
{
    if (m_workMode != MODE_SERVER) {
        qDebug() << "错误：当前不是服务器模式";
        return;
    }
    
    if (m_connectionState != STATE_DISCONNECTED) {
        qDebug() << "警告：已有活动连接，先停止当前连接";
        stop();
    }
    
    // 创建服务器
    m_server = new QTcpServer(this);
    
    // 连接新连接信号
    connect(m_server, &QTcpServer::newConnection, this, &CTCPDebugger::onNewConnection);
    
    // 开始监听
    if (m_server->listen(bindAddress, port)) {
        m_currentPort = port;
        setConnectionState(STATE_LISTENING, QString("服务器监听在 %1:%2").arg(bindAddress.toString()).arg(port));
        
        qDebug() << "服务器开始监听" << bindAddress.toString() << ":" << port;
        m_statsTimer->start();
    } else {
        setConnectionState(STATE_ERROR, QString("服务器启动失败：%1").arg(m_server->errorString()));
        qDebug() << "服务器启动失败：" << m_server->errorString();
    }
}

/**
 * @brief 停止连接或监听
 */
void CTCPDebugger::stop()
{
    if (m_statsTimer && m_statsTimer->isActive()) {
        m_statsTimer->stop();
    }
    
    if (m_workMode == MODE_CLIENT && m_clientSocket) {
        m_clientSocket->disconnectFromHost();
        if (m_clientSocket->state() != QAbstractSocket::UnconnectedState) {
            m_clientSocket->waitForDisconnected(3000);
        }
    }
    
    if (m_workMode == MODE_SERVER && m_server) {
        // 断开所有客户端连接
        for (QTcpSocket* socket : m_clientSockets) {
            socket->disconnectFromHost();
            socket->deleteLater();
        }
        m_clientSockets.clear();
        
        m_server->close();
    }
    
    cleanupConnections();
    setConnectionState(STATE_DISCONNECTED, "连接已停止");
    
    qDebug() << "TCP调试器已停止";
}

/**
 * @brief 发送数据
 * @param data 要发送的数据
 * @return 成功发送的字节数，-1表示失败
 */
qint64 CTCPDebugger::sendData(const QByteArray& data)
{
    qint64 totalSent = 0;
    
    if (m_workMode == MODE_CLIENT && m_clientSocket && m_clientSocket->state() == QAbstractSocket::ConnectedState) {
        qint64 sent = m_clientSocket->write(data);
        m_clientSocket->flush();
        if (sent > 0) {
            totalSent = sent;
            m_totalBytesSent += sent;
            m_totalPacketsSent++;
        }
    } else if (m_workMode == MODE_SERVER) {
        // 服务器模式：向所有连接的客户端发送数据
        for (QTcpSocket* socket : m_clientSockets) {
            if (socket->state() == QAbstractSocket::ConnectedState) {
                qint64 sent = socket->write(data);
                socket->flush();
                if (sent > 0) {
                    totalSent += sent;
                    m_totalBytesSent += sent;
                }
            }
        }
        if (totalSent > 0) {
            m_totalPacketsSent++;
        }
    }
    
    if (totalSent > 0) {
        qDebug() << "发送数据：" << totalSent << "字节";
    } else {
        qDebug() << "发送数据失败：连接状态异常";
    }
    
    return totalSent > 0 ? totalSent : -1;
}

/**
 * @brief 发送文本数据
 * @param text 要发送的文本
 * @return 成功发送的字节数，-1表示失败
 */
qint64 CTCPDebugger::sendText(const QString& text)
{
    return sendData(text.toUtf8());
}

/**
 * @brief 获取当前连接状态
 * @return 连接状态
 */
CTCPDebugger::ConnectionState CTCPDebugger::getConnectionState() const
{
    return m_connectionState;
}

/**
 * @brief 获取连接统计信息
 * @return 统计信息字符串
 */
QString CTCPDebugger::getConnectionStats() const
{
    QString stats;
    stats += QString("工作模式：%1\n").arg(m_workMode == MODE_CLIENT ? "客户端" : "服务器");
    stats += QString("连接状态：%1\n").arg(connectionStateToString(m_connectionState));
    
    if (m_workMode == MODE_CLIENT) {
        stats += QString("目标地址：%1:%2\n").arg(m_currentHost).arg(m_currentPort);
    } else {
        stats += QString("监听端口：%1\n").arg(m_currentPort);
        stats += QString("已连接客户端：%1\n").arg(m_clientSockets.size());
    }
    
    stats += QString("总接收字节：%1\n").arg(m_totalBytesReceived);
    stats += QString("总发送字节：%1\n").arg(m_totalBytesSent);
    stats += QString("总接收包数：%1\n").arg(m_totalPacketsReceived);
    stats += QString("总发送包数：%1\n").arg(m_totalPacketsSent);
    
    if (!m_connectionStartTime.isNull()) {
        qint64 duration = m_connectionStartTime.secsTo(QDateTime::currentDateTime());
        stats += QString("连接时长：%1秒\n").arg(duration);
        
        if (duration > 0) {
            double rxRate = m_totalBytesReceived / static_cast<double>(duration);
            double txRate = m_totalBytesSent / static_cast<double>(duration);
            stats += QString("平均接收速率：%.2f 字节/秒\n").arg(rxRate);
            stats += QString("平均发送速率：%.2f 字节/秒\n").arg(txRate);
        }
    }
    
    return stats;
}

/**
 * @brief 获取本地IP地址列表
 * @return IP地址列表，优先显示最可能的活动网络接口
 */
QStringList CTCPDebugger::getLocalIPAddresses()
{
    QStringList addresses;
    QStringList preferredAddresses;  // 优先级高的地址
    QStringList normalAddresses;     // 普通地址
    
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    for (const QNetworkInterface &interface : interfaces) {
        // 只考虑活动的、运行中的、非回环接口
        if (interface.flags() & QNetworkInterface::IsUp &&
            interface.flags() & QNetworkInterface::IsRunning &&
            !(interface.flags() & QNetworkInterface::IsLoopBack)) {
            
            qDebug() << "检测到网络接口：" << interface.name() << interface.humanReadableName();
            
            const QList<QNetworkAddressEntry> entries = interface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    QString ipAddress = entry.ip().toString();
                    
                    // 排除链路本地地址和APIPA地址
                    if (!ipAddress.startsWith("169.254.")) {
                        // 根据接口类型和IP范围判断优先级
                        QString interfaceName = interface.name().toLower();
                        
                        // 优先级判断：以太网 > WiFi > 其他
                        bool isPreferred = false;
                        if (interfaceName.contains("ethernet") || 
                            interfaceName.contains("eth") ||
                            interfaceName.contains("lan") ||
                            interfaceName.contains("local")) {
                            isPreferred = true;
                        } else if (interfaceName.contains("wifi") || 
                                 interfaceName.contains("wlan") ||
                                 interfaceName.contains("wireless")) {
                            isPreferred = true;
                        }
                        
                        // 常见内网IP段也优先显示
                        if (ipAddress.startsWith("192.168.") || 
                            ipAddress.startsWith("10.") ||
                            ipAddress.startsWith("172.")) {
                            isPreferred = true;
                        }
                        
                        if (isPreferred) {
                            preferredAddresses << QString("%1 (%2)").arg(ipAddress, interface.humanReadableName());
                        } else {
                            normalAddresses << QString("%1 (%2)").arg(ipAddress, interface.humanReadableName());
                        }
                        
                        qDebug() << "发现IP地址：" << ipAddress << "接口：" << interface.humanReadableName();
                    }
                }
            }
        }
    }
    
    // 按优先级组合地址列表
    addresses << preferredAddresses;
    addresses << normalAddresses;
    
    // 添加特殊地址选项
    addresses << "127.0.0.1 (本地回环)";
    addresses << "0.0.0.0 (所有接口)";
    
    // 去除重复项，但保留描述信息
    addresses.removeDuplicates();
    
    // 如果没有找到任何网络接口，提供默认选项
    if (addresses.isEmpty()) {
        addresses << "127.0.0.1 (本地回环)";
        addresses << "0.0.0.0 (所有接口)";
        qDebug() << "警告：未检测到可用的网络接口，使用默认配置";
    }
    
    qDebug() << "本地IP地址列表：" << addresses;
    return addresses;
}

/**
 * @brief 设置数据显示格式
 * @param format 显示格式
 */
void CTCPDebugger::setDataDisplayFormat(CDataFormatter::DataDisplayFormat format)
{
    m_displayFormat = format;
    qDebug() << "数据显示格式设置为：" << m_dataFormatter->formatToString(format);
}

/**
 * @brief 获取当前数据显示格式
 * @return 当前显示格式
 */
CDataFormatter::DataDisplayFormat CTCPDebugger::getDataDisplayFormat() const
{
    return m_displayFormat;
}

/**
 * @brief 设置是否显示时间戳
 * @param show 是否显示时间戳
 */
void CTCPDebugger::setShowTimestamp(bool show)
{
    m_showTimestamp = show;
    qDebug() << "时间戳显示：" << (show ? "开启" : "关闭");
}

/**
 * @brief 获取是否显示时间戳
 * @return 是否显示时间戳
 */
bool CTCPDebugger::getShowTimestamp() const
{
    return m_showTimestamp;
}

/**
 * @brief 清空统计信息
 */
void CTCPDebugger::clearStats()
{
    m_totalBytesReceived = 0;
    m_totalBytesSent = 0;
    m_totalPacketsReceived = 0;
    m_totalPacketsSent = 0;
    m_connectionStartTime = QDateTime::currentDateTime();
    
    qDebug() << "统计信息已清空";
}

/**
 * @brief 客户端连接成功槽函数
 */
void CTCPDebugger::onClientConnected()
{
    setConnectionState(STATE_CONNECTED, QString("已连接到 %1:%2").arg(m_currentHost).arg(m_currentPort));
    m_connectionStartTime = QDateTime::currentDateTime();
    m_statsTimer->start();
    
    qDebug() << "客户端连接成功";
}

/**
 * @brief 客户端断开连接槽函数
 */
void CTCPDebugger::onClientDisconnected()
{
    setConnectionState(STATE_DISCONNECTED, "连接已断开");
    
    if (m_statsTimer && m_statsTimer->isActive()) {
        m_statsTimer->stop();
    }
    
    qDebug() << "客户端连接断开";
}

/**
 * @brief 数据接收槽函数
 */
void CTCPDebugger::onDataReceived()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }
    
    QByteArray data = socket->readAll();
    if (!data.isEmpty()) {
        processReceivedData(socket, data);
    }
}

/**
 * @brief 套接字错误槽函数
 * @param error 错误类型
 */
void CTCPDebugger::onSocketError(QAbstractSocket::SocketError error)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    QString errorMessage = socket ? socket->errorString() : "未知错误";
    
    // 针对常见错误提供中文解释和解决建议
    QString friendlyMessage = errorMessage;
    QString suggestion = "";
    
    switch (error) {
        case QAbstractSocket::ConnectionRefusedError:
            friendlyMessage = "连接被拒绝";
            suggestion = "请检查目标主机是否在线，端口是否正确，防火墙设置等";
            break;
            
        case QAbstractSocket::RemoteHostClosedError:
            friendlyMessage = "远程主机关闭了连接";
            suggestion = "对方主动断开了连接";
            break;
            
        case QAbstractSocket::HostNotFoundError:
            friendlyMessage = "找不到主机";
            suggestion = "请检查IP地址或主机名是否正确，网络连接是否正常";
            break;
            
        case QAbstractSocket::SocketAccessError:
            friendlyMessage = "套接字访问错误";
            suggestion = "可能是权限问题或端口被占用";
            break;
            
        case QAbstractSocket::SocketResourceError:
            friendlyMessage = "套接字资源错误";
            suggestion = "系统资源不足或达到连接数限制";
            break;
            
        case QAbstractSocket::SocketTimeoutError:
            friendlyMessage = "连接超时";
            suggestion = "网络延迟过高或目标主机无响应";
            break;
            
        case QAbstractSocket::NetworkError:
            if (errorMessage.contains("proxy")) {
                friendlyMessage = "网络代理错误";
                suggestion = "程序已尝试禁用代理，请检查系统网络设置";
            } else {
                friendlyMessage = "网络错误";
                suggestion = "请检查网络连接是否正常";
            }
            break;
            
        case QAbstractSocket::UnsupportedSocketOperationError:
            friendlyMessage = "不支持的套接字操作";
            suggestion = "当前网络配置不支持此操作";
            break;
            
        default:
            // 特殊处理代理相关错误
            if (errorMessage.contains("proxy")) {
                friendlyMessage = "网络代理配置错误";
                suggestion = "程序已禁用代理，如仍有问题请检查系统网络设置";
            }
            break;
    }
    
    QString fullMessage = QString("%1：%2").arg(friendlyMessage, errorMessage);
    if (!suggestion.isEmpty()) {
        fullMessage += QString("\n建议：%1").arg(suggestion);
    }
    
    setConnectionState(STATE_ERROR, fullMessage);
    
    emit errorOccurred(error, fullMessage);
    qDebug() << "套接字错误：" << errorMessage << "类型：" << error;
}

/**
 * @brief 新连接到达槽函数（服务器模式）
 */
void CTCPDebugger::onNewConnection()
{
    if (!m_server) {
        return;
    }
    
    while (m_server->hasPendingConnections()) {
        QTcpSocket* clientSocket = m_server->nextPendingConnection();
        
        // 禁用代理，避免代理设置导致的连接问题
        clientSocket->setProxy(QNetworkProxy::NoProxy);
        
        m_clientSockets.append(clientSocket);
        
        // 连接客户端信号槽
        connect(clientSocket, &QTcpSocket::disconnected, this, [this, clientSocket]() {
            QString clientAddr = getRemoteAddressInfo(clientSocket);
            m_clientSockets.removeAll(clientSocket);
            clientSocket->deleteLater();
            
            emit clientDisconnected(clientAddr);
            qDebug() << "客户端断开连接：" << clientAddr;
        });
        
        connect(clientSocket, &QTcpSocket::readyRead, this, &CTCPDebugger::onDataReceived);
        connect(clientSocket, &QAbstractSocket::errorOccurred,
                this, &CTCPDebugger::onSocketError);
        
        QString clientAddr = getRemoteAddressInfo(clientSocket);
        emit newClientConnected(clientAddr);
        
        qDebug() << "新客户端连接：" << clientAddr;
        
        if (m_connectionStartTime.isNull()) {
            m_connectionStartTime = QDateTime::currentDateTime();
        }
    }
}

/**
 * @brief 统计更新定时器槽函数
 */
void CTCPDebugger::updateStats()
{
    // 这里可以添加定期统计更新逻辑
    // 当前版本主要用于保持统计时间的准确性
}

/**
 * @brief 清理连接
 */
void CTCPDebugger::cleanupConnections()
{
    if (m_clientSocket) {
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    
    if (m_server) {
        m_server->deleteLater();
        m_server = nullptr;
    }
    
    for (QTcpSocket* socket : m_clientSockets) {
        socket->deleteLater();
    }
    m_clientSockets.clear();
}

/**
 * @brief 设置连接状态
 * @param state 新状态
 * @param message 状态信息
 */
void CTCPDebugger::setConnectionState(ConnectionState state, const QString& message)
{
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state, message);
        qDebug() << "连接状态变更：" << connectionStateToString(state) << "-" << message;
    }
}

/**
 * @brief 处理接收到的数据
 * @param socket 来源套接字
 * @param data 原始数据
 */
void CTCPDebugger::processReceivedData(QTcpSocket* socket, const QByteArray& data)
{
    m_totalBytesReceived += data.size();
    m_totalPacketsReceived++;
    
    // 格式化数据
    QString formattedData = m_dataFormatter->formatData(data, m_displayFormat, m_showTimestamp);
    
    // 获取远程地址信息
    QString remoteAddress = getRemoteAddressInfo(socket);
    
    // 发射数据接收信号
    emit dataReceived(data, formattedData, remoteAddress);
    
    qDebug() << QString("接收数据 [%1]: %2 字节").arg(remoteAddress).arg(data.size());
}

/**
 * @brief 获取套接字的远程地址信息
 * @param socket 套接字
 * @return 地址信息字符串
 */
QString CTCPDebugger::getRemoteAddressInfo(QTcpSocket* socket)
{
    if (!socket) {
        return "Unknown";
    }
    
    return QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
}

/**
 * @brief 连接状态转换为字符串
 * @param state 连接状态
 * @return 状态字符串
 */
QString CTCPDebugger::connectionStateToString(ConnectionState state) const
{
    switch (state) {
        case STATE_DISCONNECTED: return "未连接";
        case STATE_CONNECTING: return "连接中";
        case STATE_CONNECTED: return "已连接";
        case STATE_LISTENING: return "监听中";
        case STATE_ERROR: return "错误状态";
        default: return "未知状态";
    }
} 