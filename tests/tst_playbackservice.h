#ifndef TST_PLAYBACKSERVICE_H
#define TST_PLAYBACKSERVICE_H

#include <QObject>

class PlaybackServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void commandsAreForwardedAndStateIsUpdated();
    void emptyLocatorRaisesError();
    void backendErrorIsPublishedAsStateAndSignal();
    void stateChangedTracksBackendEvents();
    void commandsFromAnotherThreadAreSerialized();
    void instantiatedSessionOwnsPlaybackService();
    void sessionsAreIndependent();
    void sessionCommandsRunOnCommandThread();
    void audioExtractionIsAsynchronous();
    void continuousCommandsKeepStateConsistent();
    void renderTargetCommandIsForwarded();
};

#endif // TST_PLAYBACKSERVICE_H
