#include "tst_appcontroller.h"

#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "../appcontroller.h"
#include "../playlistmodel.h"
#include "../playlistrepository.h"
#include "fakeplaybackbackend.h"

namespace {

// 内存播放列表仓储：测试不触碰真实配置文件
class FakePlaylistRepository final : public IPlaylistRepository
{
public:
    QStringList load() const override { return stored; }
    void save(const QStringList &locators) const override
    {
        ++saveCount;
        stored = locators;
    }

    mutable QStringList stored;
    mutable int saveCount = 0;
};

} // namespace

void AppControllerTest::initTestCase()
{
    m_dir = new QTemporaryDir();
    QVERIFY(m_dir->isValid());
}

QString AppControllerTest::createTempMedia(const QString &name) const
{
    const QString path = m_dir->filePath(name);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return QString();
    }
    file.write("stub");
    file.close();
    return path;
}

void AppControllerTest::loadsPersistedPlaylist()
{
    const QString first = createTempMedia("loaded-1.mp4");
    const QString second = createTempMedia("loaded-2.mp4");
    QVERIFY(!first.isEmpty() && !second.isEmpty());

    FakePlaylistRepository repository;
    repository.stored = {first, second};
    FakePlaybackBackend backend;

    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    QCOMPARE(controller.playlistModel()->rowCount(), 2);
    QCOMPARE(controller.playlistModel()->itemAt(0).locator, first);
    QCOMPARE(controller.playlistModel()->itemAt(1).locator, second);
}

void AppControllerTest::rejectsDuplicateLocatorAndNotifies()
{
    const QString media = createTempMedia("duplicate.mp4");
    QVERIFY(!media.isEmpty());

    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    QSignalSpy notifications(&controller, &AppController::notificationRequested);
    controller.addLocator(media);
    controller.addLocator(media);

    QCOMPARE(controller.playlistModel()->rowCount(), 1);
    QCOMPARE(notifications.count(), 1);
}

void AppControllerTest::rejectsMissingFileAndNotifies()
{
    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    QSignalSpy notifications(&controller, &AppController::notificationRequested);
    controller.addLocator(m_dir->filePath("does-not-exist.mp4"));

    QCOMPARE(controller.playlistModel()->rowCount(), 0);
    QCOMPARE(notifications.count(), 1);
}

void AppControllerTest::addingLocatorPersistsPlaylist()
{
    const QString media = createTempMedia("persist.mp4");
    QVERIFY(!media.isEmpty());

    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    controller.addLocator(media);

    QCOMPARE(repository.saveCount, 1);
    QCOMPARE(repository.stored, QStringList{media});
}

void AppControllerTest::playsLocatorAndForwardsCommands()
{
    const QString media = createTempMedia("play.mp4");
    QVERIFY(!media.isEmpty());

    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    QSignalSpy started(&controller, &AppController::started);
    controller.addLocatorAndPlay(media);

    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 2000);
    QCOMPARE(started.last().at(0).toString(), media);
    QCOMPARE(controller.playlistModel()->rowCount(), 1);

    QSignalSpy pauseSpy(&controller, &AppController::pauseChanged);
    controller.togglePause();
    QTRY_COMPARE_WITH_TIMEOUT(pauseSpy.count(), 1, 2000);
    QCOMPARE(pauseSpy.last().at(0).toBool(), true);

    controller.setVolume(0.4);
    QTRY_COMPARE_WITH_TIMEOUT(backend.lastVolume, 0.4, 2000);
}

void AppControllerTest::playNextFollowsPlayMode()
{
    const QString first = createTempMedia("next-1.mp4");
    const QString second = createTempMedia("next-2.mp4");
    QVERIFY(!first.isEmpty() && !second.isEmpty());

    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    controller.addLocator(first);
    controller.addLocator(second);

    QSignalSpy started(&controller, &AppController::started);
    controller.playIndex(0);
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 2000);
    QCOMPARE(started.last().at(0).toString(), first);

    controller.playNext();
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 2, 2000);
    QCOMPARE(started.last().at(0).toString(), second);

    // 列表循环回到第一项
    controller.playNext();
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 3, 2000);
    QCOMPARE(started.last().at(0).toString(), first);
}

void AppControllerTest::renderTargetIsForwarded()
{
    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    const WId target = 4242;
    controller.setRenderTarget(target);

    QTRY_COMPARE_WITH_TIMEOUT(backend.lastRenderTarget, target, 2000);
}

void AppControllerTest::extractAudioNotifiesStartAndFinish()
{
    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    QSignalSpy startSpy(&controller, &AppController::audioExtractionStarted);
    QSignalSpy finishSpy(&controller, &AppController::audioExtractionFinished);

    controller.extractAudio("input.mp4", "output.wav");

    QCOMPARE(startSpy.count(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 2000);
    QCOMPARE(finishSpy.last().at(0).toBool(), true);
}

void AppControllerTest::userInteractionRequestsControlBar()
{
    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    QSignalSpy showSpy(&controller, &AppController::showControlBarRequested);
    controller.notifyUserInteraction();

    QCOMPARE(showSpy.count(), 1);
}

void AppControllerTest::playbackStateDrivesControlBarPolicy()
{
    FakePlaylistRepository repository;
    FakePlaybackBackend backend;
    AppController controller(&backend, &backend, &repository);
    QVERIFY(controller.init());

    const QString media = createTempMedia("state.mp4");
    QSignalSpy stateSpy(&controller, &AppController::stateChanged);
    controller.addLocatorAndPlay(media);

    QTRY_VERIFY_WITH_TIMEOUT(stateSpy.count() > 0, 2000);
    QCOMPARE(controller.state().status, PlaybackStatus::Opening);
    QCOMPARE(controller.state().currentLocator, media);
    QVERIFY(!controller.isPlaying());

    // 后端上报暂停状态收敛为 Playing/Paused，界面据此决定控制栏显示策略
    emit backend.SigPauseStat(false);
    QTRY_VERIFY_WITH_TIMEOUT(controller.isPlaying(), 2000);

    emit backend.SigPauseStat(true);
    QTRY_VERIFY_WITH_TIMEOUT(!controller.isPlaying(), 2000);
    QCOMPARE(controller.state().status, PlaybackStatus::Paused);
}
