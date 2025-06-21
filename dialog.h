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
#include <QTimer>
#include <QProgressBar>
#include <QVariantList>
#include <QSlider>
#include <QScrollArea>
#include <QPixmap>
#include <QResizeEvent>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QRegularExpression>
#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
#include <QRegExp>
#endif
#include "sysdefine.h"
#include "tcpdebugger.h"
#include "dataformatter.h"

// 前向声明
// class CommandWindow; // 已移除独立窗口
// #include "ui_dialog.h"  // 已使用现代化界面替代

// namespace Ui {
// class Dialog;
// }  // 已使用现代化界面替代

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

    /**
     * @brief 执行服务端诊断
     */
    void performDiagnostics();
    
    /**
     * @brief 显示诊断信息
     * @param diagnosticInfo 诊断信息文本
     */
    void showDiagnosticInfo(const QString& diagnosticInfo);

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
    
    /**
     * @brief 应用分辨率预设
     * @param index 预设索引
     */
    void applyResolutionPreset(int index);

    /**
     * @brief 手动重连槽函数
     */
    void manualReconnect();
    
    /**
     * @brief 切换自动重连状态
     */
    void toggleAutoReconnect(bool enabled);

    /**
     * @brief 更新分辨率状态显示
     */
    void updateResolutionStatus();
    
    /**
     * @brief 创建图像缩放控制面板
     * @return 缩放控制面板布局
     */
    QLayout* createZoomControlPanel();
    
    /**
     * @brief 更新图像显示
     * @param pixmap 要显示的图像
     */
    void updateImageDisplay(const QPixmap& pixmap);
    
    /**
     * @brief 缩放图像到指定因子
     * @param factor 缩放因子
     */
    void scaleImage(double factor);
    
    /**
     * @brief 适应窗口大小显示图像
     */
    void fitImageToWindow();
    
    /**
     * @brief 显示图像实际大小
     */
    void showActualSize();
    
    /**
     * @brief 放大图像
     */
    void zoomIn();
    
    /**
     * @brief 缩小图像
     */
    void zoomOut();
    
    /**
     * @brief 设置缩放因子
     * @param factor 缩放因子
     */
    void setZoomFactor(double factor);
    
    /**
     * @brief 更新缩放控件状态
     */
    void updateZoomControls();

    /**
     * @brief 更新连接状态显示
     */
    void updateConnectionStatus();
    
    /**
     * @brief 更新重连进度显示
     */
    void updateReconnectProgress();

    /**
     * @brief 切换控制面板的可见性
     */
    void toggleControlsVisibility();

    /**
     * @brief 刷新串口列表
     */
    void refreshSerialPorts();
    
    /**
     * @brief 连接/断开串口
     */
    void toggleSerialConnection();
    
    /**
     * @brief 发送时间字符显示指令
     */
    void sendTimeDisplayCommand();
    
    /**
     * @brief 发送关闭字符显示指令
     */
    void sendTimeOffCommand();
    
    /**
     * @brief 开始/停止自动切换字符显示
     */
    void toggleAutoDisplaySwitch();
    
    /**
     * @brief 自动切换字符显示状态
     */
    void autoSwitchDisplay();
    
    /**
     * @brief 发送自定义指令
     */
    void sendCustomCommand();
    
    /**
     * @brief 清空指令数据显示
     */
    void clearCommandData();
    
    /**
     * @brief 更新时间显示
     */
    void updateTimeDisplay();
    
    /**
     * @brief 串口数据接收处理
     */
    void onCommandSerialDataReceived();
    
    /**
     * @brief 串口错误处理
     */
    void onCommandSerialError(QSerialPort::SerialPortError error);
    
    /**
     * @brief 字符显示状态改变
     */
    void onCommandDisplayStateChanged();
    
    /**
     * @brief 更新指令数据统计
     */
    void updateCommandDataStats();
    
    /**
     * @brief 切换编辑模式
     */
    void toggleEditMode(bool enabled);
    
    /**
     * @brief 保存接收数据到文件
     */
    void saveReceiveDataToFile();
    
    /**
     * @brief 显示接收数据右键菜单
     */
    void showReceiveDataContextMenu(const QPoint& pos);

