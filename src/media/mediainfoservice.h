#ifndef MEDIAINFOSERVICE_H
#define MEDIAINFOSERVICE_H

#include <QList>
#include <QString>

#include "media_types.h"

// 播放信息面板中的一行：字段名与取值
struct MediaInfoField
{
    QString label;
    QString value;
};

/*
 * 播放信息展示服务。
 *
 * 负责把后端采集的 MediaInfo 快照整理成有序的“字段-取值”列表，
 * 并统一处理未知值与单位换算。只做格式化，不依赖 FFmpeg，
 * 界面（MediaInfoDialog）与单元测试共用同一份展示逻辑。
 */
class MediaInfoService
{
public:
    // 面板中展示的全部字段，返回顺序即界面的展示顺序
    static QList<MediaInfoField> describe(const MediaInfo &info);

    static QString formatResolution(int width, int height);
    static QString formatFrameRate(double frameRate);
    static QString formatBitRate(qint64 bitRate);
    static QString formatSampleRate(int sampleRate);
    static QString formatChannels(int channels);
    static QString formatCodec(const QString &videoCodec, const QString &audioCodec);
    static QString formatBufferStatus(const MediaInfo &info);
};

#endif // MEDIAINFOSERVICE_H
