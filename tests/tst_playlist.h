#ifndef TST_PLAYLIST_H
#define TST_PLAYLIST_H

#include <QObject>

// 播放列表模型、播放模式和会话代际的无 GUI 测试
class PlaylistTest : public QObject
{
    Q_OBJECT

private slots:
    void modelAddsAndRejectsDuplicates();
    void modelExposesRoles();
    void modelRemovesAndClears();
    void coordinatorRequestsPlaybackForIndex();
    void coordinatorIgnoresInvalidIndex();
    void nextIndexFollowsPlayMode();
    void shuffleAlwaysSelectsAnotherItem();
    void previousWrapsAroundAndStopsWhenEmpty();
    void currentIndexChangesArePublished();
    void sessionEpochRejectsStaleSessions();
};

#endif // TST_PLAYLIST_H
