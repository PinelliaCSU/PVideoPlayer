#ifndef PLAYBACKBACKEND_H
#define PLAYBACKBACKEND_H

#include <QString>
#include <QObject>
#include <QWidget>

class PlaybackEventSource : public QObject
{
    Q_OBJECT

signals:
    void SigStartPlay(QString locator);
    void SigSpeed(float speed);
    void SigPauseStat(bool paused);
    void SigStopFinished();
    void SigUserStopFinished();
    void SigVideoTotalSeconds(int seconds);
    void SigVideoPlaySeconds(int seconds);
    void SigVideoVolume(double percent);
    void SigFrameDimensionsChanged(int width, int height);
    void SigSeekForwardCompleted(int targetSeconds);
    void SigSeekBackCompleted(int targetSeconds);
    void SigError(QString message);

protected:
    explicit PlaybackEventSource(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
};

class IPlaybackBackend
{
public:
    virtual ~IPlaybackBackend() = default;

    virtual void start_play(QString filename, WId playWidgetId) = 0;
    virtual void OnSetSpeed(float speed) = 0;
    virtual void OnPause() = 0;
    virtual void OnStop() = 0;
    virtual void OnUserStop() = 0;
    virtual void OnPlayVolume(double percent) = 0;
    virtual void OnPlaySeek(double percent) = 0;
    virtual void OnSeekForward() = 0;
    virtual void OnSeekBack() = 0;
    virtual void OnAddVolume() = 0;
    virtual void OnSubVolume() = 0;
    virtual void OnStep() = 0;
    virtual bool OnExtractAudio(const QString &inputFile, const QString &outputFile) = 0;
};

#endif // PLAYBACKBACKEND_H
