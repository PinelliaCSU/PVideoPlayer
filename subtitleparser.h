#ifndef SUBTITLEPARSER_H
#define SUBTITLEPARSER_H

#include <QString>
#include <QVector>

#include "subtitleplugin.h"

// 字幕文件解析：从文本内容提取带时间轴的字幕条目。
// 同时兼容 SRT（逗号分隔毫秒）与 WebVTT（点号分隔毫秒）的时间戳格式。
namespace SubtitleParser
{
QVector<SubtitleCue> parse(const QString &content);

// 按文件扩展名选择解析器；未知扩展名返回空结果
QVector<SubtitleCue> parseFile(const QString &filePath);
} // namespace SubtitleParser

#endif // SUBTITLEPARSER_H
