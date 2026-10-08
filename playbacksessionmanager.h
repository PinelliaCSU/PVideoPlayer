#ifndef PLAYBACKSESSIONMANAGER_H
#define PLAYBACKSESSIONMANAGER_H

#include <QObject>
#include <QHash>

#include "playbackservice.h"

class PlaybackSessionManager final : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackSessionManager(QObject *parent = nullptr);
    int createSession(PlaybackEventSource *events, IPlaybackBackend *backend);
    PlaybackService *session(int sessionId) const;
    bool destroySession(int sessionId);

private:
    int m_nextSessionId = 1;
    QHash<int, PlaybackService *> m_sessions;
};

#endif // PLAYBACKSESSIONMANAGER_H
