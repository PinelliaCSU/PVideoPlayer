#ifndef PLAYBACKSESSION_H
#define PLAYBACKSESSION_H

#include <QObject>

#include "playbackbackend.h"

class PlaybackService;
class PlaybackSessionManager;

/*
 * 一个可实例化的播放会话。
 *
 * 会话拥有命令线程和 PlaybackService，但不拥有外部注入的后端；
 * 这样测试可以继续注入 FakePlaybackBackend，生产代码则通过
 * createDefault() 创建真实的 FFmpeg/SDL 后端。
 */
class PlaybackSession final : public QObject
{
    Q_OBJECT

public:
    PlaybackSession(PlaybackEventSource *events,
                    IPlaybackBackend *backend,
                    QObject *parent = nullptr);
    ~PlaybackSession() override;

    static PlaybackSession *createDefault(QObject *parent = nullptr);

    PlaybackService *service() const { return m_service; }
    PlaybackEventSource *events() const { return m_events; }
    IPlaybackBackend *backend() const { return m_backend; }
    bool isValid() const { return m_service != nullptr; }

private:
    explicit PlaybackSession(QObject *parent);
    bool initialize(PlaybackEventSource *events, IPlaybackBackend *backend);

    PlaybackSessionManager *m_sessionManager = nullptr;
    PlaybackEventSource *m_events = nullptr;
    IPlaybackBackend *m_backend = nullptr;
    PlaybackService *m_service = nullptr;
};

#endif // PLAYBACKSESSION_H
