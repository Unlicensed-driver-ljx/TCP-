#include "dialog.h"
#include "ui_dialog.h"

/**
 * @brief Dialog构造函数
 * @param parent 父窗口指针
 * 
 * 初始化主对话框，设置UI界面，连接信号槽，分配图像显示缓冲区
 */
Dialog::Dialog(QWidget *parent) :
    QDialog(parent),
    // ui(new Ui::Dialog),  // 已移除UI依赖
    m_showBuffer(nullptr),
    m_reconnectBtn(nullptr),
    m_autoReconnectCheckBox(nullptr),
    m_connectionStatusLabel(nullptr),
    m_serverIPEdit(nullptr),
    m_serverPortEdit(nullptr),
    m_connectBtn(nullptr),
    m_currentZoomFactor(1.0),
    m_fitToWindow(true),
    m_resizeTimer(nullptr),
    m_controlsContainer(nullptr),
    m_toggleControlsBtn(nullptr),
    m_controlsVisible(true)
{
    // 设置用户界面
    // ui->setupUi(this);  // 不再需要，使用完全现代化界面
    
    // 设置窗口属性（替代UI文件）
    this->setWindowTitle("TCP图像传输程序 v2.2.3");
    this->resize(1603, 700);
    this->setMinimumSize(800, 600);  // 设置最小窗口尺寸
    
    // 设置统一的现代化样式表
    setUnifiedStyleSheet();
    
    // 初始化网络调试器
    m_tcpDebugger = new CTCPDebugger(this);
    
    // 连接调试器信号
    connect(m_tcpDebugger, &CTCPDebugger::dataReceived, 
            this, &Dialog::onDebugDataReceived);
    connect(m_tcpDebugger, &CTCPDebugger::connectionStateChanged, 
            this, &Dialog::onDebugConnectionStateChanged);
    
    // 初始化调试界面
    initDebugInterface();
    
    // 现代化服务器连接面板初始化
    // 注意：这些控件将在createServerConnectionPanel()中创建

    // 连接TCP图像数据就绪信号到图像显示槽函数
    connect(&m_tcpImg, &CTCPImg::tcpImgReadySig, this, &Dialog::showLabelImg);
    
    // 连接诊断信息信号
    connect(&m_tcpImg, &CTCPImg::signalDiagnosticInfo, this, &Dialog::showDiagnosticInfo);
    
    // 初始化自动重连功能（默认启用）
    // 注意：这个调用必须在initDebugInterface()之后，因为控件需要先创建
    QTimer::singleShot(100, this, [this]() {
        if (m_autoReconnectCheckBox && m_autoReconnectCheckBox->isChecked()) {
            toggleAutoReconnect(true);
        }
    });
    
    // 为图像显示分配内存缓冲区
    m_showBuffer = new char[WIDTH * HEIGHT * CHANLE];
    if (m_showBuffer == nullptr) {
        qDebug() << "严重错误：图像缓冲区内存分配失败";
        m_imageDisplayLabel->setText("严重错误：内存分配失败\n程序可能无法正常工作");
        return;
    }
    
    // 初始化显示缓冲区为0
    memset((void*)m_showBuffer, 0, WIDTH * HEIGHT * CHANLE);
    
    // 设置标签的初始显示文本（已使用现代化界面）
    // ui->labelShowImg->setText("TCP图像传输接收程序已启动\n\n请输入服务器地址和端口号，然后点击开始连接\n\n默认配置：\nIP：192.168.1.31\n端口：17777");
    // ui->labelShowImg->setAlignment(Qt::AlignCenter);  // 居中显示文本
    
    qDebug() << "Dialog界面初始化完成，图像缓冲区大小：" << (WIDTH * HEIGHT * CHANLE) << "字节";

    // 初始化缩放防抖动定时器
    m_resizeTimer = new QTimer(this);
    m_resizeTimer->setSingleShot(true); // 设置为单次触发
    m_resizeTimer->setInterval(50);     // 设置50ms延迟
    connect(m_resizeTimer, &QTimer::timeout, this, &Dialog::fitImageToWindow);
}

/**
 * @brief Dialog析构函数
 * 
 * 清理UI资源和释放图像显示缓冲区内存
 */
Dialog::~Dialog()
{
    // 释放UI资源（已使用现代化界面）
    // delete ui;
    
    // 释放图像显示缓冲区
    if (m_showBuffer != nullptr) {
        delete[] m_showBuffer;
        m_showBuffer = nullptr;
        qDebug() << "图像显示缓冲区已释放";
    }
    
    qDebug() << "Dialog对象销毁完成";
}

/**
 * @brief 显示图像的槽函数
 * 
 * 当接收到完整图像数据时被调用，将图像数据转换为QImage并显示在界面上
 * 包含完整的错误检查和异常处理机制
 */
void Dialog::showLabelImg()
{
    qDebug() << "开始更新图像显示...";
    
    // 从TCP图像对象获取图像帧缓冲区
    char *frameBuffer = m_tcpImg.getFrameBuffer();
    if (frameBuffer == nullptr) {
        qDebug() << "错误：获取图像缓冲区失败";
        m_imageDisplayLabel->setText("错误：无法获取图像数据");
        // ui->pushButtonStart->setEnabled(true);  // 已移除原始UI控件
        return;
    }
    
    // 获取当前图像参数
    int width = m_tcpImg.getImageWidth();
    int height = m_tcpImg.getImageHeight();
    int channels = m_tcpImg.getImageChannels();
    int totalSize = width * height * channels;
    
    // 将接收到的图像数据复制到显示缓冲区
    try {
        memcpy(m_showBuffer, frameBuffer, totalSize);
    } catch (const std::exception& e) {
        qDebug() << "错误：图像数据复制失败：" << e.what();
        m_imageDisplayLabel->setText("错误：图像数据处理失败");
        // ui->pushButtonStart->setEnabled(true);  // 已移除原始UI控件
        return;
    }
    
    // 根据通道数选择合适的图像格式
    QImage::Format imageFormat;
    switch (channels) {
        case 1:
            imageFormat = QImage::Format_Grayscale8;
            break;
        case 3:
            imageFormat = QImage::Format_RGB888;
            break;
        case 4:
            imageFormat = QImage::Format_RGBA8888;
            break;
        case 2:
        case 5:
        case 6:
        case 7:
        case 8:
            // 对于2通道或5-8通道，提取第一个通道显示为灰度图像
            imageFormat = QImage::Format_Grayscale8;
            qDebug() << "多通道图像" << channels << "通道，提取第一通道显示为灰度图像";
            break;
        default:
            // 其他情况使用灰度格式
            imageFormat = QImage::Format_Grayscale8;
            qDebug() << "不支持的通道数" << channels << "，使用灰度格式显示";
            break;
    }
    
    // 创建QImage对象进行图像格式转换
    if (channels == 1) {
        // 灰度图像：直接使用数据
        m_qimage = QImage((const unsigned char*)(m_showBuffer), width, height, imageFormat);
    } else if (channels == 3) {
        // RGB图像：计算行跨度
        int bytesPerLine = width * channels;
        m_qimage = QImage((const unsigned char*)(m_showBuffer), width, height, bytesPerLine, imageFormat);
    } else if (channels == 4) {
        // RGBA图像：计算行跨度
        int bytesPerLine = width * channels;
        m_qimage = QImage((const unsigned char*)(m_showBuffer), width, height, bytesPerLine, imageFormat);
    } else {
        // 多通道图像（2，5-8通道）：提取第一个通道显示
        unsigned char* grayBuffer = new unsigned char[width * height];
        
        try {
            // 提取第一个通道的数据
            for (int i = 0; i < width * height; ++i) {
                grayBuffer[i] = static_cast<unsigned char>(m_showBuffer[i * channels]);
            }
            
            // 创建灰度图像
            m_qimage = QImage(grayBuffer, width, height, QImage::Format_Grayscale8).copy();
            
            // 释放临时缓冲区
            delete[] grayBuffer;
            
            qDebug() << "多通道图像处理完成，提取了第一通道用于显示";
            
        } catch (const std::exception& e) {
            qDebug() << "多通道图像处理失败：" << e.what();
            delete[] grayBuffer;
            m_imageDisplayLabel->setText("错误：多通道图像处理失败");
            // ui->pushButtonStart->setEnabled(true);  // 已移除原始UI控件
            return;
        }
    }
    
    // 检查QImage对象是否创建成功
    if (!m_qimage.isNull()) {
        // 图像创建成功，保存原始图像并更新显示
        m_originalPixmap = QPixmap::fromImage(m_qimage);
        
        // 更新图像显示
        updateImageDisplay(m_originalPixmap);
        
        // 重新启用开始按钮，允许用户重新连接
        // ui->pushButtonStart->setEnabled(true);  // 已移除原始UI控件
        
        qDebug() << "图像显示更新成功，图像尺寸：" << m_qimage.width() << "x" << m_qimage.height();
        
        // 🔍 在界面上显示帧头验证结果
        char* frameBuffer = m_tcpImg.getFrameBuffer();
        if (frameBuffer && totalSize >= 2) {
            unsigned char* data = reinterpret_cast<unsigned char*>(frameBuffer);
            bool headerMatch = (data[0] == 0x7E && data[1] == 0x7E);
            
            QString headerInfo = QString("帧头：%1 %2 %3")
                                .arg(data[0], 2, 16, QChar('0')).toUpper()
                                .arg(data[1], 2, 16, QChar('0')).toUpper()
                                .arg(headerMatch ? "✅" : "❌");
            
            // 显示更多帧结构信息
            if (totalSize >= 8) {
                QString frameStructure = QString("帧结构：%1 %2 | %3 %4 %5 %6 %7 %8")
                                        .arg(data[0], 2, 16, QChar('0')).toUpper()
                                        .arg(data[1], 2, 16, QChar('0')).toUpper()
                                        .arg(data[2], 2, 16, QChar('0')).toUpper()
                                        .arg(data[3], 2, 16, QChar('0')).toUpper()
                                        .arg(data[4], 2, 16, QChar('0')).toUpper()
                                        .arg(data[5], 2, 16, QChar('0')).toUpper()
                                        .arg(data[6], 2, 16, QChar('0')).toUpper()
                                        .arg(data[7], 2, 16, QChar('0')).toUpper();
                qDebug() << "界面显示帧结构：" << frameStructure;
            }
            
            // 可以在状态栏或其他地方显示这个信息
            qDebug() << "界面显示帧头信息：" << headerInfo;
        }
        
        // 图像显示成功，重新启用开始按钮
        // ui->pushButtonStart->setEnabled(true);  // 已移除原始UI控件
    }
    else {
        // 图像创建失败
        qDebug() << "错误：QImage对象创建失败，可能是图像数据格式不正确";
        qDebug() << "图像参数：宽度=" << width << "，高度=" << height << "，通道数=" << channels;
        
        // 显示错误信息给用户
        m_imageDisplayLabel->setText("错误：图像数据格式不正确\n\n可能原因：\n1. 图像数据损坏\n2. 数据格式不匹配\n3. 网络传输错误\n\n请检查服务器端图像格式设置");
        
        // 重新启用开始按钮
        // ui->pushButtonStart->setEnabled(true);  // 已移除原始UI控件
    }
}

