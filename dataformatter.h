#ifndef DATAFORMATTER_H
#define DATAFORMATTER_H

#include <QString>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

/**
 * @class CDataFormatter
 * @brief 数据格式化处理类
 * 
 * 提供多种数据显示格式的转换功能，支持：
 * - 原始文本显示
 * - 十六进制显示
 * - 二进制显示
 * - ASCII码显示
 * - JSON格式化显示
 * - 数据统计分析
 */
class CDataFormatter
{
public:
    /**
     * @enum DataDisplayFormat
     * @brief 数据显示格式枚举
     */
    enum DataDisplayFormat {
        FORMAT_RAW_TEXT,     ///< 原始文本格式
        FORMAT_HEX,          ///< 十六进制格式
        FORMAT_BINARY,       ///< 二进制格式
        FORMAT_ASCII,        ///< ASCII码格式
        FORMAT_JSON,         ///< JSON格式
        FORMAT_MIXED         ///< 混合格式显示
    };

    /**
     * @brief 构造函数
     */
    CDataFormatter();

    /**
     * @brief 析构函数
     */
    ~CDataFormatter();

    /**
     * @brief 格式化数据为指定格式
     * @param data 原始数据
     * @param format 目标格式
     * @param timestamp 是否添加时间戳
     * @return 格式化后的字符串
     */
    QString formatData(const QByteArray& data, DataDisplayFormat format, bool timestamp = true);

    /**
     * @brief 将数据格式化为十六进制显示
     * @param data 原始数据
     * @param bytesPerLine 每行显示的字节数
     * @param showAddress 是否显示地址
     * @return 十六进制格式字符串
     */
    QString toHexFormat(const QByteArray& data, int bytesPerLine = 16, bool showAddress = true);

    /**
     * @brief 将数据格式化为二进制显示
     * @param data 原始数据
     * @param bitsPerByte 每字节显示的位数分组
     * @return 二进制格式字符串
     */
    QString toBinaryFormat(const QByteArray& data, int bitsPerByte = 4);

    /**
     * @brief 将数据格式化为ASCII码显示
     * @param data 原始数据
     * @param showNonPrintable 是否显示不可打印字符
     * @return ASCII格式字符串
     */
    QString toAsciiFormat(const QByteArray& data, bool showNonPrintable = true);

    /**
     * @brief 尝试将数据格式化为JSON显示
     * @param data 原始数据
     * @return JSON格式字符串，如果不是有效JSON则返回原始文本
     */
    QString toJsonFormat(const QByteArray& data);

    /**
     * @brief 生成数据统计信息
     * @param data 原始数据
     * @return 统计信息字符串
     */
    QString generateDataStatistics(const QByteArray& data);

    /**
     * @brief 获取当前时间戳字符串
     * @return 格式化的时间戳
     */
    QString getCurrentTimestamp();

    /**
     * @brief 检测数据的可能格式
     * @param data 原始数据
     * @return 建议的显示格式
     */
    DataDisplayFormat detectDataFormat(const QByteArray& data);

    /**
     * @brief 格式名称转换
     * @param format 格式枚举
     * @return 格式的中文名称
     */
    QString formatToString(DataDisplayFormat format);

private:
    /**
     * @brief 判断字符是否为可打印字符
     * @param ch 字符
     * @return true如果可打印
     */
    bool isPrintableChar(char ch);

    /**
     * @brief 替换不可打印字符
     * @param ch 字符
     * @return 替换后的字符串表示
     */
    QString replaceNonPrintable(char ch);

    /**
     * @brief 检查数据是否为有效的JSON
     * @param data 数据
     * @return true如果是有效JSON
     */
    bool isValidJson(const QByteArray& data);

    /**
     * @brief 检查数据是否为纯文本
     * @param data 数据
     * @return true如果是纯文本
     */
    bool isPlainText(const QByteArray& data);
};

#endif // DATAFORMATTER_H 