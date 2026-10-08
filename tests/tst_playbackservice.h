#ifndef TST_PLAYBACKSERVICE_H
#define TST_PLAYBACKSERVICE_H

#include <QObject>

class PlaybackServiceTest : public QObject
{
    Q_OBJECT

private slots:
    void commandsAreForwardedAndStateIsUpdated();
    void emptyLocatorRaisesError();
    void commandsFromAnotherThreadAreSerialized();
    void sessionsAreIndependent();
};

#endif // TST_PLAYBACKSERVICE_H