/**
 * @brief 初始化调试界面
 * 
 * 创建包含图像传输和网络调试两个标签页的界面
 */
void Dialog::initDebugInterface()
{
    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);
    
    // 创建图像传输标签页
    m_imageTab = new QWidget();
    QVBoxLayout* imageLayout = new QVBoxLayout(m_imageTab);
    
    // --- 新增：顶部工具栏 ---
    QHBoxLayout* topToolbarLayout = new QHBoxLayout();
    m_toggleControlsBtn = new QPushButton("🔼 隐藏控件");
    m_toggleControlsBtn->setToolTip("显示或隐藏下方的所有控制面板");
    m_toggleControlsBtn->setStyleSheet("QPushButton { background-color: transparent; border: 1px solid #ccc; padding: 4px 8px; }");
    connect(m_toggleControlsBtn, &QPushButton::clicked, this, &Dialog::toggleControlsVisibility);
    
    topToolbarLayout->addWidget(m_toggleControlsBtn);
    topToolbarLayout->addStretch();
    imageLayout->addLayout(topToolbarLayout);

    // --- 修改：创建可隐藏的控件容器 ---
    m_controlsContainer = new QWidget();
    QVBoxLayout* controlsLayout = new QVBoxLayout(m_controlsContainer);
    controlsLayout->setContentsMargins(0, 0, 0, 0); // 移除容器的边距

    // 将所有控制面板添加到这个容器中
    controlsLayout->addLayout(createServerConnectionPanel());
    controlsLayout->addLayout(createResolutionPanel());
    controlsLayout->addLayout(createReconnectPanel());
    controlsLayout->addLayout(createZoomControlPanel());
    
    imageLayout->addWidget(m_controlsContainer); // 将容器添加到主布局
    
    // 创建图像滚动区域替代原来的labelShowImg
    m_imageScrollArea = new QScrollArea();
    m_imageDisplayLabel = new QLabel();
    m_imageDisplayLabel->setAlignment(Qt::AlignCenter);
    m_imageDisplayLabel->setStyleSheet("QLabel { background-color: #f0f0f0; border: 1px solid #ccc; }");
    m_imageDisplayLabel->setText("TCP图像传输接收程序已启动\n\n请输入服务器地址和端口号，然后点击开始连接\n\n默认配置：\nIP：192.168.1.31\n端口：17777");
    
    m_imageScrollArea->setWidget(m_imageDisplayLabel);
    m_imageScrollArea->setWidgetResizable(false);  // 不自动调整大小，支持滚动
    m_imageScrollArea->setAlignment(Qt::AlignCenter);
    
    imageLayout->addWidget(m_imageScrollArea, 1);  // 图像显示区占主要空间
    
    m_tabWidget->addTab(m_imageTab, "图像传输");
    
    // 创建网络调试标签页
    m_debugTab = new QWidget();
    QVBoxLayout* debugLayout = new QVBoxLayout(m_debugTab);
    
    // 添加调试控制面板
    debugLayout->addLayout(createDebugControlPanel());
    
    // 添加调试数据面板  
    debugLayout->addLayout(createDebugDataPanel());
    
    m_tabWidget->addTab(m_debugTab, "网络调试");
    
    // 重新设置主窗口布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
    
    // 初始化调试界面状态
    updateDebugUIState();
    
    // 显示网络代理状态信息
    QNetworkProxy appProxy = QNetworkProxy::applicationProxy();
    qDebug() << "当前应用程序代理类型：" << appProxy.type();
    qDebug() << "调试界面初始化完成";
    
    m_debugDataDisplay->append("=== TCP图像传输 + 网络调试工具 v2.0 ===");
    m_debugDataDisplay->append("✅ 网络代理已禁用，避免代理设置干扰");
    m_debugDataDisplay->append("✅ 支持客户端/服务器双模式");
    m_debugDataDisplay->append("✅ 支持多种数据格式显示");
    m_debugDataDisplay->append("📝 使用说明：");
    m_debugDataDisplay->append("  1. 选择工作模式（客户端/服务器）");
    m_debugDataDisplay->append("  2. 配置连接参数");
    m_debugDataDisplay->append("  3. 选择数据显示格式");
    m_debugDataDisplay->append("  4. 点击开始按钮建立连接");
    m_debugDataDisplay->append("准备就绪，等待操作...\n");
}

/**
 * @brief 创建调试控制面板
 * @return 控制面板布局
 */
QLayout* Dialog::createDebugControlPanel()
{
    QVBoxLayout* controlLayout = new QVBoxLayout();
    
    // 工作模式选择组
    QGroupBox* modeGroup = new QGroupBox("工作模式");
    QHBoxLayout* modeLayout = new QHBoxLayout(modeGroup);
    
    m_clientModeRadio = new QRadioButton("客户端模式");
    m_serverModeRadio = new QRadioButton("服务器模式");
    m_clientModeRadio->setChecked(true);  // 默认客户端模式
    
    modeLayout->addWidget(m_clientModeRadio);
    modeLayout->addWidget(m_serverModeRadio);
    
    // 连接工作模式变化信号
    connect(m_clientModeRadio, &QRadioButton::toggled, this, &Dialog::onWorkModeChanged);
    connect(m_serverModeRadio, &QRadioButton::toggled, this, &Dialog::onWorkModeChanged);
    
    controlLayout->addWidget(modeGroup);
    
    // 连接参数组
    QGroupBox* connectionGroup = new QGroupBox("连接参数");
    QGridLayout* connectionLayout = new QGridLayout(connectionGroup);
    
    // 客户端模式的控件
    connectionLayout->addWidget(new QLabel("目标主机:"), 0, 0);
    m_debugHostEdit = new QLineEdit("127.0.0.1");
    connectionLayout->addWidget(m_debugHostEdit, 0, 1);
    
    // 服务器模式的控件
    connectionLayout->addWidget(new QLabel("本地IP:"), 1, 0);
    
    QHBoxLayout* ipLayout = new QHBoxLayout();
    m_localIPCombo = new QComboBox();
    m_localIPCombo->addItems(CTCPDebugger::getLocalIPAddresses());
    m_localIPCombo->setToolTip("选择服务器监听的本地IP地址");
    
    m_refreshIPBtn = new QPushButton("🔄");
    m_refreshIPBtn->setFixedSize(30, 25);
    m_refreshIPBtn->setToolTip("刷新本地IP地址列表");
    m_refreshIPBtn->setStyleSheet("QPushButton { font-size: 12px; }");
    
    ipLayout->addWidget(m_localIPCombo);
    ipLayout->addWidget(m_refreshIPBtn);
    
    QWidget* ipWidget = new QWidget();
    ipWidget->setLayout(ipLayout);
    connectionLayout->addWidget(ipWidget, 1, 1);
    
    // 连接刷新按钮信号
    connect(m_refreshIPBtn, &QPushButton::clicked, this, &Dialog::refreshLocalIPAddresses);
    
    connectionLayout->addWidget(new QLabel("端口:"), 2, 0);
    m_debugPortEdit = new QLineEdit("12345");
    connectionLayout->addWidget(m_debugPortEdit, 2, 1);
    
    controlLayout->addWidget(connectionGroup);
    
    // 数据格式选择组
    QGroupBox* formatGroup = new QGroupBox("数据显示格式");
    QHBoxLayout* formatLayout = new QHBoxLayout(formatGroup);
    
    m_dataFormatCombo = new QComboBox();
    m_dataFormatCombo->addItem("原始文本", static_cast<int>(CDataFormatter::FORMAT_RAW_TEXT));
    m_dataFormatCombo->addItem("十六进制", static_cast<int>(CDataFormatter::FORMAT_HEX));
    m_dataFormatCombo->addItem("二进制", static_cast<int>(CDataFormatter::FORMAT_BINARY));
    m_dataFormatCombo->addItem("ASCII码", static_cast<int>(CDataFormatter::FORMAT_ASCII));
    m_dataFormatCombo->addItem("JSON格式", static_cast<int>(CDataFormatter::FORMAT_JSON));
    m_dataFormatCombo->addItem("混合显示", static_cast<int>(CDataFormatter::FORMAT_MIXED));
    
    m_timestampCheckBox = new QCheckBox("显示时间戳");
    m_timestampCheckBox->setChecked(true);
    
    formatLayout->addWidget(m_dataFormatCombo);
    formatLayout->addWidget(m_timestampCheckBox);
    
    // Qt 5.12兼容性：使用SIGNAL/SLOT宏
    connect(m_dataFormatCombo, SIGNAL(currentIndexChanged(int)), 
            this, SLOT(onDataFormatChanged()));
    connect(m_timestampCheckBox, &QCheckBox::toggled, [this](bool checked) {
        m_tcpDebugger->setShowTimestamp(checked);
    });
    
    controlLayout->addWidget(formatGroup);
    
    // 控制按钮组
    QGroupBox* buttonGroup = new QGroupBox("操作控制");
    QHBoxLayout* buttonLayout = new QHBoxLayout(buttonGroup);
    
    m_debugStartBtn = new QPushButton("开始");
    m_debugStopBtn = new QPushButton("停止");
    m_debugClearBtn = new QPushButton("清空");
    
    m_debugStartBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    m_debugStopBtn->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    
    buttonLayout->addWidget(m_debugStartBtn);
    buttonLayout->addWidget(m_debugStopBtn);
    buttonLayout->addWidget(m_debugClearBtn);
    
    // 连接按钮信号
    connect(m_debugStartBtn, &QPushButton::clicked, this, &Dialog::startDebugMode);
    connect(m_debugStopBtn, &QPushButton::clicked, this, &Dialog::stopDebugMode);
    connect(m_debugClearBtn, &QPushButton::clicked, this, &Dialog::clearDebugData);
    
    controlLayout->addWidget(buttonGroup);
    
    // 状态信息组
    QGroupBox* statusGroup = new QGroupBox("状态信息");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);
    
    m_debugStatusLabel = new QLabel("状态：未连接");
    m_debugStatsLabel = new QLabel("统计：无数据");
    
    m_debugStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: #666; }");
    m_debugStatsLabel->setStyleSheet("QLabel { font-size: 9pt; color: #888; }");
    
    statusLayout->addWidget(m_debugStatusLabel);
    statusLayout->addWidget(m_debugStatsLabel);
    
    controlLayout->addWidget(statusGroup);
    
    return controlLayout;
}

