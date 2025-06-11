#-------------------------------------------------
#
# Project created by QtCreator 2022-05-21T21:28:39
#
#-------------------------------------------------

QT       += core gui
QT       += network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = TCPImg
TEMPLATE = app

# Qt 5.12兼容性设置
DEFINES += QT_DEPRECATED_WARNINGS

# 确保支持Qt 5.12及以上版本
lessThan(QT_MAJOR_VERSION, 5) {
    error("This project requires Qt 5.12 or later")
}

lessThan(QT_MAJOR_VERSION, 5) | lessThan(QT_MINOR_VERSION, 12) {
    # 如果Qt版本小于5.12，可能需要一些特殊处理
    message("Qt version is less than 5.12, some features may not be available")
}

# 为Qt 5.12设置C++标准
CONFIG += c++11


SOURCES += \
        main.cpp \
        dialog.cpp \
        ctcpimg.cpp \
        dataformatter.cpp \
        tcpdebugger.cpp

HEADERS += \
        dialog.h \
        ctcpimg.h \
        sysdefine.h \
        dataformatter.h \
        tcpdebugger.h

FORMS += \
        dialog.ui
