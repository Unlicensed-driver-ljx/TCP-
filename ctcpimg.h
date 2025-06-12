#ifndef CTCPIMG_H
#define CTCPIMG_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTcpSocket>
#include <QHostAddress>
#include <QNetworkProxy>
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
signals:
   /**
    * @brief 图像数据就绪信号
    * 当完整的图像数据接收完成并存储到缓冲区后发射此信号
    * 通知界面层更新图像显示
    */
   void  tcpImgReadySig();
private:
   QTcpSocket* TCP_sendMesSocket;  ///< TCP套接字对象指针，用于网络通信
   bool m_brefresh;                ///< 刷新标志位，表示是否正在接收数据
   QByteArray pictmp;              ///< 临时数据缓冲区，用于累积接收的图像数据
   char* frameBuffer;              ///< 图像帧缓冲区，存储完整的图像数据
   int m_totalsize;                ///< 预期接收的图像数据总大小（字节）
   
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
};

#endif // CTCPIMG_H
