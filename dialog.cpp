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
    ui(new Ui::Dialog),
    m_reconnectBtn(nullptr),
    m_autoReconnectCheckBox(nullptr),
    m_connectionStatusLabel(nullptr)
{
    // 设置用户界面
    ui->setupUi(this);
    
    // 设置窗口标题
    this->setWindowTitle("TCP图像传输接收程序 - 集成网络调试工具");
    
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
    
    // 连接开始按钮的点击事件
    connect(ui->pushButtonStart, &QPushButton::clicked, [=]()
    {
        // 获取用户输入的IP地址和端口号
        QString ipAddress = ui->lineEditAddr->text().trimmed();
        QString portText = ui->lineEditPort->text().trimmed();
        
        // 输入验证
        if (ipAddress.isEmpty()) {
            qDebug() << "错误：请输入服务器IP地址";
            ui->labelShowImg->setText("错误：请输入服务器IP地址");
            return;
        }
        
        if (portText.isEmpty()) {
            qDebug() << "错误：请输入服务器端口号";
            ui->labelShowImg->setText("错误：请输入服务器端口号");
            return;
        }
        
        // 端口号转换和验证
        bool ok;
        int port = portText.toInt(&ok);
        if (!ok || port <= 0 || port > 65535) {
            qDebug() << "错误：端口号格式不正确，请输入1-65535范围内的数字";
            ui->labelShowImg->setText("错误：端口号格式不正确\n请输入1-65535范围内的数字");
            return;
        }
        
        // 显示连接状态信息
        ui->labelShowImg->setText(QString("正在连接到 %1:%2...\n请等待连接建立").arg(ipAddress).arg(port));
        ui->pushButtonStart->setEnabled(false);  // 防止重复点击
        
        qDebug() << "用户发起连接请求：" << ipAddress << ":" << port;
        
        // 启动TCP连接
        m_tcpImg.start(ipAddress, port);
    });

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
        ui->labelShowImg->setText("严重错误：内存分配失败\n程序可能无法正常工作");
        return;
    }
    
    // 初始化显示缓冲区为0
    memset((void*)m_showBuffer, 0, WIDTH * HEIGHT * CHANLE);
    
    // 设置标签的初始显示文本
    ui->labelShowImg->setText("TCP图像传输接收程序已启动\n\n请输入服务器地址和端口号，然后点击开始连接\n\n默认配置：\nIP：192.168.1.31\n端口：17777");
    ui->labelShowImg->setAlignment(Qt::AlignCenter);  // 居中显示文本
    
    qDebug() << "Dialog界面初始化完成，图像缓冲区大小：" << (WIDTH * HEIGHT * CHANLE) << "字节";
}

/**
 * @brief Dialog析构函数
 * 
 * 清理UI资源和释放图像显示缓冲区内存
 */
