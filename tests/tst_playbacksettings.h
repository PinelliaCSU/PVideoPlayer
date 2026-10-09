#ifndef TST_PLAYBACKSETTINGS_H
#define TST_PLAYBACKSETTINGS_H

#include <QObject>

// 播放参数（倍速、音量、逐帧标记）的无 GUI 测试
class PlaybackSettingsTest : public QObject
{
    Q_OBJECT

private slots:
    void defaultRateIsNormal();
    void rateChangeIsFlaggedOnce();
    void unsupportedRateIsRejected();
    void volumeRatioIsClamped();
    void volumeScalesToRequestedRange();
    void quietVolumeCanStillBeLowered();
    void forcesSingleFramePlayback();
};

#endif // TST_PLAYBACKSETTINGS_H
