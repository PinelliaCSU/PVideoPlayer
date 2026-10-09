#include "playbacksession.h"

PlaybackSession *PlaybackSession::createDefault(QObject *parent)
{
    auto *session = new PlaybackSession(parent);
    const PlaybackBackendBundle bundle = CreateDefaultPlaybackBackend(session);
    if (!session->initialize(bundle.events, bundle.backend)) {
        delete session;
        return nullptr;
    }
    return session;
}