/**
 * @brief 创建调试数据面板
 * @return 数据面板布局
 */
QLayout* Dialog::createDebugDataPanel()
{
    QVBoxLayout* dataLayout = new QVBoxLayout();
    
    // 数据显示区域
    QGroupBox* displayGroup = new QGroupBox("接收数据");
    QVBoxLayout* displayLayout = new QVBoxLayout(displayGroup);
    
    m_debugDataDisplay = new QTextEdit();
    m_debugDataDisplay->setReadOnly(true);
    m_debugDataDisplay->setFont(QFont("Consolas", 9));  // 使用等宽字体
    m_debugDataDisplay->setPlainText("等待数据接收...\n\n提示：\n- 支持多种数据格式显示\n- 可显示时间戳\n- 支持客户端和服务器模式\n- 实时统计连接信息");
    
    displayLayout->addWidget(m_debugDataDisplay);
    dataLayout->addWidget(displayGroup);
    
    // 数据发送区域
    QGroupBox* sendGroup = new QGroupBox("发送数据");
    QHBoxLayout* sendLayout = new QHBoxLayout(sendGroup);
    
    m_debugSendEdit = new QLineEdit();
    m_debugSendEdit->setPlaceholderText("输入要发送的数据...");
    
    m_debugSendBtn = new QPushButton("发送");
    m_debugSendBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; }");
    
    sendLayout->addWidget(m_debugSendEdit);
    sendLayout->addWidget(m_debugSendBtn);
    
    // 连接发送功能
    connect(m_debugSendBtn, &QPushButton::clicked, this, &Dialog::sendDebugData);
    connect(m_debugSendEdit, &QLineEdit::returnPressed, this, &Dialog::sendDebugData);
    
    dataLayout->addWidget(sendGroup);
    
    return dataLayout;
}

/**
 * @brief 更新调试界面状态
 */
void Dialog::updateDebugUIState()
{
    CTCPDebugger::ConnectionState state = m_tcpDebugger->getConnectionState();
    bool isConnected = (state == CTCPDebugger::STATE_CONNECTED || state == CTCPDebugger::STATE_LISTENING);
    
    // 更新按钮状态
    m_debugStartBtn->setEnabled(!isConnected);
    m_debugStopBtn->setEnabled(isConnected);
    m_debugSendBtn->setEnabled(isConnected);
    
    // 更新模式控件状态
    m_clientModeRadio->setEnabled(!isConnected);
    m_serverModeRadio->setEnabled(!isConnected);
    
    // 更新连接参数控件状态
    bool isClientMode = m_clientModeRadio->isChecked();
    m_debugHostEdit->setEnabled(!isConnected && isClientMode);
    m_localIPCombo->setEnabled(!isConnected && !isClientMode);
    m_debugPortEdit->setEnabled(!isConnected);
    
    // 更新统计信息
    m_debugStatsLabel->setText(m_tcpDebugger->getConnectionStats());
}

/**
 * @brief 启动网络调试
 */
void Dialog::startDebugMode()
{
    // 设置工作模式
    CTCPDebugger::WorkMode mode = m_clientModeRadio->isChecked() ? 
                                  CTCPDebugger::MODE_CLIENT : CTCPDebugger::MODE_SERVER;
    m_tcpDebugger->setWorkMode(mode);
    
    // 设置数据显示格式
    onDataFormatChanged();
    
    // 获取端口号
    bool ok;
    quint16 port = m_debugPortEdit->text().toUInt(&ok);
    if (!ok || port == 0) {
        m_debugDataDisplay->append("错误：端口号格式不正确");
        return;
    }
    
    // 根据模式启动
    if (mode == CTCPDebugger::MODE_CLIENT) {
        QString host = m_debugHostEdit->text().trimmed();
        if (host.isEmpty()) {
            m_debugDataDisplay->append("错误：请输入目标主机地址");
            return;
        }
        m_tcpDebugger->startClient(host, port);
    } else {
        QString selectedIP = m_localIPCombo->currentText();
        QHostAddress bindAddress;
        
        // 从组合框文本中提取IP地址（格式："IP地址 (接口名称)"）
        QString ipAddress = selectedIP.split(" ").first();
        
        if (ipAddress == "127.0.0.1") {
            bindAddress = QHostAddress::LocalHost;
        } else if (ipAddress == "0.0.0.0") {
            bindAddress = QHostAddress::Any;  // 监听所有接口
        } else {
            bindAddress = QHostAddress(ipAddress);
        }
        
        qDebug() << "服务器模式 - 选择的IP：" << selectedIP << "绑定地址：" << bindAddress.toString();
        m_tcpDebugger->startServer(port, bindAddress);
    }
    
    updateDebugUIState();
}

/**
 * @brief 停止网络调试
 */
void Dialog::stopDebugMode()
{
    m_tcpDebugger->stop();
    updateDebugUIState();
    m_debugDataDisplay->append("=== 连接已停止 ===");
}

/**
 * @brief 发送调试数据
 */
void Dialog::sendDebugData()
{
    QString text = m_debugSendEdit->text();
    if (text.isEmpty()) {
        return;
    }
    
    qint64 sent = m_tcpDebugger->sendText(text);
    if (sent > 0) {
        m_debugDataDisplay->append(QString(">>> 发送: %1 (%2 字节)").arg(text).arg(sent));
        m_debugSendEdit->clear();
    } else {
        m_debugDataDisplay->append(">>> 发送失败：连接异常");
    }
    
    updateDebugUIState();
}

/**
 * @brief 清空调试数据显示
 */
void Dialog::clearDebugData()
{
    m_debugDataDisplay->clear();
    m_tcpDebugger->clearStats();
    updateDebugUIState();
}

/**
 * @brief 数据格式变化槽函数
 */
void Dialog::onDataFormatChanged()
{
    int formatValue = m_dataFormatCombo->currentData().toInt();
    CDataFormatter::DataDisplayFormat format = static_cast<CDataFormatter::DataDisplayFormat>(formatValue);
    m_tcpDebugger->setDataDisplayFormat(format);
}

/**
 * @brief 工作模式变化槽函数
 */
void Dialog::onWorkModeChanged()
{
    updateDebugUIState();
}

/**
 * @brief 调试数据接收槽函数
 * @param data 原始数据
 * @param formattedData 格式化后的数据
 * @param remoteAddress 远程地址
 */
void Dialog::onDebugDataReceived(const QByteArray& data, const QString& formattedData, const QString& remoteAddress)
{
    QString displayText = QString("<<< 接收来自 %1 (%2 字节):\n%3\n")
                         .arg(remoteAddress)
                         .arg(data.size())
                         .arg(formattedData);
    
    m_debugDataDisplay->append(displayText);
    
    // 自动滚动到底部
    QTextCursor cursor = m_debugDataDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_debugDataDisplay->setTextCursor(cursor);
    
    updateDebugUIState();
}

