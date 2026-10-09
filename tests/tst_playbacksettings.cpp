#include "tst_playbacksettings.h"

#include <QtTest>

#include "../playbacksettings.h"

namespace {
constexpr int kMaxVolume = 128;
}

void PlaybackSettingsTest::defaultRateIsNormal()
{
    PlaybackSettings settings;
    QCOMPARE(settings.rate(), 1.0f);
    QVERIFY(settings.isNormalRate());
    QVERIFY(!settings.rateChanged());
}

void PlaybackSettingsTest::rateChangeIsFlaggedOnce()
{
    PlaybackSettings settings;
    settings.setRate(2.0f);

    QCOMPARE(settings.rate(), 2.0f);
    QVERIFY(!settings.isNormalRate());
    // 倍率变化标记由音频回调消费，消费后必须清除，避免反复重建 sonic 流
    QVERIFY(settings.rateChanged());
    settings.setRateChanged(false);
    QVERIFY(!settings.rateChanged());
}

void PlaybackSettingsTest::unsupportedRateIsRejected()
{
    PlaybackSettings settings;
    QVERIFY(settings.isRateSupported(PLAYBACK_RATE_MIN));
    QVERIFY(settings.isRateSupported(PLAYBACK_RATE_MAX));
    QVERIFY(!settings.isRateSupported(PLAYBACK_RATE_MIN / 2));
    QVERIFY(!settings.isRateSupported(PLAYBACK_RATE_MAX * 2));
}

void PlaybackSettingsTest::volumeRatioIsClamped()
{
    PlaybackSettings settings;
    settings.setVolumeRatio(1.5);
    QCOMPARE(settings.volumeRatio(), 1.0);
    settings.setVolumeRatio(-0.5);
    QCOMPARE(settings.volumeRatio(), 0.0);
}

void PlaybackSettingsTest::volumeScalesToRequestedRange()
{
    PlaybackSettings settings;
    settings.setVolumeRatio(0.5);
    QCOMPARE(settings.volumeFor(kMaxVolume), kMaxVolume / 2);
    QCOMPARE(settings.volumeFor(0), 0);

    settings.setVolumeFrom(kMaxVolume, kMaxVolume);
    QCOMPARE(settings.volumeRatio(), 1.0);
    QCOMPARE(settings.volumeFor(kMaxVolume), kMaxVolume);
}

void PlaybackSettingsTest::quietVolumeCanStillBeLowered()
{
    PlaybackSettings settings;

    // 满音量继续增大保持在最大值
    QCOMPARE(settings.steppedVolume(kMaxVolume, 1, 0.75, kMaxVolume), kMaxVolume);

    // 极小音量下 dB 换算可能取整为同一个值，此时仍必须能够减到 0
    int volume = 1;
    for (int i = 0; i < 4 && volume > 0; ++i) {
        volume = settings.steppedVolume(volume, -1, 0.75, kMaxVolume);
    }
    QCOMPARE(volume, 0);

    // 音量增大不会越界
    const int raised = settings.steppedVolume(kMaxVolume - 1, 1, 0.75, kMaxVolume);
    QVERIFY(raised <= kMaxVolume && raised > 0);
}

void PlaybackSettingsTest::forcesSingleFramePlayback()
{
    PlaybackSettings settings;
    QVERIFY(settings.forcePlay());

    settings.setForcePlay(false);
    QVERIFY(!settings.forcePlay());

    settings.setForcePlay(true);
    QVERIFY(settings.forcePlay());
}
