#ifndef CTCPIMG_H
#define CTCPIMG_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTcpSocket>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QTimer>
#include <QDateTime>
#include <QImage>
#include "sysdefine.h"

/**
 * @class CTCPImg
 * @brief TCP图像传输处理类
 * 
 * 负责建立TCP连接，接收图像数据，并管理图像缓冲区
 * 支持实时图像数据传输和显示更新
 */
class CTCPImg: public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针，用于Qt对象树管理
     */
    CTCPImg(QObject *parent = NULL);
   // CTCPImg();
    /**
     * @brief 析构函数
     * 清理网络连接和释放内存资源
     */
    ~CTCPImg();
    /**
     * @brief 获取图像帧缓冲区指针
     * @return 返回图像数据缓冲区的char指针
     */
    char* getFrameBuffer();
    
    /**
     * @brief 设置图像分辨率参数
     * @param width 图像宽度 (1-8192)
     * @param height 图像高度 (1-8192)
     * @param channels 图像通道数 (1-8, 8bit深度)
     * @return 成功返回true，失败返回false
     */
    bool setImageResolution(int width, int height, int channels);
    
    /**
     * @brief 获取当前图像宽度
     * @return 图像宽度像素数
     */
    int getImageWidth() const { return m_imageWidth; }
    
    /**
     * @brief 获取当前图像高度
     * @return 图像高度像素数
     */
    int getImageHeight() const { return m_imageHeight; }
    
    /**
     * @brief 获取当前图像通道数
     * @return 图像通道数
     */
    int getImageChannels() const { return m_imageChannels; }
    
    /**
     * @brief 获取当前图像数据总大小
     * @return 图像数据字节数
     */
    int getImageTotalSize() const { return m_totalsize; }
    
    /**
     * @brief 获取当前tap模式
     * @return tap模式（1或2）
     */
    int getTapMode() const { return m_tapMode; }
    
    /**
     * @brief 设置自动重连参数
     * @param enabled 是否启用自动重连
     * @param maxAttempts 最大重连尝试次数（默认5次）
     * @param interval 重连间隔时间（毫秒，默认3000ms）
     */
    void setAutoReconnect(bool enabled, int maxAttempts = 5, int interval = 3000);
    
    /**
     * @brief 获取当前连接状态
     * @return 连接状态
     */
    QAbstractSocket::SocketState getConnectionState() const;
    
    /**
     * @brief 手动触发重连
     * 立即尝试重新连接到服务器
     */
    void reconnectNow();
    
    /**
     * @brief 获取当前重连尝试次数
     * @return 当前重连尝试次数
     */
    int getCurrentReconnectAttempts() const { return m_reconnectAttempts; }
    
    /**
     * @brief 获取最大重连尝试次数
     * @return 最大重连尝试次数
     */
    int getMaxReconnectAttempts() const { return m_maxReconnectAttempts; }
    
    /**
     * @brief 获取重连间隔时间
     * @return 重连间隔时间（毫秒）
     */
    int getReconnectInterval() const { return m_reconnectInterval; }
    
    /**
     * @brief 检查是否正在重连
     * @return 如果重连定时器正在运行返回true
     */
    bool isReconnecting() const { return m_reconnectTimer && m_reconnectTimer->isActive(); }
    
    /**
     * @brief 获取重连定时器剩余时间
     * @return 剩余时间（毫秒），如果定时器未运行返回-1
     */
    int getReconnectRemainingTime() const { 
        return (m_reconnectTimer && m_reconnectTimer->isActive()) ? m_reconnectTimer->remainingTime() : -1; 
    }

    /**
     * @brief 停止自动重连
     */
    void stopAutoReconnect();

    /**
     * @brief 执行服务端诊断检查
     * 当重连失败后，检查服务端状态和网络连通性
     */
    void performServerDiagnostics();

    /**
     * @brief 检查网络连通性
     * @param host 目标主机地址
     * @param port 目标端口
     * @return 连通性检查结果
     */
    QString checkNetworkConnectivity(const QString& host, int port);
    
    /**
     * @brief 生成诊断报告
     * @return 详细的诊断报告字符串
     */
    QString generateDiagnosticReport();

public slots:
    /**
     * @brief 启动TCP连接
     * @param strAddr 服务器IP地址字符串
     * @param port 服务器端口号
     */
    void start(QString strAddr, int port);
    /**
     * @brief 发送消息槽函数（预留接口）
     * 当前版本未实现，预留用于向服务器发送消息
     */
    void slot_sendmessage();
    /**
     * @brief 接收消息处理槽函数
     * 处理从服务器接收到的图像数据和控制消息
     */
    void slot_recvmessage();
    /**
     * @brief 连接断开处理槽函数
     * 处理TCP连接断开事件，清理相关状态
     */
    void slot_disconnect();
    /**
     * @brief 连接建立成功处理槽函数
     * 处理TCP连接成功建立事件，初始化接收状态
     */
    void slot_connected();
    /**
     * @brief 套接字错误处理槽函数
     * @param error 套接字错误类型枚举值
     * 处理各种网络连接错误，提供详细错误信息
     */
    void slot_socketError(QAbstractSocket::SocketError error);
    
    /**
     * @brief 自动重连槽函数
     * 在连接断开后尝试重新连接到服务器
     */
    void slot_reconnect();
    
    /**
     * @brief 停止自动重连
     * 停止重连定时器，取消自动重连
     */
    void stopReconnect();