/**
 * @brief 调试连接状态变化槽函数
 * @param state 连接状态
 * @param message 状态信息
 */
void Dialog::onDebugConnectionStateChanged(CTCPDebugger::ConnectionState state, const QString& message)
{
    Q_UNUSED(state)  // 标记参数已被处理，避免编译警告
    
    m_debugStatusLabel->setText(QString("状态：%1").arg(message));
    m_debugDataDisplay->append(QString("=== %1 ===").arg(message));
    
    updateDebugUIState();
}

/**
 * @brief 刷新本地IP地址列表
 */
void Dialog::refreshLocalIPAddresses()
{
    QString currentSelection = m_localIPCombo->currentText();
    
    m_localIPCombo->clear();
    QStringList ipAddresses = CTCPDebugger::getLocalIPAddresses();
    m_localIPCombo->addItems(ipAddresses);
    
    // 尝试恢复之前的选择
    int index = m_localIPCombo->findText(currentSelection);
    if (index >= 0) {
        m_localIPCombo->setCurrentIndex(index);
    } else if (!ipAddresses.isEmpty()) {
        // 如果找不到之前的选择，默认选择第一个非回环地址
        for (int i = 0; i < ipAddresses.size(); ++i) {
            if (!ipAddresses[i].startsWith("127.0.0.1")) {
                m_localIPCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    
    m_debugDataDisplay->append(QString("=== 已刷新本地IP地址列表，发现 %1 个可用地址 ===").arg(ipAddresses.size()));
    
    qDebug() << "本地IP地址列表已刷新，当前选择：" << m_localIPCombo->currentText();
}

/**
 * @brief 创建分辨率设置面板
 * @return 分辨率设置面板布局
 */
QLayout* Dialog::createResolutionPanel()
{
    QGroupBox* resolutionGroup = new QGroupBox("图像分辨率设置");
    QVBoxLayout* mainLayout = new QVBoxLayout(resolutionGroup);
    
    // 第一行：分辨率预设选择
    QHBoxLayout* presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel("预设:"));
    
    m_resolutionPresetCombo = new QComboBox();
    m_resolutionPresetCombo->addItem("自定义", QVariantList({0, 0}));
    m_resolutionPresetCombo->addItem("640x480 (VGA)", QVariantList({640, 480}));
    m_resolutionPresetCombo->addItem("800x600 (SVGA)", QVariantList({800, 600}));
    m_resolutionPresetCombo->addItem("1024x768 (XGA)", QVariantList({1024, 768}));
    m_resolutionPresetCombo->addItem("1280x720 (HD)", QVariantList({1280, 720}));
    m_resolutionPresetCombo->addItem("1280x1024 (SXGA)", QVariantList({1280, 1024}));
    m_resolutionPresetCombo->addItem("1600x1200 (UXGA)", QVariantList({1600, 1200}));
    m_resolutionPresetCombo->addItem("1920x1080 (FHD)", QVariantList({1920, 1080}));
    m_resolutionPresetCombo->addItem("2048x1536 (QXGA)", QVariantList({2048, 1536}));
    m_resolutionPresetCombo->addItem("2560x1440 (QHD)", QVariantList({2560, 1440}));
    m_resolutionPresetCombo->addItem("3840x2160 (4K)", QVariantList({3840, 2160}));
    m_resolutionPresetCombo->addItem("640x2048 (线阵)", QVariantList({640, 2048}));
    m_resolutionPresetCombo->addItem("1024x2048 (线阵)", QVariantList({1024, 2048}));
    m_resolutionPresetCombo->addItem("2048x2048 (方形)", QVariantList({2048, 2048}));
    m_resolutionPresetCombo->addItem("4096x4096 (大方形)", QVariantList({4096, 4096}));
    
    m_resolutionPresetCombo->setFixedWidth(180);
    m_resolutionPresetCombo->setToolTip("选择常用分辨率预设，或选择'自定义'手动输入");
    presetLayout->addWidget(m_resolutionPresetCombo);
    presetLayout->addStretch();
    
    mainLayout->addLayout(presetLayout);
    
    // 第二行：手动输入分辨率
    QHBoxLayout* resolutionLayout = new QHBoxLayout();
    
    // 宽度设置
    resolutionLayout->addWidget(new QLabel("宽度:"));
    m_widthEdit = new QLineEdit(QString::number(m_tcpImg.getImageWidth()));
    m_widthEdit->setFixedWidth(80);
    m_widthEdit->setToolTip("图像宽度 (1-8192)");
    resolutionLayout->addWidget(m_widthEdit);
    
    // 高度设置
    resolutionLayout->addWidget(new QLabel("高度:"));
    m_heightEdit = new QLineEdit(QString::number(m_tcpImg.getImageHeight()));
    m_heightEdit->setFixedWidth(80);
    m_heightEdit->setToolTip("图像高度 (1-8192)");
    resolutionLayout->addWidget(m_heightEdit);
    
    // 通道数设置
    resolutionLayout->addWidget(new QLabel("通道:"));
    m_channelsCombo = new QComboBox();
    m_channelsCombo->addItem("1 (灰度)", 1);
    m_channelsCombo->addItem("2 (双通道)", 2);
    m_channelsCombo->addItem("3 (RGB)", 3);
    m_channelsCombo->addItem("4 (RGBA)", 4);
    m_channelsCombo->addItem("5 (5通道)", 5);
    m_channelsCombo->addItem("6 (6通道)", 6);
    m_channelsCombo->addItem("7 (7通道)", 7);
    m_channelsCombo->addItem("8 (8通道)", 8);
    
    // 设置当前通道数
    for (int i = 0; i < m_channelsCombo->count(); ++i) {
        if (m_channelsCombo->itemData(i).toInt() == m_tcpImg.getImageChannels()) {
            m_channelsCombo->setCurrentIndex(i);
            break;
        }
    }
    
    m_channelsCombo->setFixedWidth(120);
    m_channelsCombo->setToolTip("图像通道数 (1-8通道, 8位深度)\n多通道图像将提取第一通道显示");
    resolutionLayout->addWidget(m_channelsCombo);
    
    // 应用按钮
    m_applyResolutionBtn = new QPushButton("应用");
    m_applyResolutionBtn->setStyleSheet("QPushButton { background-color: #2196F3; color: white; font-weight: bold; }");
    m_applyResolutionBtn->setToolTip("应用新的分辨率设置");
    resolutionLayout->addWidget(m_applyResolutionBtn);
    
    // 重置按钮
    m_resetResolutionBtn = new QPushButton("重置");
    m_resetResolutionBtn->setToolTip("重置为默认分辨率");
    resolutionLayout->addWidget(m_resetResolutionBtn);
    
    // 添加一些弹性空间
    resolutionLayout->addStretch();
    
    mainLayout->addLayout(resolutionLayout);
    
    // 第三行：状态标签
    m_resolutionStatusLabel = new QLabel();
    updateResolutionStatus();
    m_resolutionStatusLabel->setStyleSheet("QLabel { color: #666; font-size: 9pt; }");
    mainLayout->addWidget(m_resolutionStatusLabel);
    
    // 连接信号槽
    connect(m_applyResolutionBtn, &QPushButton::clicked, this, &Dialog::applyResolutionSettings);
    connect(m_resetResolutionBtn, &QPushButton::clicked, this, &Dialog::resetResolutionToDefault);
    connect(m_resolutionPresetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &Dialog::applyResolutionPreset);
    
    // 连接宽度和高度输入框的变化信号，自动设置预设为"自定义"
    connect(m_widthEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_resolutionPresetCombo->currentIndex() != 0) {
            m_resolutionPresetCombo->setCurrentIndex(0); // 设置为"自定义"
        }
    });
    connect(m_heightEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_resolutionPresetCombo->currentIndex() != 0) {
            m_resolutionPresetCombo->setCurrentIndex(0); // 设置为"自定义"
        }
    });
    
    QVBoxLayout* panelLayout = new QVBoxLayout();
    panelLayout->addWidget(resolutionGroup);
    
    return panelLayout;
}

/**
 * @brief 应用分辨率设置
 */
void Dialog::applyResolutionSettings()
{
    // 获取输入值
    bool widthOk, heightOk;
    int width = m_widthEdit->text().toInt(&widthOk);
    int height = m_heightEdit->text().toInt(&heightOk);
    int channels = m_channelsCombo->currentData().toInt();
    
    // 验证输入
    if (!widthOk || width <= 0) {
        m_imageDisplayLabel->setText("错误：图像宽度格式不正确");
        return;
    }
    
    if (!heightOk || height <= 0) {
        m_imageDisplayLabel->setText("错误：图像高度格式不正确");
        return;
    }
    
    // 计算内存大小并提醒用户
    long long totalBytes = (long long)width * height * channels;
    if (totalBytes > 50 * 1024 * 1024) {
        m_imageDisplayLabel->setText(QString("错误：图像数据过大\n需要 %1 MB 内存，超过50MB限制")
                                  .arg(totalBytes / 1024.0 / 1024.0, 0, 'f', 1));
        return;
    }
    
    // 应用新的分辨率设置
    if (m_tcpImg.setImageResolution(width, height, channels)) {
        // 重新分配显示缓冲区
        if (m_showBuffer != nullptr) {
            delete[] m_showBuffer;
        }
        
        try {
            m_showBuffer = new char[totalBytes];
            memset(m_showBuffer, 0, totalBytes);
            
                         updateResolutionStatus();
             
             QString channelInfo;
             if (channels == 1) channelInfo = "灰度图像";
             else if (channels == 3) channelInfo = "RGB彩色图像";
             else if (channels == 4) channelInfo = "RGBA彩色图像";
             else channelInfo = QString("%1通道图像(提取第一通道显示)").arg(channels);
             
             m_imageDisplayLabel->setText(QString("✅ 分辨率设置成功\n\n新设置：%1 x %2 x %3\n格式：8bit %4\n内存占用：%5 MB\n\n准备接收新的图像数据...")
                                       .arg(width).arg(height).arg(channels)
                                       .arg(channelInfo)
                                       .arg(totalBytes / 1024.0 / 1024.0, 0, 'f', 2));
                                      
            qDebug() << "分辨率设置成功：" << width << "x" << height << "x" << channels;
            
        } catch (const std::bad_alloc&) {
            m_imageDisplayLabel->setText("错误：显示缓冲区内存分配失败");
            m_showBuffer = nullptr;
        }
    } else {
        m_imageDisplayLabel->setText("错误：分辨率设置失败\n请检查输入参数");
    }
}

