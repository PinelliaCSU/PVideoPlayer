#include "playbacksession.h"

#include "playbackservice.h"
#include "playbacksessionmanager.h"

PlaybackSession::PlaybackSession(PlaybackEventSource *events,
                                 IPlaybackBackend *backend,
                                 QObject *parent)
    : QObject(parent)
    , m_sessionManager(new PlaybackSessionManager(this))
{
    initialize(events, backend);
}

PlaybackSession::PlaybackSession(QObject *parent)
    : QObject(parent)
    , m_sessionManager(new PlaybackSessionManager(this))
{
}

PlaybackSession::~PlaybackSession() = default;

bool PlaybackSession::initialize(PlaybackEventSource *events, IPlaybackBackend *backend)
{
    if (!events || !backend || m_service) {
        return false;
    }

    const int sessionId = m_sessionManager->createSession(events, backend);
    if (sessionId < 0) {
        return false;
    }

    m_events = events;
    m_backend = backend;
    m_service = m_sessionManager->session(sessionId);
    return m_service != nullptr;
}
