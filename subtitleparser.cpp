#include "subtitleparser.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace
{
// 解析 HH:MM:SS,mmm / HH:MM:SS.mmm / MM:SS.mmm 形式的时间戳
bool parseTimestamp(const QString &text, qint64 *milliseconds)
{
    const QString normalized = QString(text).trimmed().replace(',', '.');
    const QStringList secondsAndMillis = normalized.split('.');
    if (secondsAndMillis.size() != 2) {
        return false;
    }

    const QStringList components = secondsAndMillis.at(0).split(':');
    if (components.size() < 2 || components.size() > 3) {
        return false;
    }

    bool ok = false;
    int componentIndex = 0;
    qint64 hours = 0;
    if (components.size() == 3) {
        hours = components.at(componentIndex++).toLongLong(&ok);
        if (!ok) {
            return false;
        }
    }
    const qint64 minutes = components.at(componentIndex++).toLongLong(&ok);
    if (!ok) {
        return false;
    }
    const qint64 seconds = components.at(componentIndex).toLongLong(&ok);
    if (!ok) {
        return false;
    }

    QString millisText = secondsAndMillis.at(1);
    if (millisText.isEmpty()) {
        return false;
    }
    while (millisText.size() < 3) {
        millisText.append('0');
    }
    const qint64 millis = millisText.left(3).toLongLong(&ok);
    if (!ok) {
        return false;
    }

    *milliseconds = ((hours * 60 + minutes) * 60 + seconds) * 1000 + millis;
    return true;
}
} // namespace

namespace SubtitleParser
{
QVector<SubtitleCue> parse(const QString &content)
{
    QString normalized = content;
    normalized.replace("\r\n", "\n");
    normalized.replace('\r', '\n');
    const QStringList lines = normalized.split('\n');

    QVector<SubtitleCue> cues;
    for (int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const QString arrowLine = lines.at(lineIndex).trimmed();
        if (!arrowLine.contains("-->")) {
            continue;
        }

        const QStringList bounds = arrowLine.split("-->");
        if (bounds.size() != 2) {
            continue;
        }

        qint64 start = 0;
        if (!parseTimestamp(bounds.at(0), &start)) {
            continue;
        }
        // VTT 的结束时间后面可能跟随位置设置，只取时间部分
        const QString endText = bounds.at(1).trimmed().section(' ', 0, 0);
        qint64 end = 0;
        if (!parseTimestamp(endText, &end)) {
            continue;
        }

        QStringList textLines;
        for (int textIndex = lineIndex + 1; textIndex < lines.size(); ++textIndex) {
            const QString textLine = lines.at(textIndex).trimmed();
            // 空行或下一个时间轴行表示当前条目结束
            if (textLine.isEmpty() || textLine.contains("-->")) {
                break;
            }
            textLines.append(lines.at(textIndex).trimmed());
            lineIndex = textIndex;
        }
        if (textLines.isEmpty()) {
            continue;
        }

        SubtitleCue cue;
        cue.startMilliseconds = start;
        cue.endMilliseconds = end;
        cue.text = textLines.join('\n');
        cues.append(cue);
    }
    return cues;
}

QVector<SubtitleCue> parseFile(const QString &filePath)
{
    const QString extension = QFileInfo(filePath).suffix().toLower();
    if (extension != "srt" && extension != "vtt") {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QString content = QString::fromUtf8(file.readAll());
    file.close();
    return parse(content);
}
} // namespace SubtitleParser