signals:
   /**
    * @brief 图像数据就绪信号
    * 当完整的图像数据接收完成并存储到缓冲区后发射此信号
    * 通知界面层更新图像显示
    */
   void  tcpImgReadySig();
   
   /**
    * @brief 图像数据接收信号
    * @param imgData 接收到的图像数据
    */
   void signalImgData(QByteArray imgData);
   
   /**
    * @brief 诊断信息更新信号
    * @param diagnosticInfo 诊断信息文本
    */
   void signalDiagnosticInfo(QString diagnosticInfo);

   // 添加新的信号
   void signal_showframestruct(const QString &info);
   void signal_showframeheader(const QString &info);

private:
   QTcpSocket* TCP_sendMesSocket;  ///< TCP套接字对象指针，用于网络通信
   bool m_brefresh;                ///< 刷新标志位，表示是否正在接收数据
   QByteArray pictmp;              ///< 临时数据缓冲区，用于累积接收的图像数据
   char* frameBuffer;              ///< 图像帧缓冲区，存储完整的图像数据
   int m_totalsize;                ///< 预期接收的图像数据总大小（字节）
   
   // 重连相关成员变量
   QTimer* m_reconnectTimer;       ///< 重连定时器
   QString m_serverAddress;        ///< 服务器地址
   int m_serverPort;               ///< 服务器端口
   int m_reconnectAttempts;        ///< 重连尝试次数
   int m_maxReconnectAttempts;     ///< 最大重连尝试次数
   int m_reconnectInterval;        ///< 重连间隔时间（毫秒）
   bool m_autoReconnectEnabled;    ///< 是否启用自动重连
   
       // 动态图像参数
    int m_imageWidth;               ///< 图像宽度（像素）
    int m_imageHeight;              ///< 图像高度（像素）
    int m_imageChannels;            ///< 图像通道数
    int m_tapMode;                  ///< tap模式（1=单点采样，2=双点插值）
   
       /**
     * @brief 重新分配图像缓冲区
     * @return 成功返回true，失败返回false
     */
    bool reallocateFrameBuffer();
    
    /**
     * @brief 格式化数据为十六进制字符串用于调试显示
     * @param data 原始数据
     * @param maxBytes 最大显示字节数
     * @return 格式化的十六进制字符串
     */
    QString formatDataForDebug(const QByteArray& data, int maxBytes = 20);
    
    /**
     * @brief 验证帧头是否匹配预期的模式
     * @param data 要验证的数据
     * @param expectedHeader 预期的帧头字节序列
     * @return 如果帧头匹配返回true，否则返回false
     */
    bool validateFrameHeader(const QByteArray& data, const QByteArray& expectedHeader);
    
    /**
     * @brief 在数据中搜索帧头位置
     * @param data 要搜索的数据
     * @param header 要搜索的帧头
     * @return 帧头位置，-1表示未找到
     */
    int findFrameHeader(const QByteArray& data, const QByteArray& header);
    
    /**
     * @brief 从帧头解析帧大小信息
     * @param frameHeader 帧头数据（至少6字节）
     * @return 解析出的帧大小，-1表示解析失败
     */
    int parseFrameSize(const QByteArray& frameHeader);
    
    /**
     * @brief 验证帧数据完整性
     * @param frameData 完整帧数据
     * @return 如果帧数据有效返回true
     */
    bool validateFrameData(const QByteArray& frameData);
    
    /**
     * @brief 图像质量检测功能
     * @param imageData 图像数据
     * @return 质量检测报告字符串
     */
    QString analyzeImageQuality(const QByteArray& imageData);
    
    /**
     * @brief 触发重连逻辑的内部函数
     * @param source 触发源（用于调试日志）
     */
    void triggerReconnectLogic(const QString& source);

    // 添加新的成员变量
    qint64 m_recvCount;           // 接收数据计数
    QByteArray m_recvBuffer;      // 接收缓冲区
    bool m_foundFirstFrame;       // 是否找到第一帧标志

    // 添加新的成员函数
    void updateImageDisplay(const QByteArray &imageData);
    
    /**
     * @brief 直接显示图像数据（简化版）
     * @param imageData 图像数据
     */
    void updateImageDisplayDirect(const QByteArray &imageData);
};

#endif // CTCPIMG_H
