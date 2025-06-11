#ifndef TCPDEBUGGER_H
#define TCPDEBUGGER_H

#include <QObject>
#include <QTcpSocket>
#include <QTcpServer>
#include <QTimer>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QNetworkProxy>
#include "dataformatter.h"

/**
 * @class CTCPDebugger
 * @brief TCP网络调试器类
 * 
 * 提供通用的TCP网络调试功能，支持：
 * - TCP客户端连接模式
 * - TCP服务器监听模式
 * - 多种数据格式显示
 * - 连接状态监控
 * - 数据发送功能
 * - 连接统计信息
 */
class CTCPDebugger : public QObject
{
    Q_OBJECT

public:
    /**
     * @enum WorkMode
     * @brief 工作模式枚举
     */
    enum WorkMode {
        MODE_CLIENT,         ///< 客户端模式
        MODE_SERVER          ///< 服务器模式
    };

    /**
     * @enum ConnectionState
     * @brief 连接状态枚举
     */
    enum ConnectionState {
        STATE_DISCONNECTED,  ///< 未连接
        STATE_CONNECTING,    ///< 连接中
        STATE_CONNECTED,     ///< 已连接
        STATE_LISTENING,     ///< 监听中
        STATE_ERROR          ///< 错误状态
    };

    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit CTCPDebugger(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~CTCPDebugger();

    /**
     * @brief 设置工作模式
     * @param mode 工作模式
     */
    void setWorkMode(WorkMode mode);

    /**
     * @brief 获取当前工作模式
     * @return 当前工作模式
     */
    WorkMode getWorkMode() const;

    /**
     * @brief 启动客户端连接
     * @param host 目标主机地址
     * @param port 目标端口
     */
    void startClient(const QString& host, quint16 port);

    /**
     * @brief 启动服务器监听
     * @param port 监听端口
     * @param bindAddress 绑定地址，默认为任意地址
     */
    void startServer(quint16 port, const QHostAddress& bindAddress = QHostAddress::Any);

    /**
     * @brief 停止连接或监听
     */
    void stop();

    /**
     * @brief 发送数据
     * @param data 要发送的数据
     * @return 成功发送的字节数，-1表示失败
     */
    qint64 sendData(const QByteArray& data);

    /**
     * @brief 发送文本数据
     * @param text 要发送的文本
     * @return 成功发送的字节数，-1表示失败
     */
    qint64 sendText(const QString& text);

    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    ConnectionState getConnectionState() const;

    /**
     * @brief 获取连接统计信息
     * @return 统计信息字符串
     */
    QString getConnectionStats() const;

    /**
     * @brief 获取本地IP地址列表
     * @return IP地址列表
     */
    static QStringList getLocalIPAddresses();

    /**
     * @brief 设置数据显示格式
     * @param format 显示格式
     */
    void setDataDisplayFormat(CDataFormatter::DataDisplayFormat format);

    /**
     * @brief 获取当前数据显示格式
     * @return 当前显示格式
     */
    CDataFormatter::DataDisplayFormat getDataDisplayFormat() const;

    /**
     * @brief 设置是否显示时间戳
     * @param show 是否显示时间戳
     */
    void setShowTimestamp(bool show);

    /**
     * @brief 获取是否显示时间戳
     * @return 是否显示时间戳
     */
    bool getShowTimestamp() const;

signals:
    /**
     * @brief 数据接收信号
     * @param data 接收到的原始数据
     * @param formattedData 格式化后的数据
     * @param remoteAddress 远程地址信息
     */
    void dataReceived(const QByteArray& data, const QString& formattedData, const QString& remoteAddress);

    /**
     * @brief 连接状态变化信号
     * @param state 新的连接状态
     * @param message 状态描述信息
     */
    void connectionStateChanged(ConnectionState state, const QString& message);

    /**
     * @brief 错误信号
     * @param error 错误类型
     * @param message 错误信息
     */
    void errorOccurred(QAbstractSocket::SocketError error, const QString& message);

    /**
     * @brief 新客户端连接信号（服务器模式）
     * @param clientAddress 客户端地址
     */
    void newClientConnected(const QString& clientAddress);

    /**
     * @brief 客户端断开连接信号（服务器模式）
     * @param clientAddress 客户端地址
     */
    void clientDisconnected(const QString& clientAddress);

public slots:
    /**
     * @brief 清空统计信息
     */
    void clearStats();

private slots:
    /**
     * @brief 客户端连接成功槽函数
     */
    void onClientConnected();

    /**
     * @brief 客户端断开连接槽函数
     */
    void onClientDisconnected();

    /**
     * @brief 数据接收槽函数
     */
    void onDataReceived();

    /**
     * @brief 套接字错误槽函数
     * @param error 错误类型
     */
    void onSocketError(QAbstractSocket::SocketError error);

    /**
     * @brief 新连接到达槽函数（服务器模式）
     */
    void onNewConnection();

    /**
     * @brief 统计更新定时器槽函数
     */
    void updateStats();

private:
    WorkMode m_workMode;                    ///< 当前工作模式
    ConnectionState m_connectionState;      ///< 当前连接状态
    
    QTcpSocket* m_clientSocket;             ///< 客户端套接字
    QTcpServer* m_server;                   ///< 服务器对象
    QList<QTcpSocket*> m_clientSockets;     ///< 已连接的客户端套接字列表（服务器模式）
    
    CDataFormatter* m_dataFormatter;       ///< 数据格式化器
    CDataFormatter::DataDisplayFormat m_displayFormat;  ///< 当前显示格式
    bool m_showTimestamp;                   ///< 是否显示时间戳
    
    QString m_currentHost;                  ///< 当前连接的主机地址
    quint16 m_currentPort;                  ///< 当前连接的端口
    
    // 统计信息
    qint64 m_totalBytesReceived;           ///< 总接收字节数
    qint64 m_totalBytesSent;               ///< 总发送字节数
    qint64 m_totalPacketsReceived;         ///< 总接收包数
    qint64 m_totalPacketsSent;             ///< 总发送包数
    QDateTime m_connectionStartTime;        ///< 连接开始时间
    QTimer* m_statsTimer;                   ///< 统计更新定时器

    /**
     * @brief 初始化组件
     */
    void initializeComponents();

    /**
     * @brief 清理连接
     */
    void cleanupConnections();

    /**
     * @brief 设置连接状态
     * @param state 新状态
     * @param message 状态信息
     */
    void setConnectionState(ConnectionState state, const QString& message = QString());

    /**
     * @brief 处理接收到的数据
     * @param socket 来源套接字
     * @param data 原始数据
     */
    void processReceivedData(QTcpSocket* socket, const QByteArray& data);

    /**
     * @brief 获取套接字的远程地址信息
     * @param socket 套接字
     * @return 地址信息字符串
     */
    QString getRemoteAddressInfo(QTcpSocket* socket);

    /**
     * @brief 连接状态转换为字符串
     * @param state 连接状态
     * @return 状态字符串
     */
    QString connectionStateToString(ConnectionState state) const;
};

#endif // TCPDEBUGGER_H 