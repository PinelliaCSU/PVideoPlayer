#ifndef PLAYBACKSERVICE_H
#define PLAYBACKSERVICE_H

#include <QObject>
#include <QMutex>
#include <QString>
#include <QMetaType>
#include <QWidget>
#include <functional>

#include "media_types.h"
#include "playbackbackend.h"

/*
 * 播放服务：播放状态的唯一权威来源。
 * - 所有播放命令通过 enqueue() 串行投递到服务所属线程执行；
 * - 后端事件只能通过显式的状态变更入口写入 m_state；
 * - 状态变化统一通过 stateChanged(PlaybackState) 发布。
 */
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
    // 音频提取在服务线程执行，完成后通过 audioExtractionFinished 通知，避免阻塞 UI
    void extractAudio(const QString &inputFile, const QString &outputFile);
    void setRenderTarget(WId playWidgetId);

    // 线程安全的状态快照
    PlaybackState state() const;
    bool isPlaying() const;
    // 当前媒体的信息快照（线程安全），未打开媒体时 valid 为 false
    MediaInfo mediaInfo() const;

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
    void playbackError(PlaybackErrorCode code, const QString &message);
    void stateChanged(const PlaybackState &state);
    void audioExtractionFinished(bool success, const QString &inputFile, const QString &outputFile);

private:
    PlaybackEventSource *m_events;
    IPlaybackBackend *m_backend;
    PlaybackState m_state;
    mutable QMutex m_stateMutex;
    bool m_acceptCommands = true;

    void enqueue(std::function<void()> command);
    // 唯一的播放状态写入入口
    void mutateState(const std::function<void(PlaybackState &)> &mutation);
    void publishState();
    void reportError(PlaybackErrorCode code, const QString &message);
};

Q_DECLARE_METATYPE(PlaybackState)

#endif // PLAYBACKSERVICE_H