protected:
    /**
     * @brief 窗口大小调整事件
     * @param event 调整大小事件
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    // Ui::Dialog *ui;          ///< UI界面指针，已使用现代化界面替代
    CTCPImg m_tcpImg;        ///< TCP图像传输对象，处理网络通信和数据接收
    char* m_showBuffer;      ///< 图像显示缓冲区，用于临时存储要显示的图像数据
    QImage m_qimage;         ///< Qt图像对象，用于图像格式转换和显示处理

    // 网络调试功能相关成员
    CTCPDebugger* m_tcpDebugger;        ///< TCP网络调试器对象
    QTabWidget* m_tabWidget;            ///< 标签页控件
    QWidget* m_imageTab;                ///< 图像传输标签页
    QWidget* m_debugTab;                ///< 网络调试标签页
    QWidget* m_commandTab;              ///< 指令调试标签页
    
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
    QComboBox* m_channelsCombo;         ///< 通道数选择下拉框
    QComboBox* m_resolutionPresetCombo; ///< 分辨率预设下拉框
    QPushButton* m_applyResolutionBtn;  ///< 应用分辨率按钮
    QPushButton* m_resetResolutionBtn;  ///< 重置分辨率按钮
    QLabel* m_resolutionStatusLabel;    ///< 分辨率状态标签
    
    // 图像缩放相关控件
    QSlider* m_zoomSlider;              ///< 缩放滑块
    QLabel* m_zoomLabel;                ///< 缩放比例标签
    QPushButton* m_fitWindowBtn;        ///< 适应窗口按钮
    QPushButton* m_actualSizeBtn;       ///< 实际大小按钮
    QPushButton* m_zoomInBtn;           ///< 放大按钮
    QPushButton* m_zoomOutBtn;          ///< 缩小按钮
    QScrollArea* m_imageScrollArea;     ///< 图像滚动区域
    QLabel* m_imageDisplayLabel;        ///< 图像显示标签（替代原来的labelShowImg）
    
    // 重连控制相关控件
    QPushButton* m_reconnectBtn;        ///< 手动重连按钮
    QCheckBox* m_autoReconnectCheckBox; ///< 自动重连开关
    QLabel* m_connectionStatusLabel;    ///< 连接状态标签
    QLabel* m_reconnectProgressLabel;   ///< 重连进度标签
    QProgressBar* m_reconnectProgressBar; ///< 重连进度条
    QTimer* m_reconnectDisplayTimer;    ///< 重连显示更新定时器
    QPushButton* m_diagnosticBtn;       ///< 诊断按钮
    
    // 现代化服务器连接控件
    QLineEdit* m_serverIPEdit;          ///< 服务器IP输入框
    QLineEdit* m_serverPortEdit;        ///< 服务器端口输入框
    QPushButton* m_connectBtn;          ///< 连接按钮

    // 缩放相关变量
    double m_currentZoomFactor;         ///< 当前缩放因子
    QPixmap m_originalPixmap;           ///< 原始图像像素图
    bool m_fitToWindow;                 ///< 是否适应窗口模式
    QTimer* m_resizeTimer;              ///< 用于窗口缩放防抖动的定时器

    // 界面显隐控制
    QWidget* m_controlsContainer;       ///< 容纳所有可隐藏控件的容器
    QPushButton* m_toggleControlsBtn;   ///< 用于显示/隐藏控件的按钮
    bool m_controlsVisible;             ///< 控件是否可见的状态标志

    // 指令调试功能相关控件
    QComboBox* m_serialPortCombo;       ///< 串口选择下拉框
    QComboBox* m_baudRateCombo;         ///< 波特率选择
    QComboBox* m_dataBitsCombo;         ///< 数据位选择
    QComboBox* m_stopBitsCombo;         ///< 停止位选择
    QComboBox* m_parityCombo;           ///< 校验位选择
    QComboBox* m_flowControlCombo;      ///< 流控制选择
    QPushButton* m_refreshPortBtn;      ///< 刷新串口按钮
    QPushButton* m_connectSerialBtn;    ///< 串口连接按钮
    QLabel* m_serialStatusLabel;        ///< 串口连接状态标签
    
    QLabel* m_currentTimeLabel;         ///< 当前时间显示
    QCheckBox* m_displayOnCheckBox;     ///< 字符显示开启复选框
    QPushButton* m_sendTimeBtn;         ///< 发送时间指令按钮
    QPushButton* m_sendTimeOffBtn;      ///< 发送关闭字符显示按钮
    QPushButton* m_autoSwitchBtn;       ///< 自动切换字符显示按钮
    QLineEdit* m_timeCommandPreview;    ///< 时间指令预览
    
    QLineEdit* m_customCommandEdit;     ///< 自定义指令输入框
    QPushButton* m_sendCustomBtn;       ///< 发送自定义指令按钮
    QCheckBox* m_hexModeCheckBox;       ///< 16进制模式复选框
    
    QTextEdit* m_commandReceiveDisplay; ///< 指令接收数据显示区
    QTextEdit* m_commandSendDisplay;    ///< 指令发送数据显示区
    QPushButton* m_clearCommandBtn;     ///< 清空指令数据按钮
    QLabel* m_commandStatsLabel;        ///< 指令统计标签
    QCheckBox* m_editModeCheckBox;      ///< 编辑模式开关
    QPushButton* m_saveReceiveDataBtn;  ///< 保存接收数据按钮
    
    QSerialPort* m_serialPort;          ///< 串口对象
    QTimer* m_timeUpdateTimer;          ///< 时间更新定时器
    QTimer* m_autoSwitchTimer;          ///< 自动切换定时器
    qint64 m_totalBytesSent;            ///< 发送字节总数
    qint64 m_totalBytesReceived;        ///< 接收字节总数
    int m_commandCount;                 ///< 发送指令计数
    bool m_autoSwitchEnabled;           ///< 自动切换是否启用
    bool m_currentDisplayState;         ///< 当前显示状态 (true=开启, false=关闭)

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
     * @brief 创建指令调试标签页内容
     */
    void createCommandTab();
    
    /**
     * @brief 生成39字节时间字符显示指令
     * @param dateTime 指定的时间，如果为空则使用当前时间
     * @return 生成的39字节指令数据
     */
    QByteArray generateTimeDisplayCommand(const QDateTime& dateTime = QDateTime());

    /**
     * @brief 生成39字节关闭字符显示指令
     * @param dateTime 指定的时间，如果为空则使用当前时间
     * @return 生成的39字节关闭显示指令数据
     */
    QByteArray generateTimeOffCommand(const QDateTime& dateTime = QDateTime());

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
     * @brief 创建重连控制面板
     * @return 重连控制面板布局
     */
    QLayout* createReconnectPanel();
    
    /**
     * @brief 创建服务器连接面板
     * @return 服务器连接面板布局
     */
    QLayout* createServerConnectionPanel();

    /**
     * @brief 设置统一的现代化样式表
     */
    void setUnifiedStyleSheet();
};

#endif // DIALOG_H