Dialog::~Dialog()
{
    // 释放UI资源
    delete ui;
    
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
        ui->labelShowImg->setText("错误：无法获取图像数据");
        ui->pushButtonStart->setEnabled(true);  // 重新启用开始按钮
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
        ui->labelShowImg->setText("错误：图像数据处理失败");
        ui->pushButtonStart->setEnabled(true);
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
            ui->labelShowImg->setText("错误：多通道图像处理失败");
            ui->pushButtonStart->setEnabled(true);
            return;
        }
    }
    
    // 检查QImage对象是否创建成功
    if (!m_qimage.isNull()) {
        // 图像创建成功，更新显示
        QPixmap pixmap = QPixmap::fromImage(m_qimage);
        
        // 如果图像尺寸大于标签尺寸，进行缩放以适应显示
        QSize labelSize = ui->labelShowImg->size();
        if (pixmap.size().width() > labelSize.width() || pixmap.size().height() > labelSize.height()) {
            pixmap = pixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            qDebug() << "图像已缩放以适应显示区域：" << labelSize;
        }
        
        // 设置图像到标签
        ui->labelShowImg->setPixmap(pixmap);
        
        // 重新启用开始按钮，允许用户重新连接
        ui->pushButtonStart->setEnabled(true);
        
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
        ui->pushButtonStart->setEnabled(true);
    }
    else {
        // 图像创建失败
        qDebug() << "错误：QImage对象创建失败，可能是图像数据格式不正确";
        qDebug() << "图像参数：宽度=" << width << "，高度=" << height << "，通道数=" << channels;
        
        // 显示错误信息给用户
        ui->labelShowImg->setText("错误：图像数据格式不正确\n\n可能原因：\n1. 图像数据损坏\n2. 数据格式不匹配\n3. 网络传输错误\n\n请检查服务器端图像格式设置");
        
        // 重新启用开始按钮
        ui->pushButtonStart->setEnabled(true);
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
    
    // 创建图像传输标签页（使用当前UI）
    m_imageTab = new QWidget();
    QVBoxLayout* imageLayout = new QVBoxLayout(m_imageTab);
    
    // 将原有的UI控件添加到图像标签页
    // 保留原有UI的完整布局
    QWidget* imageWidget = new QWidget();
    QVBoxLayout* originalLayout = new QVBoxLayout(imageWidget);
    
    // 添加输入控件组
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel("服务器IP:"));
    inputLayout->addWidget(ui->lineEditAddr);
    inputLayout->addWidget(new QLabel("端口:"));
    inputLayout->addWidget(ui->lineEditPort);
    inputLayout->addWidget(ui->pushButtonStart);
    
    originalLayout->addLayout(inputLayout);
    
    // 添加分辨率设置面板
    originalLayout->addLayout(createResolutionPanel());
    
    // 添加重连控制面板
    originalLayout->addLayout(createReconnectPanel());
    
    originalLayout->addWidget(ui->labelShowImg, 1);  // 图像显示区占主要空间
    
    imageLayout->addWidget(imageWidget);
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
        ui->labelShowImg->setText("错误：图像宽度格式不正确");
        return;
    }
    
    if (!heightOk || height <= 0) {
        ui->labelShowImg->setText("错误：图像高度格式不正确");
        return;
    }
    
    // 计算内存大小并提醒用户
    long long totalBytes = (long long)width * height * channels;
    if (totalBytes > 50 * 1024 * 1024) {
        ui->labelShowImg->setText(QString("错误：图像数据过大\n需要 %1 MB 内存，超过50MB限制")
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
             
             ui->labelShowImg->setText(QString("✅ 分辨率设置成功\n\n新设置：%1 x %2 x %3\n格式：8bit %4\n内存占用：%5 MB\n\n准备接收新的图像数据...")
                                       .arg(width).arg(height).arg(channels)
                                       .arg(channelInfo)
                                       .arg(totalBytes / 1024.0 / 1024.0, 0, 'f', 2));
                                      
            qDebug() << "分辨率设置成功：" << width << "x" << height << "x" << channels;
            
        } catch (const std::bad_alloc&) {
            ui->labelShowImg->setText("错误：显示缓冲区内存分配失败");
            m_showBuffer = nullptr;
        }
    } else {
        ui->labelShowImg->setText("错误：分辨率设置失败\n请检查输入参数");
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
    
    ui->labelShowImg->setText("✅ 已重置为默认分辨率\n\n准备接收图像数据...");
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
    ui->labelShowImg->setText(QString("✅ 已应用分辨率预设：%1\n\n准备接收图像数据...").arg(presetName));
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
    ui->labelShowImg->setText("🔍 正在执行服务端诊断检查...\n\n请稍候，正在检测网络连通性和服务端状态...");
    
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
    ui->labelShowImg->setText(diagnosticInfo);
    
    // 设置文本对齐方式为左上角对齐，便于阅读长文本
    ui->labelShowImg->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    
    // 设置文本自动换行
    ui->labelShowImg->setWordWrap(true);
    
    // 设置字体为等宽字体，保持格式对齐
    QFont font("Consolas, Monaco, monospace");
    font.setPointSize(9);
    ui->labelShowImg->setFont(font);
    
    // 设置背景色为浅灰色，便于阅读
    ui->labelShowImg->setStyleSheet(
        "QLabel {"
        "    background-color: #f5f5f5;"
        "    border: 1px solid #ddd;"
        "    padding: 10px;"
        "    color: #333;"
        "}"
    );
    
    qDebug() << "诊断信息已显示在界面上";
}
