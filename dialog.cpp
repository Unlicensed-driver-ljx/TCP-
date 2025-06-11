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
    this->setWindowTitle("TCP图像传输接收程序 - 新版本");  // 正确的修改，要保留
    
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
