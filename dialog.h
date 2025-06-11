#ifndef DIALOG_H
#define DIALOG_H

#include <QDialog>
#include <ctcpimg.h>
#include <QImage>
#include "sysdefine.h"

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

private:
    Ui::Dialog *ui;          ///< UI界面指针，由Qt设计器生成的界面对象
    CTCPImg m_tcpImg;        ///< TCP图像传输对象，处理网络通信和数据接收
    char* m_showBuffer;      ///< 图像显示缓冲区，用于临时存储要显示的图像数据
    QImage m_qimage;         ///< Qt图像对象，用于图像格式转换和显示处理
};

#endif // DIALOG_H
