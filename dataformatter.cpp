#include "dataformatter.h"
#include <QDebug>
#include <QJsonParseError>
#include <QRegularExpression>
#include <cmath>

/**
 * @brief 构造函数
 */
CDataFormatter::CDataFormatter()
{
    qDebug() << "数据格式化器初始化完成";
}

/**
 * @brief 析构函数
 */
CDataFormatter::~CDataFormatter()
{
    qDebug() << "数据格式化器销毁";
}

/**
 * @brief 格式化数据为指定格式
 * @param data 原始数据
 * @param format 目标格式
 * @param timestamp 是否添加时间戳
 * @return 格式化后的字符串
 */
QString CDataFormatter::formatData(const QByteArray& data, DataDisplayFormat format, bool timestamp)
{
    QString result;
    QString timeStr = timestamp ? getCurrentTimestamp() : "";
    
    switch (format) {
        case FORMAT_RAW_TEXT:
            result = QString::fromUtf8(data);
            break;
            
        case FORMAT_HEX:
            result = toHexFormat(data);
            break;
            
        case FORMAT_BINARY:
            result = toBinaryFormat(data);
            break;
            
        case FORMAT_ASCII:
            result = toAsciiFormat(data);
            break;
            
        case FORMAT_JSON:
            result = toJsonFormat(data);
            break;
            
        case FORMAT_MIXED:
            result = QString("=== 原始文本 ===\n%1\n\n=== 十六进制 ===\n%2\n\n=== 统计信息 ===\n%3")
                    .arg(QString::fromUtf8(data))
                    .arg(toHexFormat(data, 16, false))
                    .arg(generateDataStatistics(data));
            break;
            
        default:
            result = QString::fromUtf8(data);
            break;
    }
    
    if (timestamp && !timeStr.isEmpty()) {
        result = QString("[%1] %2").arg(timeStr, result);
    }
    
    return result;
}

/**
 * @brief 将数据格式化为十六进制显示
 * @param data 原始数据
 * @param bytesPerLine 每行显示的字节数
 * @param showAddress 是否显示地址
 * @return 十六进制格式字符串
 */
QString CDataFormatter::toHexFormat(const QByteArray& data, int bytesPerLine, bool showAddress)
{
    QString result;
    
    for (int i = 0; i < data.size(); i += bytesPerLine) {
        QString line;
        
        // 添加地址
        if (showAddress) {
            line += QString("%1: ").arg(i, 8, 16, QChar('0')).toUpper();
        }
        
        // 十六进制部分
        QString hexPart;
        QString asciiPart;
        
        for (int j = 0; j < bytesPerLine && (i + j) < data.size(); ++j) {
            unsigned char byte = static_cast<unsigned char>(data[i + j]);
            hexPart += QString("%1 ").arg(byte, 2, 16, QChar('0')).toUpper();
            
            // ASCII部分
            if (isPrintableChar(byte)) {
                asciiPart += QChar(byte);
            } else {
                asciiPart += '.';
            }
        }
        
        // 补齐空格
        while (hexPart.length() < bytesPerLine * 3) {
            hexPart += "   ";
        }
        
        line += hexPart + " |" + asciiPart + "|";
        result += line + "\n";
    }
    
    return result;
}

/**
 * @brief 将数据格式化为二进制显示
 * @param data 原始数据
 * @param bitsPerByte 每字节显示的位数分组
 * @return 二进制格式字符串
 */
QString CDataFormatter::toBinaryFormat(const QByteArray& data, int bitsPerByte)
{
    QString result;
    
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        QString binaryStr;
        
        for (int bit = 7; bit >= 0; --bit) {
            binaryStr += (byte & (1 << bit)) ? '1' : '0';
            
            // 按位分组
            if (bitsPerByte == 4 && bit == 4) {
                binaryStr += ' ';
            }
        }
        
        result += QString("字节%1: %2 (0x%3, %4)\n")
                 .arg(i, 3, 10, QChar(' '))
                 .arg(binaryStr)
                 .arg(byte, 2, 16, QChar('0')).toUpper()
                 .arg(byte);
    }
    
    return result;
}

/**
 * @brief 将数据格式化为ASCII码显示
 * @param data 原始数据
 * @param showNonPrintable 是否显示不可打印字符
 * @return ASCII格式字符串
 */
QString CDataFormatter::toAsciiFormat(const QByteArray& data, bool showNonPrintable)
{
    QString result;
    
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        
        if (isPrintableChar(byte)) {
            result += QString("字节%1: '%2' (ASCII: %3, HEX: 0x%4)\n")
                     .arg(i, 3, 10, QChar(' '))
                     .arg(QChar(byte))
                     .arg(byte)
                     .arg(byte, 2, 16, QChar('0')).toUpper();
        } else if (showNonPrintable) {
            result += QString("字节%1: %2 (ASCII: %3, HEX: 0x%4)\n")
                     .arg(i, 3, 10, QChar(' '))
                     .arg(replaceNonPrintable(byte))
                     .arg(byte)
                     .arg(byte, 2, 16, QChar('0')).toUpper();
        }
    }
    
    return result;
}

/**
 * @brief 尝试将数据格式化为JSON显示
 * @param data 原始数据
 * @return JSON格式字符串，如果不是有效JSON则返回原始文本
 */
QString CDataFormatter::toJsonFormat(const QByteArray& data)
{
    QJsonParseError error;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &error);
    
    if (error.error == QJsonParseError::NoError) {
        // 是有效的JSON，进行格式化
        return jsonDoc.toJson(QJsonDocument::Indented);
    } else {
        // 不是有效JSON，返回解析错误信息和原始数据
        return QString("JSON解析错误：%1\n原始数据：\n%2")
               .arg(error.errorString())
               .arg(QString::fromUtf8(data));
    }
}