/**
 * @brief 重置分辨率为默认值
 */
void Dialog::resetResolutionToDefault()
{
    m_widthEdit->setText(QString::number(WIDTH));
    m_heightEdit->setText(QString::number(HEIGHT));
    
    // 设置通道数
    for (int i = 0; i < m_channelsCombo->count(); ++i) {
        if (m_channelsCombo->itemData(i).toInt() == CHANLE) {
            m_channelsCombo->setCurrentIndex(i);
            break;
        }
    }
    
    // 自动应用默认设置
    applyResolutionSettings();
    
    m_imageDisplayLabel->setText("✅ 已重置为默认分辨率\n\n准备接收图像数据...");
    qDebug() << "分辨率已重置为默认值";
}

/**
 * @brief 应用分辨率预设
 * @param index 预设索引
 */
void Dialog::applyResolutionPreset(int index)
{
    if (index == 0) {
        // 选择了"自定义"，不做任何操作
        qDebug() << "用户选择自定义分辨率";
        return;
    }
    
    // 获取预设的分辨率数据
    QVariantList resolution = m_resolutionPresetCombo->itemData(index).toList();
    if (resolution.size() != 2) {
        qDebug() << "错误：分辨率预设数据格式不正确";
        return;
    }
    
    int width = resolution[0].toInt();
    int height = resolution[1].toInt();
    
    qDebug() << QString("应用分辨率预设：%1x%2").arg(width).arg(height);
    
    // 临时断开信号连接，避免触发"自定义"设置
    disconnect(m_widthEdit, &QLineEdit::textChanged, nullptr, nullptr);
    disconnect(m_heightEdit, &QLineEdit::textChanged, nullptr, nullptr);
    
    // 更新输入框的值
    m_widthEdit->setText(QString::number(width));
    m_heightEdit->setText(QString::number(height));
    
    // 重新连接信号
    connect(m_widthEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_resolutionPresetCombo->currentIndex() != 0) {
            m_resolutionPresetCombo->setCurrentIndex(0); // 设置为"自定义"
        }
    });
    connect(m_heightEdit, &QLineEdit::textChanged, this, [this]() {
        if (m_resolutionPresetCombo->currentIndex() != 0) {
            m_resolutionPresetCombo->setCurrentIndex(0); // 设置为"自定义"
        }
    });
    
    // 自动应用新的分辨率设置
    applyResolutionSettings();
    
    QString presetName = m_resolutionPresetCombo->currentText();
    m_imageDisplayLabel->setText(QString("✅ 已应用分辨率预设：%1\n\n准备接收图像数据...").arg(presetName));
    qDebug() << QString("分辨率预设已应用：%1 (%2x%3)").arg(presetName).arg(width).arg(height);
}

/**
 * @brief 更新分辨率状态显示
 */
void Dialog::updateResolutionStatus()
{
    int width = m_tcpImg.getImageWidth();
    int height = m_tcpImg.getImageHeight();
    int channels = m_tcpImg.getImageChannels();
    long long totalBytes = (long long)width * height * channels;
    
    QString statusText = QString("当前：%1x%2x%3 (8bit, %4 MB)")
                        .arg(width).arg(height).arg(channels)
                        .arg(totalBytes / 1024.0 / 1024.0, 0, 'f', 2);
    
    m_resolutionStatusLabel->setText(statusText);
}

/**
 * @brief 创建重连控制面板
 * @return 重连控制面板布局
 */
QLayout* Dialog::createReconnectPanel()
{
    QGroupBox* reconnectGroup = new QGroupBox("🔗 连接控制");
    QVBoxLayout* mainLayout = new QVBoxLayout(reconnectGroup);
    
    // 第一行：连接状态和控制按钮
    QHBoxLayout* controlLayout = new QHBoxLayout();
    
    // 连接状态标签
    m_connectionStatusLabel = new QLabel("状态：未连接");
    m_connectionStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: #666; padding: 4px 8px; border-radius: 3px; background-color: #f0f0f0; }");
    
    // 自动重连开关
    m_autoReconnectCheckBox = new QCheckBox("🔄 自动重连");
    m_autoReconnectCheckBox->setChecked(true);  // 默认启用
    m_autoReconnectCheckBox->setToolTip("启用后，连接断开时会自动尝试重连\n最大5次尝试，间隔3秒");
    
    // 手动重连按钮
    m_reconnectBtn = new QPushButton("🚀 立即重连");
    m_reconnectBtn->setEnabled(false);  // 初始状态禁用
    m_reconnectBtn->setToolTip("手动触发重连，会重置重连计数");
    
    // 诊断按钮
    m_diagnosticBtn = new QPushButton("🔍 诊断");
    m_diagnosticBtn->setEnabled(true);
    m_diagnosticBtn->setToolTip("检查服务端状态和网络连通性");
    
    controlLayout->addWidget(m_connectionStatusLabel);
    controlLayout->addWidget(m_autoReconnectCheckBox);
    controlLayout->addWidget(m_reconnectBtn);
    controlLayout->addWidget(m_diagnosticBtn);
    controlLayout->addStretch();
    
    // 第二行：重连进度显示
    QHBoxLayout* progressLayout = new QHBoxLayout();
    
    // 重连进度标签
    m_reconnectProgressLabel = new QLabel("重连状态：待机");
    m_reconnectProgressLabel->setStyleSheet("QLabel { color: #666; font-size: 9pt; }");
    
    // 重连进度条
    m_reconnectProgressBar = new QProgressBar();
    m_reconnectProgressBar->setVisible(false);  // 初始隐藏
    m_reconnectProgressBar->setMaximum(100);
    m_reconnectProgressBar->setTextVisible(true);
    m_reconnectProgressBar->setStyleSheet(R"(
        QProgressBar {
            border: 2px solid #ddd;
            border-radius: 5px;
            text-align: center;
            font-weight: bold;
            background-color: #f0f0f0;
        }
        QProgressBar::chunk {
            background-color: #FF9800;
            border-radius: 3px;
        }
    )");
    
    progressLayout->addWidget(m_reconnectProgressLabel);
    progressLayout->addWidget(m_reconnectProgressBar);
    
    // 添加到主布局
    mainLayout->addLayout(controlLayout);
    mainLayout->addLayout(progressLayout);
    
    // 连接信号
    connect(m_autoReconnectCheckBox, &QCheckBox::toggled, this, &Dialog::toggleAutoReconnect);
    connect(m_reconnectBtn, &QPushButton::clicked, this, &Dialog::manualReconnect);
    connect(m_diagnosticBtn, &QPushButton::clicked, this, &Dialog::performDiagnostics);
    
    // 启动定时器定期更新连接状态
    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &Dialog::updateConnectionStatus);
    statusTimer->start(1000);  // 每秒更新一次状态
    
    // 重连进度显示定时器
    m_reconnectDisplayTimer = new QTimer(this);
    connect(m_reconnectDisplayTimer, &QTimer::timeout, this, &Dialog::updateReconnectProgress);
    
    // 创建一个垂直布局来包装GroupBox
    QVBoxLayout* panelLayout = new QVBoxLayout();
    panelLayout->addWidget(reconnectGroup);
    
    return panelLayout;
}

/**
 * @brief 更新连接状态显示
 */
