#include "playbacksessionmanager.h"

PlaybackSessionManager::PlaybackSessionManager(QObject *parent)
    : QObject(parent)
{
}

int PlaybackSessionManager::createSession(PlaybackEventSource *events, IPlaybackBackend *backend)
{
    if (!events || !backend) {
        return -1;
    }
    const int id = m_nextSessionId++;
    m_sessions.insert(id, new PlaybackService(events, backend, this));
    return id;
}

PlaybackService *PlaybackSessionManager::session(int sessionId) const
{
    return m_sessions.value(sessionId, nullptr);
}

bool PlaybackSessionManager::destroySession(int sessionId)
{
    PlaybackService *service = m_sessions.take(sessionId);
    if (!service) {
        return false;
    }
    delete service;
    return true;
}
