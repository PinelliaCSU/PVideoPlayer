#include "tst_mediainfoservice.h"

#include <QtTest>

#include "../src/media/mediainfoservice.h"

namespace {

MediaInfo fullInfo()
{
    MediaInfo info;
    info.valid = true;
    info.fileName = QStringLiteral("sample.mkv");
    info.filePath = QStringLiteral("D:/media/sample.mkv");
    info.width = 1920;
    info.height = 1080;
    info.frameRate = 23.976;
    info.videoCodec = QStringLiteral("h264");
    info.audioCodec = QStringLiteral("aac");
    info.bitRate = 5200000;
    info.audioSampleRate = 48000;
    info.audioChannels = 2;
    info.decodeMethod = QStringLiteral("视频 硬件解码（D3D11VA） / 音频 软件解码");
    info.hardwareAcceleration = QStringLiteral("已启用（D3D11VA）");
    info.videoBufferFrames = 3;
    info.videoBufferPackets = 12;
    info.audioBufferFrames = 5;
    info.audioBufferPackets = 8;
    info.droppedFrames = 1;
    return info;
}

QString valueOf(const QList<MediaInfoField> &fields, const QString &label)
{
    for (const MediaInfoField &field : fields) {
        if (field.label == label) {
            return field.value;
        }
    }
    return QString();
}

} // namespace

void MediaInfoServiceTest::describeReportsSpecifiedFieldsInOrder()
{
    const QList<MediaInfoField> fields = MediaInfoService::describe(fullInfo());

    QStringList labels;
    for (const MediaInfoField &field : fields) {
        labels << field.label;
    }

    const QStringList expected{
        QStringLiteral("文件名"),      QStringLiteral("分辨率"),       QStringLiteral("编码格式"),
        QStringLiteral("帧率"),        QStringLiteral("比特率"),       QStringLiteral("音频采样率"),
        QStringLiteral("声道数"),      QStringLiteral("当前解码方式"), QStringLiteral("当前播放缓冲状态"),
        QStringLiteral("硬件加速状态"),
    };
    QCOMPARE(labels, expected);
}

void MediaInfoServiceTest::describeCombinesVideoAndAudioCodecs()
{
    const QList<MediaInfoField> fields = MediaInfoService::describe(fullInfo());
    QCOMPARE(valueOf(fields, QStringLiteral("编码格式")), QStringLiteral("视频 h264 / 音频 aac"));

    MediaInfo videoOnly = fullInfo();
    videoOnly.audioCodec.clear();
    QCOMPARE(valueOf(MediaInfoService::describe(videoOnly), QStringLiteral("编码格式")),
             QStringLiteral("视频 h264"));
}

void MediaInfoServiceTest::formatsResolutionAndFrameRate()
{
    QCOMPARE(MediaInfoService::formatResolution(1920, 1080), QStringLiteral("1920 × 1080"));
    QCOMPARE(MediaInfoService::formatFrameRate(23.976), QStringLiteral("23.976 fps"));
    // 整数帧率不应显示多余的小数位
    QCOMPARE(MediaInfoService::formatFrameRate(25.0), QStringLiteral("25 fps"));
}

void MediaInfoServiceTest::formatsBitRateWithUnits()
{
    QCOMPARE(MediaInfoService::formatBitRate(5200000), QStringLiteral("5.2 Mbps"));
    QCOMPARE(MediaInfoService::formatBitRate(192000), QStringLiteral("192 kbps"));
    QCOMPARE(MediaInfoService::formatBitRate(999), QStringLiteral("999 bps"));
}

void MediaInfoServiceTest::formatsSampleRateAndChannels()
{
    QCOMPARE(MediaInfoService::formatSampleRate(48000), QStringLiteral("48 kHz"));
    QCOMPARE(MediaInfoService::formatSampleRate(44100), QStringLiteral("44.1 kHz"));
    QCOMPARE(MediaInfoService::formatChannels(1), QStringLiteral("单声道（1 声道）"));
    QCOMPARE(MediaInfoService::formatChannels(2), QStringLiteral("立体声（2 声道）"));
    QCOMPARE(MediaInfoService::formatChannels(3), QStringLiteral("3 声道"));
}

void MediaInfoServiceTest::formatsBufferStatus()
{
    const QList<MediaInfoField> fields = MediaInfoService::describe(fullInfo());
    QCOMPARE(valueOf(fields, QStringLiteral("当前播放缓冲状态")),
             QStringLiteral("视频 3 帧 / 12 包，音频 5 帧 / 8 包，累计丢帧 1"));

    // 所有缓冲为空时不显示一串 0
    MediaInfo idle = fullInfo();
    idle.videoBufferFrames = idle.videoBufferPackets = 0;
    idle.audioBufferFrames = idle.audioBufferPackets = 0;
    QCOMPARE(MediaInfoService::formatBufferStatus(idle), QStringLiteral("无缓冲数据"));
}

void MediaInfoServiceTest::unknownValuesFallBackToPlaceholder()
{
    MediaInfo info;
    info.valid = true;
    info.filePath = QStringLiteral("D:/media/only-path.mkv");

    const QList<MediaInfoField> fields = MediaInfoService::describe(info);

    // 没有文件名时回退到完整路径
    QCOMPARE(valueOf(fields, QStringLiteral("文件名")), QStringLiteral("only-path.mkv"));
    QCOMPARE(valueOf(fields, QStringLiteral("分辨率")), QStringLiteral("—"));
    QCOMPARE(valueOf(fields, QStringLiteral("编码格式")), QStringLiteral("—"));
    QCOMPARE(valueOf(fields, QStringLiteral("比特率")), QStringLiteral("—"));
    QCOMPARE(valueOf(fields, QStringLiteral("音频采样率")), QStringLiteral("—"));
    QCOMPARE(valueOf(fields, QStringLiteral("声道数")), QStringLiteral("—"));
}
