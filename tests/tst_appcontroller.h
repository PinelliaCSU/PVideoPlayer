#ifndef TST_APPCONTROLLER_H
#define TST_APPCONTROLLER_H

#include <QObject>
#include <QTemporaryDir>

// 应用控制器：播放列表、持久化、播放意图与控制栏策略的集成测试
class AppControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsPersistedPlaylist();
    void rejectsDuplicateLocatorAndNotifies();
    void rejectsMissingFileAndNotifies();
    void addingLocatorPersistsPlaylist();
    void playsLocatorAndForwardsCommands();
    void playNextFollowsPlayMode();
    void renderTargetIsForwarded();
    void extractAudioNotifiesStartAndFinish();
    void userInteractionRequestsControlBar();
    void playbackStateDrivesControlBarPolicy();
    void controlBarHidesWhilePlayingDespiteProgressUpdates();

private:
    QString createTempMedia(const QString &name) const;
    QTemporaryDir *m_dir = nullptr;
};

#endif // TST_APPCONTROLLER_H
