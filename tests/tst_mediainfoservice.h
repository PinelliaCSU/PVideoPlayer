#ifndef TST_MEDIAINFOSERVICE_H
#define TST_MEDIAINFOSERVICE_H

#include <QObject>

// 播放信息展示逻辑（字段顺序、单位换算、未知值占位）的无 GUI 测试
class MediaInfoServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void describeReportsSpecifiedFieldsInOrder();
    void describeCombinesVideoAndAudioCodecs();
    void formatsResolutionAndFrameRate();
    void formatsBitRateWithUnits();
    void formatsSampleRateAndChannels();
    void formatsBufferStatus();
    void unknownValuesFallBackToPlaceholder();
};

#endif // TST_MEDIAINFOSERVICE_H
