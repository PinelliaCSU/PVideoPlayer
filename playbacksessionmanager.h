#ifndef PLAYBACKSESSIONMANAGER_H
#define PLAYBACKSESSIONMANAGER_H

#include <QObject>
#include <QHash>

#include "playbackservice.h"

class QThread;

/*
 * 播放会话管理：负责创建/销毁播放服务，并为所有播放服务提供唯一的命令线程。
 * 播放命令（start/pause/stop/seek/...）在命令线程串行执行，
 * 使 FFmpeg/SDL 调用与 UI 线程解耦，并保证命令之间不会并发。
 */
class PlaybackSessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackSessionManager(QObject *parent = nullptr);
    ~PlaybackSessionManager() override;

    int createSession(PlaybackEventSource *events, IPlaybackBackend *backend);
    PlaybackService *session(int sessionId) const;
    bool destroySession(int sessionId);

    QThread *commandThread() const;

private:
    void stopCommandThread();

    QThread *m_commandThread = nullptr;
    int m_nextSessionId = 1;
    QHash<int, PlaybackService *> m_sessions;
};

#endif // PLAYBACKSESSIONMANAGER_H
