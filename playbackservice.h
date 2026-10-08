#ifndef PLAYBACKSERVICE_H
#define PLAYBACKSERVICE_H

#include <QObject>
#include <QString>
#include <QMetaType>
#include <QWidget>
#include <functional>

#include "media_types.h"
#include "playbackbackend.h"

class PlaybackService : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackService(PlaybackEventSource *events,
                             IPlaybackBackend *backend,
                             QObject *parent = nullptr);
    ~PlaybackService() override;

    void start(const QString &locator, WId playWidgetId);
    void pause();
    void stop();
    void userStop();
    void setVolume(double percent);
    void seek(double percent);
    void setSpeed(float speed);
    void seekForward();
    void seekBack();
    void addVolume();
    void subVolume();
    void step();
    bool extractAudio(const QString &inputFile, const QString &outputFile);
    void setRenderTarget(WId playWidgetId);

    PlaybackState state() const;

signals:
    void started(const QString &locator);
    void speedChanged(float speed);
    void pauseChanged(bool paused);
    void finished();
    void userStopped();
    void totalSecondsChanged(int seconds);
    void positionSecondsChanged(int seconds);
    void volumeChanged(double percent);
    void frameDimensionsChanged(int width, int height);
    void seekForwardCompleted(int targetSeconds);
    void seekBackCompleted(int targetSeconds);
    void errorOccurred(const QString &message);

private:
    PlaybackEventSource *m_events;
    IPlaybackBackend *m_backend;
    PlaybackState m_state;
    bool m_acceptCommands = true;

    void enqueue(std::function<void()> command);
};

Q_DECLARE_METATYPE(PlaybackState)

#endif // PLAYBACKSERVICE_H
