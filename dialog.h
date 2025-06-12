#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <ctcpimg.h>
#include <QImage>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QTabWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QCheckBox>
#include "sysdefine.h"
#include "tcpdebugger.h"
#include "dataformatter.h"

namespace Ui {
class Dialog;
}

/**
 * @class Dialog
 * @brief 主对话框类
 * 
 * 负责用户界面的显示和交互，包括：
 * - TCP连接配置界面
 * - 图像显示区域管理
 * - 用户输入验证和错误提示
 * - 与CTCPImg类的协调工作
 */
class Dialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，用于Qt窗口管理
     * 
     * 初始化用户界面，设置信号槽连接，分配图像显示缓冲区
     */
    explicit Dialog(QWidget *parent = 0);
    
    /**
     * @brief 析构函数
     * 
     * 清理UI资源，释放图像显示缓冲区内存
     */
    ~Dialog();

public slots:
    /**
     * @brief 显示图像标签的槽函数
     * 
     * 当接收到完整的图像数据时被CTCPImg对象调用
     * 负责将图像数据转换为QImage格式并在界面上显示
     * 包含完整的错误处理和用户反馈机制
     */
    void showLabelImg();

    // 网络调试功能相关槽函数
    /**
     * @brief 调试数据接收槽函数
     * @param data 原始数据
     * @param formattedData 格式化后的数据
     * @param remoteAddress 远程地址
     */
    void onDebugDataReceived(const QByteArray& data, const QString& formattedData, const QString& remoteAddress);

    /**
     * @brief 调试连接状态变化槽函数
     * @param state 连接状态
     * @param message 状态信息
     */
    void onDebugConnectionStateChanged(CTCPDebugger::ConnectionState state, const QString& message);

private slots:
    /**
     * @brief 启动网络调试
     */
    void startDebugMode();

    /**
     * @brief 停止网络调试
     */
    void stopDebugMode();

    /**
     * @brief 发送调试数据
     */
    void sendDebugData();

    /**
     * @brief 清空调试数据显示
     */
    void clearDebugData();

    /**
     * @brief 数据格式变化槽函数
     */
    void onDataFormatChanged();

    /**
     * @brief 工作模式变化槽函数
     */
    void onWorkModeChanged();

    /**
     * @brief 刷新本地IP地址列表
     */
    void refreshLocalIPAddresses();

    /**
     * @brief 应用分辨率设置
     */
    void applyResolutionSettings();

    /**
     * @brief 重置分辨率为默认值
     */
    void resetResolutionToDefault();

private:
    Ui::Dialog *ui;          ///< UI界面指针，由Qt设计器生成的界面对象
    CTCPImg m_tcpImg;        ///< TCP图像传输对象，处理网络通信和数据接收
    char* m_showBuffer;      ///< 图像显示缓冲区，用于临时存储要显示的图像数据
    QImage m_qimage;         ///< Qt图像对象，用于图像格式转换和显示处理

    // 网络调试功能相关成员
    CTCPDebugger* m_tcpDebugger;        ///< TCP网络调试器对象
    QTabWidget* m_tabWidget;            ///< 标签页控件
    QWidget* m_imageTab;                ///< 图像传输标签页
    QWidget* m_debugTab;                ///< 网络调试标签页
    
    // 调试界面控件
    QTextEdit* m_debugDataDisplay;      ///< 调试数据显示区域
    QLineEdit* m_debugHostEdit;         ///< 调试主机地址输入
    QLineEdit* m_debugPortEdit;         ///< 调试端口输入
    QComboBox* m_dataFormatCombo;       ///< 数据格式选择
    QComboBox* m_localIPCombo;          ///< 本地IP地址选择
    QPushButton* m_refreshIPBtn;        ///< 刷新IP地址按钮
    QRadioButton* m_clientModeRadio;    ///< 客户端模式选择
    QRadioButton* m_serverModeRadio;    ///< 服务器模式选择
    QPushButton* m_debugStartBtn;       ///< 调试开始按钮
    QPushButton* m_debugStopBtn;        ///< 调试停止按钮
    QPushButton* m_debugSendBtn;        ///< 调试发送按钮
    QPushButton* m_debugClearBtn;       ///< 调试清空按钮
    QLineEdit* m_debugSendEdit;         ///< 调试发送数据输入
    QLabel* m_debugStatusLabel;         ///< 调试状态标签
    QLabel* m_debugStatsLabel;          ///< 调试统计信息标签
    QCheckBox* m_timestampCheckBox;     ///< 时间戳显示选择
    
    // 分辨率设置相关控件
    QLineEdit* m_widthEdit;             ///< 图像宽度输入框
    QLineEdit* m_heightEdit;            ///< 图像高度输入框
    QComboBox* m_channelsCombo;         ///< 图像通道数选择
    QPushButton* m_applyResolutionBtn;  ///< 应用分辨率按钮
    QPushButton* m_resetResolutionBtn;  ///< 重置分辨率按钮
    QLabel* m_resolutionStatusLabel;    ///< 分辨率状态标签

    /**
     * @brief 初始化调试界面
     */
    void initDebugInterface();

    /**
     * @brief 创建调试控制面板
     * @return 控制面板布局
     */
    QLayout* createDebugControlPanel();

    /**
     * @brief 创建调试数据面板
     * @return 数据面板布局
     */
    QLayout* createDebugDataPanel();

    /**
     * @brief 更新调试界面状态
     */
    void updateDebugUIState();

    /**
     * @brief 创建分辨率设置面板
     * @return 分辨率设置面板布局
     */
    QLayout* createResolutionPanel();

    /**
     * @brief 更新分辨率状态显示
     */
    void updateResolutionStatus();
};

#endif // DIALOG_H