void Dialog::updateConnectionStatus()
{
    if (!m_connectionStatusLabel) return;
    
    QAbstractSocket::SocketState state = m_tcpImg.getConnectionState();
    QString statusText;
    QString styleSheet;
    
    switch (state) {
        case QAbstractSocket::UnconnectedState:
            statusText = "🔴 未连接";
            styleSheet = "QLabel { font-weight: bold; color: white; background-color: #666; padding: 4px 8px; border-radius: 3px; }";
            if (m_reconnectBtn) m_reconnectBtn->setEnabled(true);
            break;
        case QAbstractSocket::HostLookupState:
            statusText = "🔍 查找主机...";
            styleSheet = "QLabel { font-weight: bold; color: white; background-color: #FF9800; padding: 4px 8px; border-radius: 3px; }";
            if (m_reconnectBtn) m_reconnectBtn->setEnabled(false);
            break;
        case QAbstractSocket::ConnectingState:
            statusText = "🔄 连接中...";
            styleSheet = "QLabel { font-weight: bold; color: white; background-color: #FF9800; padding: 4px 8px; border-radius: 3px; }";
            if (m_reconnectBtn) m_reconnectBtn->setEnabled(false);
            break;
        case QAbstractSocket::ConnectedState:
            statusText = "🟢 已连接";
            styleSheet = "QLabel { font-weight: bold; color: white; background-color: #4CAF50; padding: 4px 8px; border-radius: 3px; }";
            if (m_reconnectBtn) m_reconnectBtn->setEnabled(false);
            // 连接成功时隐藏进度条
            if (m_reconnectProgressBar) {
                m_reconnectProgressBar->setVisible(false);
                m_reconnectProgressLabel->setText("重连状态：连接正常");
                if (m_reconnectDisplayTimer) m_reconnectDisplayTimer->stop();
            }
            break;
        case QAbstractSocket::BoundState:
            statusText = "🔗 已绑定";
            styleSheet = "QLabel { font-weight: bold; color: white; background-color: #2196F3; padding: 4px 8px; border-radius: 3px; }";
            if (m_reconnectBtn) m_reconnectBtn->setEnabled(false);
            break;
        case QAbstractSocket::ClosingState:
            statusText = "🔄 断开中...";
            styleSheet = "QLabel { font-weight: bold; color: white; background-color: #FF5722; padding: 4px 8px; border-radius: 3px; }";
            if (m_reconnectBtn) m_reconnectBtn->setEnabled(false);
            break;
        default:
            statusText = "❓ 未知状态";
            styleSheet = "QLabel { font-weight: bold; color: white; background-color: #9E9E9E; padding: 4px 8px; border-radius: 3px; }";
            if (m_reconnectBtn) m_reconnectBtn->setEnabled(true);
            break;
    }
    
    m_connectionStatusLabel->setText(statusText);
    m_connectionStatusLabel->setStyleSheet(styleSheet);
}

/**
 * @brief 手动重连槽函数
 */
void Dialog::manualReconnect()
{
    qDebug() << "用户触发手动重连";
    
    // 更新界面显示
    if (m_reconnectProgressLabel) {
        m_reconnectProgressLabel->setText("🚀 手动重连中...");
    }
    if (m_reconnectProgressBar) {
        m_reconnectProgressBar->setVisible(true);
        m_reconnectProgressBar->setValue(0);
        m_reconnectProgressBar->setFormat("正在重连...");
    }
    
    // 启动进度更新定时器
    if (m_reconnectDisplayTimer && !m_reconnectDisplayTimer->isActive()) {
        m_reconnectDisplayTimer->start(100);
    }
    
    // 触发重连
    m_tcpImg.reconnectNow();
    
    // 暂时禁用重连按钮，防止重复点击
    if (m_reconnectBtn) {
        m_reconnectBtn->setEnabled(false);
        
        // 3秒后重新启用按钮
        QTimer::singleShot(3000, this, [this]() {
            if (m_reconnectBtn && m_tcpImg.getConnectionState() != QAbstractSocket::ConnectedState) {
                m_reconnectBtn->setEnabled(true);
            }
        });
    }
}

/**
 * @brief 切换自动重连状态
 * @param enabled 是否启用自动重连
 */
void Dialog::toggleAutoReconnect(bool enabled)
{
    qDebug() << "自动重连设置变更：" << (enabled ? "启用" : "禁用");
    
    // 设置TCP图像对象的自动重连参数
    m_tcpImg.setAutoReconnect(enabled, 5, 3000);  // 最大5次，间隔3秒
    
    // 更新界面显示
    if (m_reconnectProgressLabel) {
        if (enabled) {
            QAbstractSocket::SocketState state = m_tcpImg.getConnectionState();
            if (state == QAbstractSocket::ConnectedState) {
                m_reconnectProgressLabel->setText("✅ 连接正常");
            } else {
                m_reconnectProgressLabel->setText("⏳ 自动重连已启用");
            }
        } else {
            m_reconnectProgressLabel->setText("🚫 自动重连已禁用");
            if (m_reconnectProgressBar) {
                m_reconnectProgressBar->setVisible(false);
            }
            if (m_reconnectDisplayTimer && m_reconnectDisplayTimer->isActive()) {
                m_reconnectDisplayTimer->stop();
            }
        }
    }
    
    // 如果禁用自动重连，停止当前的重连尝试
    if (!enabled) {
        m_tcpImg.stopReconnect();
    }
    
    qDebug() << "自动重连状态已更新：" << (enabled ? "启用" : "禁用");
}

/**
 * @brief 执行服务端诊断
 */
void Dialog::performDiagnostics()
{
    qDebug() << "用户手动触发服务端诊断";
    
    // 更新界面显示
    if (m_reconnectProgressLabel) {
        m_reconnectProgressLabel->setText("🔍 正在执行服务端诊断...");
    }
    
    // 暂时禁用诊断按钮，防止重复点击
    if (m_diagnosticBtn) {
        m_diagnosticBtn->setEnabled(false);
        m_diagnosticBtn->setText("🔍 诊断中...");
    }
    
    // 在主图像显示区域显示诊断提示
    m_imageDisplayLabel->setText("🔍 正在执行服务端诊断检查...\n\n请稍候，正在检测网络连通性和服务端状态...");
    
    // 异步执行诊断，避免阻塞UI
    QTimer::singleShot(100, this, [this]() {
        // 调用CTCPImg的诊断功能（会通过信号显示结果）
        m_tcpImg.performServerDiagnostics();
        
        // 更新界面显示
        if (m_reconnectProgressLabel) {
            m_reconnectProgressLabel->setText("✅ 诊断完成 | 详细信息已显示在图像区域");
        }
        
        // 重新启用诊断按钮
        if (m_diagnosticBtn) {
            m_diagnosticBtn->setEnabled(true);
            m_diagnosticBtn->setText("🔍 诊断");
        }
    });
}

/**
 * @brief 更新重连进度显示
 * 
 * 显示重连的实时进度，包括当前尝试次数、剩余时间等
 */
void Dialog::updateReconnectProgress()
{
    if (!m_reconnectProgressLabel || !m_reconnectProgressBar) return;
    
    QAbstractSocket::SocketState state = m_tcpImg.getConnectionState();
    bool isReconnecting = m_tcpImg.isReconnecting();
    int currentAttempts = m_tcpImg.getCurrentReconnectAttempts();
    int maxAttempts = m_tcpImg.getMaxReconnectAttempts();
    int remainingTime = m_tcpImg.getReconnectRemainingTime();
    int interval = m_tcpImg.getReconnectInterval();
    
    if (state == QAbstractSocket::ConnectedState) {
        // 连接成功
        m_reconnectProgressBar->setVisible(false);
        m_reconnectProgressLabel->setText("✅ 连接正常");
        if (m_reconnectDisplayTimer && m_reconnectDisplayTimer->isActive()) {
            m_reconnectDisplayTimer->stop();
        }
    } else if (state == QAbstractSocket::ConnectingState) {
        // 正在连接
        m_reconnectProgressLabel->setText("🔄 正在尝试连接...");
        m_reconnectProgressBar->setVisible(true);
        m_reconnectProgressBar->setValue(50);
        m_reconnectProgressBar->setFormat("连接中...");
    } else if (isReconnecting && remainingTime > 0) {
        // 正在重连等待中
        m_reconnectProgressBar->setVisible(true);
        
        QString progressText = QString("🔄 重连中 (第%1/%2次) - %3秒后重试")
                              .arg(currentAttempts)
                              .arg(maxAttempts)
                              .arg(remainingTime / 1000 + 1); // 转换为秒并向上取整
        
        m_reconnectProgressLabel->setText(progressText);
        
        // 计算进度百分比
        int totalTime = interval; // 总间隔时间
        int elapsedTime = totalTime - remainingTime;
        int progress = (elapsedTime * 100) / totalTime;
        
        m_reconnectProgressBar->setValue(progress);
        m_reconnectProgressBar->setFormat(QString("%1秒后重试").arg(remainingTime / 1000 + 1));
        
        // 启动进度更新定时器（如果还没启动）
        if (m_reconnectDisplayTimer && !m_reconnectDisplayTimer->isActive()) {
            m_reconnectDisplayTimer->start(100); // 每100ms更新一次进度
        }
    } else if (currentAttempts >= maxAttempts && !isReconnecting) {
        // 重连失败
        m_reconnectProgressBar->setVisible(false);
        m_reconnectProgressLabel->setText(QString("🔍 重连失败：正在诊断服务端状态..."));
        if (m_reconnectDisplayTimer && m_reconnectDisplayTimer->isActive()) {
            m_reconnectDisplayTimer->stop();
        }
        
        // 延迟显示诊断结果，给用户一个"正在诊断"的感觉
        QTimer::singleShot(2000, this, [this, maxAttempts]() {
            if (m_reconnectProgressLabel) {
                m_reconnectProgressLabel->setText(QString("❌ 连接失败：已尝试%1次 | 🔍 请检查服务端和采集端状态").arg(maxAttempts));
            }
        });
    } else if (state == QAbstractSocket::UnconnectedState && m_autoReconnectCheckBox->isChecked()) {
        // 未连接但启用了自动重连
        if (currentAttempts == 0) {
            m_reconnectProgressLabel->setText("⏳ 等待重连触发...");
            m_reconnectProgressBar->setVisible(false);
        } else {
            m_reconnectProgressLabel->setText(QString("🔄 准备第%1次重连...").arg(currentAttempts + 1));
            m_reconnectProgressBar->setVisible(false);
        }
    } else {
        // 其他状态
        m_reconnectProgressBar->setVisible(false);
        if (m_autoReconnectCheckBox->isChecked()) {
            m_reconnectProgressLabel->setText("⏸️ 重连待机");
        } else {
            m_reconnectProgressLabel->setText("🚫 自动重连已禁用");
        }
        if (m_reconnectDisplayTimer && m_reconnectDisplayTimer->isActive()) {
            m_reconnectDisplayTimer->stop();
        }
    }
}