/**
 * @brief 生成数据统计信息
 * @param data 原始数据
 * @return 统计信息字符串
 */
QString CDataFormatter::generateDataStatistics(const QByteArray& data)
{
    if (data.isEmpty()) {
        return "数据为空";
    }
    
    // 基本统计
    int totalBytes = data.size();
    int printableChars = 0;
    int controlChars = 0;
    int nullBytes = 0;
    
    // 字符频率统计
    QMap<unsigned char, int> charFreq;
    
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        charFreq[byte]++;
        
        if (byte == 0) {
            nullBytes++;
        } else if (isPrintableChar(byte)) {
            printableChars++;
        } else {
            controlChars++;
        }
    }
    
    // 计算熵值（简化版）
    double entropy = 0.0;
    for (auto it = charFreq.begin(); it != charFreq.end(); ++it) {
        double probability = static_cast<double>(it.value()) / totalBytes;
        if (probability > 0) {
            entropy -= probability * log2(probability);
        }
    }
    
    QString result;
    result += QString("数据大小：%1 字节\n").arg(totalBytes);
    result += QString("可打印字符：%1 (%2%)\n")
              .arg(printableChars)
              .arg(printableChars * 100.0 / totalBytes, 0, 'f', 1);
    result += QString("控制字符：%1 (%2%)\n")
              .arg(controlChars)
              .arg(controlChars * 100.0 / totalBytes, 0, 'f', 1);
    result += QString("空字节：%1 (%2%)\n")
              .arg(nullBytes)
              .arg(nullBytes * 100.0 / totalBytes, 0, 'f', 1);
    result += QString("唯一字节数：%1\n").arg(charFreq.size());
    result += QString("数据熵值：%1\n").arg(entropy, 0, 'f', 2);
    result += QString("建议格式：%1\n").arg(formatToString(detectDataFormat(data)));
    
    return result;
}

/**
 * @brief 获取当前时间戳字符串
 * @return 格式化的时间戳
 */
QString CDataFormatter::getCurrentTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

/**
 * @brief 检测数据的可能格式
 * @param data 原始数据
 * @return 建议的显示格式
 */
CDataFormatter::DataDisplayFormat CDataFormatter::detectDataFormat(const QByteArray& data)
{
    if (data.isEmpty()) {
        return FORMAT_RAW_TEXT;
    }
    
    // 检查是否为JSON
    if (isValidJson(data)) {
        return FORMAT_JSON;
    }
    
    // 检查是否为纯文本
    if (isPlainText(data)) {
        return FORMAT_RAW_TEXT;
    }
    
    // 如果包含很多二进制数据，建议十六进制显示
    int binaryBytes = 0;
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        if (!isPrintableChar(byte) && byte != '\n' && byte != '\r' && byte != '\t') {
            binaryBytes++;
        }
    }
    
    if (binaryBytes > data.size() * 0.3) {  // 如果超过30%是二进制数据
        return FORMAT_HEX;
    }
    
    return FORMAT_RAW_TEXT;
}

/**
 * @brief 格式名称转换
 * @param format 格式枚举
 * @return 格式的中文名称
 */
QString CDataFormatter::formatToString(DataDisplayFormat format)
{
    switch (format) {
        case FORMAT_RAW_TEXT: return "原始文本";
        case FORMAT_HEX: return "十六进制";
        case FORMAT_BINARY: return "二进制";
        case FORMAT_ASCII: return "ASCII码";
        case FORMAT_JSON: return "JSON格式";
        case FORMAT_MIXED: return "混合显示";
        default: return "未知格式";
    }
}

/**
 * @brief 判断字符是否为可打印字符
 * @param ch 字符
 * @return true如果可打印
 */
bool CDataFormatter::isPrintableChar(char ch)
{
    unsigned char byte = static_cast<unsigned char>(ch);
    return (byte >= 32 && byte <= 126) || byte >= 128;  // ASCII可打印字符或扩展ASCII
}

/**
 * @brief 替换不可打印字符
 * @param ch 字符
 * @return 替换后的字符串表示
 */
QString CDataFormatter::replaceNonPrintable(char ch)
{
    unsigned char byte = static_cast<unsigned char>(ch);
    
    switch (byte) {
        case 0: return "[NULL]";
        case 7: return "[BEL]";
        case 8: return "[BS]";
        case 9: return "[TAB]";
        case 10: return "[LF]";
        case 11: return "[VT]";
        case 12: return "[FF]";
        case 13: return "[CR]";
        case 27: return "[ESC]";
        case 127: return "[DEL]";
        default:
            if (byte < 32) {
                return QString("[CTRL+%1]").arg(QChar('A' + byte - 1));
            } else {
                return QString("[0x%1]").arg(byte, 2, 16, QChar('0')).toUpper();
            }
    }
}

/**
 * @brief 检查数据是否为有效的JSON
 * @param data 数据
 * @return true如果是有效JSON
 */
bool CDataFormatter::isValidJson(const QByteArray& data)
{
    if (data.isEmpty()) {
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument::fromJson(data, &error);
    return error.error == QJsonParseError::NoError;
}

/**
 * @brief 检查数据是否为纯文本
 * @param data 数据
 * @return true如果是纯文本
 */
bool CDataFormatter::isPlainText(const QByteArray& data)
{
    if (data.isEmpty()) {
        return true;
    }
    
    int printableCount = 0;
    for (int i = 0; i < data.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(data[i]);
        if (isPrintableChar(byte) || byte == '\n' || byte == '\r' || byte == '\t') {
            printableCount++;
        }
    }
    
    // 如果90%以上是可打印字符，认为是纯文本
    return printableCount >= data.size() * 0.9;
} 