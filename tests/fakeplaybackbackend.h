#ifndef FAKEPLAYBACKBACKEND_H
#define FAKEPLAYBACKBACKEND_H

#include "../playbackbackend.h"

class FakePlaybackBackend final : public PlaybackEventSource, public IPlaybackBackend
{
    Q_OBJECT

public:
    explicit FakePlaybackBackend(QObject *parent = nullptr)
        : PlaybackEventSource(parent)
    {
    }

    void start_play(QString filename, WId) override
    {
        lastLocator = filename;
        ++startCount;
        emit SigStartPlay(filename);
    }

    void OnSetSpeed(float speed) override
    {
        lastSpeed = speed;
        emit SigSpeed(speed);
    }

    void OnPause() override
    {
        paused = !paused;
        emit SigPauseStat(paused);
    }

    void OnStop() override
    {
        ++stopCount;
        emit SigStopFinished();
    }

    void OnUserStop() override
    {
        ++userStopCount;
        emit SigUserStopFinished();
    }

    void OnPlayVolume(double percent) override
    {
        lastVolume = percent;
        emit SigVideoVolume(percent);
    }

    void OnPlaySeek(double percent) override
    {
        lastSeek = percent;
    }

    void OnSeekForward() override
    {
        emit SigSeekForwardCompleted(5);
    }

    void OnSeekBack() override
    {
        emit SigSeekBackCompleted(0);
    }

    void OnAddVolume() override {}
    void OnSubVolume() override {}
    void OnStep() override {}

    bool OnExtractAudio(const QString &, const QString &) override
    {
        return true;
    }

    void OnSetRenderTarget(WId playWidgetId) override
    {
        lastRenderTarget = playWidgetId;
        ++renderTargetCount;
    }

    QString lastLocator;
    float lastSpeed = 1.0f;
    double lastVolume = 0.0;
    double lastSeek = 0.0;
    int startCount = 0;
    int stopCount = 0;
    int userStopCount = 0;
    int renderTargetCount = 0;
    WId lastRenderTarget = 0;
    bool paused = false;
};

#endif // FAKEPLAYBACKBACKEND_H
