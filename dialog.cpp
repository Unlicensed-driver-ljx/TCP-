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
    ui(new Ui::Dialog)
{
    // 设置用户界面
    ui->setupUi(this);
    
    // 设置窗口标题
    this->setWindowTitle("TCP图像传输接收程序 - 集成网络调试工具");
    
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
    
    // 添加连接状态监控（通过定时器或其他方式）
    // 这里可以添加更多的状态监控逻辑
    
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
    
    // 将接收到的图像数据复制到显示缓冲区
    try {
        memcpy(m_showBuffer, frameBuffer, WIDTH * HEIGHT * CHANLE);
    } catch (const std::exception& e) {
        qDebug() << "错误：图像数据复制失败：" << e.what();
        ui->labelShowImg->setText("错误：图像数据处理失败");
        ui->pushButtonStart->setEnabled(true);
        return;
    }
    
    // 创建QImage对象进行图像格式转换
    // 注意：这里使用灰度格式，如果需要RGB格式，请修改为Format_RGB888
    m_qimage = QImage((const unsigned char*)(m_showBuffer), WIDTH, HEIGHT, QImage::Format_Grayscale8);
    
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
    }
    else {
        // 图像创建失败
        qDebug() << "错误：QImage对象创建失败，可能是图像数据格式不正确";
        qDebug() << "图像参数：宽度=" << WIDTH << "，高度=" << HEIGHT << "，通道数=" << CHANLE;
        
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
    
    connect(m_dataFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &Dialog::onDataFormatChanged);
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