/**
 * @brief 设置统一的现代化样式表
 * 
 * 为整个应用程序设置一致的现代化UI风格，包括：
 * - 统一的颜色方案
 * - 现代化的按钮样式
 * - 清晰的输入框样式
 * - 美观的状态指示器
 */
void Dialog::setUnifiedStyleSheet()
{
    QString styleSheet = R"(
        /* 主窗口样式 */
        QDialog {
            background-color: #f5f5f5;
            font-family: "Microsoft YaHei", "SimHei", sans-serif;
            font-size: 9pt;
        }
        
        /* 按钮统一样式 */
        QPushButton {
            background-color: #4CAF50;
            border: none;
            color: white;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: bold;
            min-width: 80px;
        }
        
        QPushButton:hover {
            background-color: #45a049;
        }
        
        QPushButton:pressed {
            background-color: #3d8b40;
        }
        
        QPushButton:disabled {
            background-color: #cccccc;
            color: #666666;
        }
        
        /* 特殊按钮样式 */
        QPushButton#pushButtonDebug {
            background-color: #2196F3;
        }
        
        QPushButton#pushButtonDebug:hover {
            background-color: #1976D2;
        }
        
        /* 重连按钮样式 */
        QPushButton[objectName*="reconnect"] {
            background-color: #FF9800;
        }
        
        QPushButton[objectName*="reconnect"]:hover {
            background-color: #F57C00;
        }
        
        /* 输入框统一样式 */
        QLineEdit {
            border: 2px solid #ddd;
            border-radius: 4px;
            padding: 6px 8px;
            background-color: white;
            selection-background-color: #4CAF50;
        }
        
        QLineEdit:focus {
            border-color: #4CAF50;
            outline: none;
        }
        
        /* 标签样式 */
        QLabel {
            color: #333333;
        }
        
        /* 状态标签特殊样式 */
        QLabel[objectName*="Status"] {
            font-weight: bold;
            padding: 4px 8px;
            border-radius: 3px;
            background-color: white;
            border: 1px solid #ddd;
        }
        
        /* 复选框样式 */
        QCheckBox {
            spacing: 8px;
        }
        
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 2px solid #ddd;
            background-color: white;
        }
        
        QCheckBox::indicator:checked {
            background-color: #4CAF50;
            border-color: #4CAF50;
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTIiIGhlaWdodD0iOSIgdmlld0JveD0iMCAwIDEyIDkiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+CjxwYXRoIGQ9Ik0xIDQuNUw0LjUgOEwxMSAxIiBzdHJva2U9IndoaXRlIiBzdHJva2Utd2lkdGg9IjIiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIgc3Ryb2tlLWxpbmVqb2luPSJyb3VuZCIvPgo8L3N2Zz4K);
        }
        
        /* 下拉框样式 */
        QComboBox {
            border: 2px solid #ddd;
            border-radius: 4px;
            padding: 6px 8px;
            background-color: white;
            min-width: 100px;
        }
        
        QComboBox:focus {
            border-color: #4CAF50;
        }
        
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        
        QComboBox::down-arrow {
            image: url(data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iMTIiIGhlaWdodD0iOCIgdmlld0JveD0iMCAwIDEyIDgiIGZpbGw9Im5vbmUiIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyI+CjxwYXRoIGQ9Ik0xIDFMNiA2TDExIDEiIHN0cm9rZT0iIzY2NjY2NiIgc3Ryb2tlLXdpZHRoPSIyIiBzdHJva2UtbGluZWNhcD0icm91bmQiIHN0cm9rZS1saW5lam9pbj0icm91bmQiLz4KPHN2Zz4K);
        }
        
        /* 文本编辑区域样式 */
        QTextEdit {
            border: 2px solid #ddd;
            border-radius: 4px;
            background-color: white;
            selection-background-color: #4CAF50;
        }
        
        QTextEdit:focus {
            border-color: #4CAF50;
        }
        
        /* 标签页样式 */
        QTabWidget::pane {
            border: 1px solid #ddd;
            background-color: white;
            border-radius: 4px;
        }
        
        QTabBar::tab {
            background-color: #e0e0e0;
            padding: 8px 16px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        
        QTabBar::tab:selected {
            background-color: #4CAF50;
            color: white;
        }
        
        QTabBar::tab:hover:!selected {
            background-color: #f0f0f0;
        }
        
        /* 分组框样式 */
        QGroupBox {
            font-weight: bold;
            border: 2px solid #ddd;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
            background-color: white;
        }
        
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 8px 0 8px;
            color: #4CAF50;
        }
        
        /* 单选按钮样式 */
        QRadioButton {
            spacing: 8px;
        }
        
        QRadioButton::indicator {
            width: 16px;
            height: 16px;
            border-radius: 8px;
            border: 2px solid #ddd;
            background-color: white;
        }
        
        QRadioButton::indicator:checked {
            background-color: #4CAF50;
            border-color: #4CAF50;
        }
        
        QRadioButton::indicator:checked::after {
            content: "";
            width: 6px;
            height: 6px;
            border-radius: 3px;
            background-color: white;
            position: absolute;
            top: 3px;
            left: 3px;
        }
    )";
    
    this->setStyleSheet(styleSheet);
    qDebug() << "统一样式表已应用";
}

/**
 * @brief 显示诊断信息
 * @param diagnosticInfo 诊断信息文本
 */
void Dialog::showDiagnosticInfo(const QString& diagnosticInfo)
{
    // 在图像显示区域显示诊断信息
    m_imageDisplayLabel->setText(diagnosticInfo);
    
    // 设置文本对齐方式为左上角对齐，便于阅读长文本
    m_imageDisplayLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    
    // 设置文本自动换行
    m_imageDisplayLabel->setWordWrap(true);
    
    // 设置字体为等宽字体，保持格式对齐
    QFont font("Consolas, Monaco, monospace");
    font.setPointSize(9);
    m_imageDisplayLabel->setFont(font);
    
    // 设置背景色为浅灰色，便于阅读
    m_imageDisplayLabel->setStyleSheet(
        "QLabel {"
        "    background-color: #f5f5f5;"
        "    border: 1px solid #ddd;"
        "    padding: 10px;"
        "    color: #333;"
        "}"
    );
    
    qDebug() << "诊断信息已显示在界面上";
}

/**
 * @brief 创建图像缩放控制面板
 * @return 缩放控制面板布局
 */
QLayout* Dialog::createZoomControlPanel()
{
    QGroupBox* zoomGroup = new QGroupBox("🔍 图像缩放控制");
    QHBoxLayout* zoomLayout = new QHBoxLayout(zoomGroup);
    
    // 缩放按钮组
    m_zoomOutBtn = new QPushButton("🔍-");
    m_zoomOutBtn->setFixedSize(35, 25);
    m_zoomOutBtn->setToolTip("缩小图像 (Ctrl+-)");
    zoomLayout->addWidget(m_zoomOutBtn);
    
    // 缩放滑块
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(10, 500);  // 10% 到 500%
    m_zoomSlider->setValue(100);      // 默认100%
    m_zoomSlider->setFixedWidth(150);
    m_zoomSlider->setToolTip("拖动调整缩放比例 (10%-500%)");
    zoomLayout->addWidget(m_zoomSlider);
    
    m_zoomInBtn = new QPushButton("🔍+");
    m_zoomInBtn->setFixedSize(35, 25);
    m_zoomInBtn->setToolTip("放大图像 (Ctrl++)");
    zoomLayout->addWidget(m_zoomInBtn);
    
    // 缩放比例标签
    m_zoomLabel = new QLabel("100%");
    m_zoomLabel->setFixedWidth(50);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setStyleSheet("QLabel { font-weight: bold; color: #2196F3; }");
    zoomLayout->addWidget(m_zoomLabel);
    
    // 预设缩放按钮
    m_fitWindowBtn = new QPushButton("📐 适应窗口");
    m_fitWindowBtn->setCheckable(true);
    m_fitWindowBtn->setChecked(true);  // 默认适应窗口
    m_fitWindowBtn->setToolTip("自动调整图像大小以适应窗口");
    zoomLayout->addWidget(m_fitWindowBtn);
    
    m_actualSizeBtn = new QPushButton("📏 实际大小");
    m_actualSizeBtn->setToolTip("显示图像的实际像素大小 (100%)");
    zoomLayout->addWidget(m_actualSizeBtn);
    
    // 添加弹性空间
    zoomLayout->addStretch();
    
    // 连接信号槽
    connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int value) {
        setZoomFactor(value / 100.0);
    });
    
    connect(m_zoomInBtn, &QPushButton::clicked, this, &Dialog::zoomIn);
    connect(m_zoomOutBtn, &QPushButton::clicked, this, &Dialog::zoomOut);
    connect(m_fitWindowBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_fitToWindow = checked;
        if (checked) {
            fitImageToWindow();
        }
    });
    connect(m_actualSizeBtn, &QPushButton::clicked, this, &Dialog::showActualSize);
    
    QVBoxLayout* panelLayout = new QVBoxLayout();
    panelLayout->addWidget(zoomGroup);
    
    return panelLayout;
}

/**
 * @brief 更新图像显示
 * @param pixmap 要显示的图像
 */
