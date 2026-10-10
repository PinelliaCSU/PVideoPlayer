#include "mediainfoservice.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStringList>

namespace {

// 未采集到数据时统一显示的占位符
const QString kUnknown = QStringLiteral("—");

QString orUnknown(const QString &value)
{
    return value.trimmed().isEmpty() ? kUnknown : value;
}

// 去掉小数末尾多余的 0，例如 23.976 保留原样、25.000 变成 25
QString trimmedDecimal(double value, int precision)
{
    QString text = QString::number(value, 'f', precision);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    return text;
}

} // namespace

QString MediaInfoService::formatResolution(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return kUnknown;
    }
    return QStringLiteral("%1 × %2").arg(width).arg(height);
}

QString MediaInfoService::formatFrameRate(double frameRate)
{
    if (frameRate <= 0.0) {
        return kUnknown;
    }
    return QCoreApplication::translate("MediaInfoService", "%1 fps")
        .arg(trimmedDecimal(frameRate, 3));
}

QString MediaInfoService::formatBitRate(qint64 bitRate)
{
    if (bitRate <= 0) {
        return kUnknown;
    }
    if (bitRate >= 1000000) {
        return QCoreApplication::translate("MediaInfoService", "%1 Mbps")
            .arg(trimmedDecimal(bitRate / 1000000.0, 2));
    }
    if (bitRate >= 1000) {
        return QCoreApplication::translate("MediaInfoService", "%1 kbps")
            .arg(trimmedDecimal(bitRate / 1000.0, 1));
    }
    return QCoreApplication::translate("MediaInfoService", "%1 bps").arg(bitRate);
}

QString MediaInfoService::formatSampleRate(int sampleRate)
{
    if (sampleRate <= 0) {
        return kUnknown;
    }
    if (sampleRate >= 1000) {
        return QCoreApplication::translate("MediaInfoService", "%1 kHz")
            .arg(trimmedDecimal(sampleRate / 1000.0, 1));
    }
    return QCoreApplication::translate("MediaInfoService", "%1 Hz").arg(sampleRate);
}

QString MediaInfoService::formatChannels(int channels)
{
    if (channels <= 0) {
        return kUnknown;
    }

    QString layout;
    switch (channels) {
    case 1:
        layout = QCoreApplication::translate("MediaInfoService", "单声道");
        break;
    case 2:
        layout = QCoreApplication::translate("MediaInfoService", "立体声");
        break;
    case 6:
        layout = QCoreApplication::translate("MediaInfoService", "5.1 声道");
        break;
    case 8:
        layout = QCoreApplication::translate("MediaInfoService", "7.1 声道");
        break;
    default:
        break;
    }

    const QString count = QCoreApplication::translate("MediaInfoService", "%1 声道").arg(channels);
    return layout.isEmpty() ? count : QStringLiteral("%1（%2）").arg(layout, count);
}

QString MediaInfoService::formatCodec(const QString &videoCodec, const QString &audioCodec)
{
    QStringList parts;
    if (!videoCodec.trimmed().isEmpty()) {
        parts << QCoreApplication::translate("MediaInfoService", "视频 %1").arg(videoCodec);
    }
    if (!audioCodec.trimmed().isEmpty()) {
        parts << QCoreApplication::translate("MediaInfoService", "音频 %1").arg(audioCodec);
    }
    return parts.isEmpty() ? kUnknown : parts.join(QStringLiteral(" / "));
}

QString MediaInfoService::formatBufferStatus(const MediaInfo &info)
{
    // 面板会随播放实时刷新，队列为空既可能是刚打开，也可能是播放已结束，
    // 因此这里只陈述事实，不推断“正在缓冲”之类的状态。
    if (info.videoBufferFrames == 0 && info.videoBufferPackets == 0
        && info.audioBufferFrames == 0 && info.audioBufferPackets == 0) {
        return QCoreApplication::translate("MediaInfoService", "无缓冲数据");
    }
    return QCoreApplication::translate(
               "MediaInfoService", "视频 %1 帧 / %2 包，音频 %3 帧 / %4 包，累计丢帧 %5")
        .arg(info.videoBufferFrames)
        .arg(info.videoBufferPackets)
        .arg(info.audioBufferFrames)
        .arg(info.audioBufferPackets)
        .arg(info.droppedFrames);
}

QList<MediaInfoField> MediaInfoService::describe(const MediaInfo &info)
{
    QString fileName = info.fileName;
    if (fileName.trimmed().isEmpty()) {
        fileName = QFileInfo(info.filePath).fileName();
    }

    QList<MediaInfoField> fields;
    fields.append({QCoreApplication::translate("MediaInfoService", "文件名"), orUnknown(fileName)});
    fields.append({QCoreApplication::translate("MediaInfoService", "分辨率"),
                   formatResolution(info.width, info.height)});
    fields.append({QCoreApplication::translate("MediaInfoService", "编码格式"),
                   formatCodec(info.videoCodec, info.audioCodec)});
    fields.append({QCoreApplication::translate("MediaInfoService", "帧率"),
                   formatFrameRate(info.frameRate)});
    fields.append({QCoreApplication::translate("MediaInfoService", "比特率"),
                   formatBitRate(info.bitRate)});
    fields.append({QCoreApplication::translate("MediaInfoService", "音频采样率"),
                   formatSampleRate(info.audioSampleRate)});
    fields.append({QCoreApplication::translate("MediaInfoService", "声道数"),
                   formatChannels(info.audioChannels)});
    fields.append({QCoreApplication::translate("MediaInfoService", "当前解码方式"),
                   orUnknown(info.decodeMethod)});
    fields.append({QCoreApplication::translate("MediaInfoService", "当前播放缓冲状态"),
                   formatBufferStatus(info)});
    fields.append({QCoreApplication::translate("MediaInfoService", "硬件加速状态"),
                   orUnknown(info.hardwareAcceleration)});
    return fields;
}
