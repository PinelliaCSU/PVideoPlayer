#include "tst_playlist.h"

#include <QtTest>
#include <QSignalSpy>

#include "../src/core/media_types.h"
#include "../src/playlist/playlistcoordinator.h"
#include "../src/playlist/playlistmodel.h"

namespace {
MediaItem makeItem(const QString &locator)
{
    MediaItem item;
    item.locator = locator;
    item.displayName = locator.section('/', -1);
    item.isNetworkStream = locator.startsWith("http");
    return item;
}
} // namespace

void PlaylistTest::modelAddsAndRejectsDuplicates()
{
    PlaylistModel model;
    QVERIFY(model.addItem(makeItem("a.mp4")));
    QVERIFY(model.addItem(makeItem("b.mp4")));
    // 同一个地址不重复加入
    QVERIFY(!model.addItem(makeItem("a.mp4")));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.indexOf("b.mp4"), 1);
    QCOMPARE(model.indexOf("missing.mp4"), -1);
    // 空地址被拒绝
    QVERIFY(!model.addItem(makeItem(QString())));
    QCOMPARE(model.rowCount(), 2);
}

void PlaylistTest::modelExposesRoles()
{
    PlaylistModel model;
    model.addItem(makeItem("http://example.com/stream"));

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, Qt::DisplayRole).toString(), QString("stream"));
    QCOMPARE(model.data(index, PlaylistModel::LocatorRole).toString(),
             QString("http://example.com/stream"));
    QCOMPARE(model.data(index, PlaylistModel::NetworkStreamRole).toBool(), true);
}

void PlaylistTest::modelRemovesAndClears()
{
    PlaylistModel model;
    model.addItem(makeItem("a.mp4"));
    model.addItem(makeItem("b.mp4"));

    QVERIFY(model.removeAt(0));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.itemAt(0).locator, QString("b.mp4"));
    QVERIFY(!model.removeAt(5));

    model.clear();
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.rowCount(), 0);
}

void PlaylistTest::coordinatorRequestsPlaybackForIndex()
{
    PlaylistModel model;
    model.addItem(makeItem("a.mp4"));
    model.addItem(makeItem("b.mp4"));
    PlaylistCoordinator coordinator(&model);
    QSignalSpy playSpy(&coordinator, &PlaylistCoordinator::playRequested);

    coordinator.playIndex(1);

    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(playSpy.last().at(0).toString(), QString("b.mp4"));
    QCOMPARE(coordinator.currentIndex(), 1);
}

void PlaylistTest::coordinatorIgnoresInvalidIndex()
{
    PlaylistModel model;
    PlaylistCoordinator coordinator(&model);
    QSignalSpy playSpy(&coordinator, &PlaylistCoordinator::playRequested);

    coordinator.playIndex(0);
    coordinator.setCurrentIndex(3);

    QCOMPARE(playSpy.count(), 0);
    QCOMPARE(coordinator.currentIndex(), -1);
}

void PlaylistTest::nextIndexFollowsPlayMode()
{
    PlaylistModel model;
    model.addItem(makeItem("a.mp4"));
    model.addItem(makeItem("b.mp4"));
    model.addItem(makeItem("c.mp4"));
    PlaylistCoordinator coordinator(&model);
    QSignalSpy playSpy(&coordinator, &PlaylistCoordinator::playRequested);

    auto playedLocator = [&playSpy](int index) {
        return playSpy.at(index).at(0).toString();
    };

    // 列表循环：末尾回到开头
    coordinator.playIndex(2);
    coordinator.playNext();
    QCOMPARE(playedLocator(1), QString("a.mp4"));

    // 顺序播放：末尾停止
    coordinator.setPlayMode(PLAYMODE_NORMAL);
    coordinator.playIndex(2);
    const int beforeNormalEnd = playSpy.count();
    coordinator.playNext();
    QCOMPARE(playSpy.count(), beforeNormalEnd);

    // 顺序播放：中间继续向后
    coordinator.playIndex(0);
    coordinator.playNext();
    QCOMPARE(playedLocator(playSpy.count() - 1), QString("b.mp4"));

    // 单曲循环：重复当前项
    coordinator.setPlayMode(PLAYMODE_REPEAT_ONE);
    coordinator.playIndex(1);
    coordinator.playNext();
    QCOMPARE(playedLocator(playSpy.count() - 1), QString("b.mp4"));
}

void PlaylistTest::shuffleAlwaysSelectsAnotherItem()
{
    PlaylistModel model;
    model.addItem(makeItem("a.mp4"));
    model.addItem(makeItem("b.mp4"));
    model.addItem(makeItem("c.mp4"));
    PlaylistCoordinator coordinator(&model);
    coordinator.setPlayMode(PLAYMODE_SHUFFLE);
    QSignalSpy playSpy(&coordinator, &PlaylistCoordinator::playRequested);

    coordinator.playIndex(0);
    for (int i = 0; i < 20; ++i) {
        const int previous = coordinator.currentIndex();
        coordinator.playNext();
        QVERIFY(coordinator.currentIndex() != previous);
        QVERIFY(coordinator.currentIndex() >= 0 && coordinator.currentIndex() < 3);
    }
    QCOMPARE(playSpy.count(), 21);
}

void PlaylistTest::previousWrapsAroundAndStopsWhenEmpty()
{
    PlaylistModel model;
    PlaylistCoordinator emptyCoordinator(&model);
    QSignalSpy emptySpy(&emptyCoordinator, &PlaylistCoordinator::playRequested);
    emptyCoordinator.playNext();
    emptyCoordinator.playPrevious();
    QCOMPARE(emptySpy.count(), 0);

    model.addItem(makeItem("a.mp4"));
    model.addItem(makeItem("b.mp4"));
    PlaylistCoordinator coordinator(&model);
    coordinator.setCurrentIndex(0);
    QSignalSpy playSpy(&coordinator, &PlaylistCoordinator::playRequested);
    coordinator.playPrevious();
    QCOMPARE(playSpy.count(), 1);
    QCOMPARE(playSpy.last().at(0).toString(), QString("b.mp4"));
}

void PlaylistTest::currentIndexChangesArePublished()
{
    PlaylistModel model;
    model.addItem(makeItem("a.mp4"));
    model.addItem(makeItem("b.mp4"));
    PlaylistCoordinator coordinator(&model);
    QSignalSpy indexSpy(&coordinator, &PlaylistCoordinator::currentIndexChanged);

    coordinator.setCurrentIndex(0);
    coordinator.setCurrentIndex(0);
    coordinator.setCurrentIndex(-1);

    // 相同索引不重复发信号
    QCOMPARE(indexSpy.count(), 2);
}

void PlaylistTest::sessionEpochRejectsStaleSessions()
{
    SessionEpoch epoch;
    // 尚未开始任何会话时，任何代际都不匹配
    QVERIFY(!epoch.matches(0));
    QVERIFY(!epoch.matches(1));

    const int first = epoch.begin();
    QVERIFY(epoch.matches(first));

    const int second = epoch.begin();
    QVERIFY(second != first);
    QVERIFY(epoch.matches(second));
    // 上一个会话的事件必须被判定为过期
    QVERIFY(!epoch.matches(first));
    QVERIFY(!epoch.matches(0));
}