void Dialog::updateImageDisplay(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return;
    }
    
    QPixmap scaledPixmap;
    
    if (m_fitToWindow) {
        // 适应窗口模式：根据滚动区域大小自动缩放
        fitImageToWindow();
    } else {
        // 固定缩放模式：按当前缩放因子显示
        scaleImage(m_currentZoomFactor);
    }
}

/**
 * @brief 缩放图像到指定因子
 * @param factor 缩放因子
 */
void Dialog::scaleImage(double factor)
{
    if (m_originalPixmap.isNull()) {
        return;
    }
    
    m_currentZoomFactor = factor;
    
    // 计算缩放后的尺寸
    QSize scaledSize = m_originalPixmap.size() * factor;
    
    // 缩放图像
    QPixmap scaledPixmap = m_originalPixmap.scaled(scaledSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // 更新显示标签
    m_imageDisplayLabel->setPixmap(scaledPixmap);
    m_imageDisplayLabel->resize(scaledSize);
    
    // 更新缩放控件状态
    updateZoomControls();
    
    qDebug() << QString("图像已缩放到 %1%，尺寸：%2x%3")
                .arg(factor * 100, 0, 'f', 1)
                .arg(scaledSize.width())
                .arg(scaledSize.height());
}

/**
 * @brief 适应窗口大小显示图像
 */
void Dialog::fitImageToWindow()
{
    if (m_originalPixmap.isNull() || !m_imageScrollArea) {
        return;
    }
    
    // 获取滚动区域的可用空间（减去滚动条和边距）
    QSize availableSize = m_imageScrollArea->viewport()->size();
    QSize imageSize = m_originalPixmap.size();
    
    // 计算适应窗口的缩放因子
    double scaleX = static_cast<double>(availableSize.width()) / imageSize.width();
    double scaleY = static_cast<double>(availableSize.height()) / imageSize.height();
    double scaleFactor = qMin(scaleX, scaleY);
    
    // 限制最小和最大缩放因子
    scaleFactor = qMax(0.1, qMin(5.0, scaleFactor));
    
    // 应用缩放
    m_currentZoomFactor = scaleFactor;
    scaleImage(scaleFactor);
    
    qDebug() << QString("图像已适应窗口，缩放因子：%1").arg(scaleFactor);
}

/**
 * @brief 显示图像实际大小
 */
void Dialog::showActualSize()
{
    m_fitToWindow = false;
    m_fitWindowBtn->setChecked(false);
    setZoomFactor(1.0);
}

/**
 * @brief 放大图像
 */
void Dialog::zoomIn()
{
    m_fitToWindow = false;
    m_fitWindowBtn->setChecked(false);
    
    double newFactor = m_currentZoomFactor * 1.25;  // 每次放大25%
    newFactor = qMin(5.0, newFactor);  // 最大500%
    
    setZoomFactor(newFactor);
}

/**
 * @brief 缩小图像
 */
void Dialog::zoomOut()
{
    m_fitToWindow = false;
    m_fitWindowBtn->setChecked(false);
    
    double newFactor = m_currentZoomFactor / 1.25;  // 每次缩小25%
    newFactor = qMax(0.1, newFactor);  // 最小10%
    
    setZoomFactor(newFactor);
}

/**
 * @brief 设置缩放因子
 * @param factor 缩放因子
 */
void Dialog::setZoomFactor(double factor)
{
    factor = qMax(0.1, qMin(5.0, factor));  // 限制在10%-500%之间
    
    if (qAbs(factor - m_currentZoomFactor) < 0.01) {
        return;  // 变化太小，不需要更新
    }
    
    scaleImage(factor);
}

/**
 * @brief 更新缩放控件状态
 */
void Dialog::updateZoomControls()
{
    if (!m_zoomSlider || !m_zoomLabel) {
        return;
    }
    
    // 更新滑块位置（避免触发信号）
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue(static_cast<int>(m_currentZoomFactor * 100));
    m_zoomSlider->blockSignals(false);
    
    // 更新缩放比例标签
    m_zoomLabel->setText(QString("%1%").arg(static_cast<int>(m_currentZoomFactor * 100)));
    
    // 更新按钮状态
    if (m_zoomInBtn) {
        m_zoomInBtn->setEnabled(m_currentZoomFactor < 5.0);
    }
    if (m_zoomOutBtn) {
        m_zoomOutBtn->setEnabled(m_currentZoomFactor > 0.1);
    }
}

/**
 * @brief 窗口大小调整事件
 * @param event 调整大小事件
 * 
 * 在窗口大小改变时被调用。如果启用了"适应窗口"模式，
 * 它会使用一个防抖动定时器来平滑地调整图像大小，避免在快速拖动时性能下降。
 */
void Dialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    
    // 如果处于适应窗口模式，重新调整图像大小
    if (m_fitToWindow && !m_originalPixmap.isNull()) {
        // 使用一个 ngắn (e.g., 50ms) 的定时器来延迟缩放操作。
        // 这可以防止在用户连续拖动窗口大小时过于频繁地调用fitImageToWindow，
        // 从而获得更平滑的用户体验，并避免性能问题。
        // 如果已经有一个定时器在等待，我们会先停止它。
        if (m_resizeTimer->isActive()) {
            m_resizeTimer->stop();
        }
        m_resizeTimer->start();
   }
}

/**
 * @brief 创建服务器连接面板
 * @return 服务器连接面板布局
 */
QLayout* Dialog::createServerConnectionPanel()
{
    QGroupBox* connectionGroup = new QGroupBox("🔗 服务器连接");
    QHBoxLayout* connectionLayout = new QHBoxLayout(connectionGroup);
    
    // 创建现代化连接控件
    m_serverIPEdit = new QLineEdit("192.168.1.31");
    m_serverPortEdit = new QLineEdit("17777");
    m_connectBtn = new QPushButton("🔗 开始连接");
    
    // 设置控件属性
    m_serverIPEdit->setPlaceholderText("服务器IP地址");
    m_serverPortEdit->setPlaceholderText("端口号");
    m_serverIPEdit->setToolTip("请输入服务器的IP地址");
    m_serverPortEdit->setToolTip("请输入服务器的端口号 (1-65535)");
    
    // 设置连接按钮样式
    m_connectBtn->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px 16px; border-radius: 4px; min-width: 100px; }");
    
    // 布局安排
    connectionLayout->addWidget(new QLabel("服务器IP:"));
    connectionLayout->addWidget(m_serverIPEdit);
    connectionLayout->addWidget(new QLabel("端口:"));
    connectionLayout->addWidget(m_serverPortEdit);
    connectionLayout->addWidget(m_connectBtn);
    connectionLayout->addStretch(); // 添加弹性空间
    
    // 连接信号
    connect(m_connectBtn, &QPushButton::clicked, [this]() {
        QString ipAddress = m_serverIPEdit->text().trimmed();
        QString portText = m_serverPortEdit->text().trimmed();
        
        // 输入验证
        if (ipAddress.isEmpty()) {
            qDebug() << "错误：请输入服务器IP地址";
            m_imageDisplayLabel->setText("❌ 连接失败：请输入服务器IP地址");
            return;
        }
        
        if (portText.isEmpty()) {
            qDebug() << "错误：请输入服务器端口号";
            m_imageDisplayLabel->setText("❌ 连接失败：请输入服务器端口号");
            return;
        }
        
        // 端口号转换和验证
        bool ok;
        int port = portText.toInt(&ok);
        if (!ok || port <= 0 || port > 65535) {
            qDebug() << "错误：端口号格式不正确，请输入1-65535范围内的数字";
            m_imageDisplayLabel->setText("❌ 连接失败：端口号无效\n请输入1-65535范围内的数字");
            return;
        }
        
        // 显示连接状态信息
        m_imageDisplayLabel->setText(QString("🔄 正在连接到服务器...\n\nIP：%1\n端口：%2\n\n请稍候...").arg(ipAddress).arg(port));
        m_connectBtn->setEnabled(false);  // 防止重复点击
        
        qDebug() << "用户发起连接请求：" << ipAddress << ":" << port;
        
        // 启动TCP连接
        m_tcpImg.slot_disconnect(); // 先断开现有连接
        
        // 启用自动重连（如果勾选了自动重连）
        if (m_autoReconnectCheckBox && m_autoReconnectCheckBox->isChecked()) {
            m_tcpImg.setAutoReconnect(true, 5, 3000);
        }
        
        m_tcpImg.start(ipAddress, port);
        
        // 3秒后重新启用按钮，防止界面卡住
        QTimer::singleShot(3000, this, [this]() {
            if (m_connectBtn) {
                m_connectBtn->setEnabled(true);
            }
        });
    });
    
    QVBoxLayout* layout = new QVBoxLayout();
    layout->addWidget(connectionGroup);
    return layout;
}

/**
 * @brief 切换控制面板的可见性
 *
 * 当用户点击"隐藏/显示控件"按钮时被调用。
 * 这个函数会切换所有控制面板的可见状态，并更新按钮的文本和图标，
 * 以提供清晰的视觉反馈。
 */
void Dialog::toggleControlsVisibility()
{
    m_controlsVisible = !m_controlsVisible; // 切换状态

    if (m_controlsContainer) {
        m_controlsContainer->setVisible(m_controlsVisible);
    }

    if (m_toggleControlsBtn) {
        if (m_controlsVisible) {
            m_toggleControlsBtn->setText("🔼 隐藏控件");
            m_toggleControlsBtn->setToolTip("点击隐藏所有控制面板");
        } else {
            m_toggleControlsBtn->setText("🔽 显示控件");
            m_toggleControlsBtn->setToolTip("点击显示所有控制面板");
        }
   }
}
