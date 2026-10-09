#include "tst_playbackservice.h"

#include <QtTest>
#include <QSignalSpy>
#include <QThread>

#include "../playbackservice.h"
#include "../playbacksessionmanager.h"
#include "fakeplaybackbackend.h"

void PlaybackServiceTest::commandsAreForwardedAndStateIsUpdated()
{
    FakePlaybackBackend backend;
    PlaybackService service(&backend, &backend);

    service.start("sample.mp4", WId{});
    QCOMPARE(backend.startCount, 1);
    QCOMPARE(backend.lastLocator, QString("sample.mp4"));
    QCOMPARE(service.state().status, PlaybackStatus::Opening);

    service.pause();
    QCOMPARE(service.state().status, PlaybackStatus::Paused);

    service.setSpeed(1.5f);
    QCOMPARE(service.state().speed, 1.5f);

    service.setVolume(0.25);
    QCOMPARE(service.state().volume, 0.25);

    service.stop();
    QCOMPARE(service.state().status, PlaybackStatus::Finished);

    service.userStop();
    QCOMPARE(service.state().status, PlaybackStatus::Idle);
}

void PlaybackServiceTest::emptyLocatorRaisesError()
{
    FakePlaybackBackend backend;
    PlaybackService service(&backend, &backend);
    QSignalSpy errors(&service, &PlaybackService::errorOccurred);
    QSignalSpy codedErrors(&service, &PlaybackService::playbackError);

    service.start(QString(), WId{});

    QCOMPARE(backend.startCount, 0);
    QCOMPARE(errors.count(), 1);
    QCOMPARE(codedErrors.count(), 1);
    // 空地址是不可恢复错误：状态进入 Error 并带有明确错误码
    QCOMPARE(service.state().status, PlaybackStatus::Error);
    QCOMPARE(service.state().errorCode, PlaybackErrorCode::InvalidLocator);
    QVERIFY(!service.state().errorMessage.isEmpty());
}

void PlaybackServiceTest::backendErrorIsPublishedAsStateAndSignal()
{
    FakePlaybackBackend backend;
    PlaybackService service(&backend, &backend);
    service.start("broken.mp4", WId{});

    QSignalSpy codedErrors(&service, &PlaybackService::playbackError);
    emit backend.SigError(QStringLiteral("解码失败"));

    QCOMPARE(codedErrors.count(), 1);
    QCOMPARE(service.state().status, PlaybackStatus::Error);
    QCOMPARE(service.state().errorCode, PlaybackErrorCode::InternalError);
    QCOMPARE(service.state().errorMessage, QStringLiteral("解码失败"));
}

void PlaybackServiceTest::stateChangedTracksBackendEvents()
{
    FakePlaybackBackend backend;
    PlaybackService service(&backend, &backend);
    QSignalSpy stateSpy(&service, &PlaybackService::stateChanged);

    service.start("movie.mp4", WId{});
    emit backend.SigVideoTotalSeconds(120);
    emit backend.SigVideoPlaySeconds(30);
    emit backend.SigPauseStat(true);

    QVERIFY(stateSpy.count() >= 4);

    const PlaybackState state = service.state();
    QCOMPARE(state.currentLocator, QString("movie.mp4"));
    QCOMPARE(state.durationSeconds, 120);
    QCOMPARE(state.positionSeconds, 30);
    QCOMPARE(state.status, PlaybackStatus::Paused);

    // stateChanged 携带的快照与 state() 一致（唯一权威来源）
    const QVariant last = stateSpy.last().at(0);
    const PlaybackState published = last.value<PlaybackState>();
    QCOMPARE(published.durationSeconds, state.durationSeconds);
    QCOMPARE(published.status, state.status);
}

void PlaybackServiceTest::commandsFromAnotherThreadAreSerialized()
{
    FakePlaybackBackend backend;
    PlaybackService service(&backend, &backend);
    QThread callerThread;
    QObject::connect(&callerThread, &QThread::started, &service, [&service, &callerThread]() {
        service.start("queued.mp4", WId{});
        service.setSpeed(2.0f);
        service.stop();
        callerThread.quit();
    }, Qt::DirectConnection);
    callerThread.start();

    QTRY_COMPARE_WITH_TIMEOUT(backend.startCount, 1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(backend.lastLocator, QString("queued.mp4"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(service.state().speed, 2.0f, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(service.state().status, PlaybackStatus::Finished, 1000);

    callerThread.wait();
}

void PlaybackServiceTest::sessionsAreIndependent()
{
    FakePlaybackBackend firstBackend;
    FakePlaybackBackend secondBackend;
    PlaybackSessionManager manager;
    const int firstId = manager.createSession(&firstBackend, &firstBackend);
    const int secondId = manager.createSession(&secondBackend, &secondBackend);

    QVERIFY(firstId > 0);
    QVERIFY(secondId > firstId);
    QVERIFY(manager.session(firstId) != nullptr);
    QVERIFY(manager.session(secondId) != nullptr);

    QSignalSpy firstStarted(manager.session(firstId), &PlaybackService::started);
    QSignalSpy secondStarted(manager.session(secondId), &PlaybackService::started);

    manager.session(firstId)->start("first.mp4", WId{});
    manager.session(secondId)->start("second.mp4", WId{});

    // 命令在命令线程执行，因此这里是异步等待而不是立即断言
    QTRY_COMPARE_WITH_TIMEOUT(firstStarted.count(), 1, 2000);
    QTRY_COMPARE_WITH_TIMEOUT(secondStarted.count(), 1, 2000);
    QCOMPARE(firstStarted.last().at(0).toString(), QString("first.mp4"));
    QCOMPARE(secondStarted.last().at(0).toString(), QString("second.mp4"));

    QVERIFY(manager.destroySession(firstId));
    QVERIFY(manager.session(firstId) == nullptr);
    QVERIFY(manager.session(secondId) != nullptr);
    QVERIFY(manager.destroySession(secondId));
}

void PlaybackServiceTest::sessionCommandsRunOnCommandThread()
{
    FakePlaybackBackend backend;
    PlaybackSessionManager manager;
    const int id = manager.createSession(&backend, &backend);
    PlaybackService *service = manager.session(id);
    QVERIFY(service != nullptr);
    QVERIFY(manager.commandThread() != nullptr);
    QVERIFY(manager.commandThread() != QThread::currentThread());

    // 服务必须运行在命令线程上，播放命令因此不会阻塞 UI 线程
    QVERIFY(service->thread() == manager.commandThread());

    QSignalSpy started(service, &PlaybackService::started);
    service->start("threaded.mp4", WId{});
    QTRY_COMPARE_WITH_TIMEOUT(started.count(), 1, 2000);
    QVERIFY(service->thread() == manager.commandThread());
}

void PlaybackServiceTest::audioExtractionIsAsynchronous()
{
    FakePlaybackBackend backend;
    PlaybackSessionManager manager;
    const int id = manager.createSession(&backend, &backend);
    PlaybackService *service = manager.session(id);
    QVERIFY(service != nullptr);
    QSignalSpy finishedSpy(service, &PlaybackService::audioExtractionFinished);

    service->extractAudio("input.mp4", "output.wav");

    // 调用立即返回（不阻塞 UI 线程），结果稍后通过信号送达
    QCOMPARE(finishedSpy.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 2000);
    const QList<QVariant> arguments = finishedSpy.last();
    QCOMPARE(arguments.at(0).toBool(), true);
    QCOMPARE(arguments.at(1).toString(), QString("input.mp4"));
    QCOMPARE(arguments.at(2).toString(), QString("output.wav"));
}

void PlaybackServiceTest::continuousCommandsKeepStateConsistent()
{
    FakePlaybackBackend backend;
    PlaybackSessionManager manager;
    const int id = manager.createSession(&backend, &backend);
    PlaybackService *service = manager.session(id);
    QVERIFY(service != nullptr);
    QSignalSpy stateSpy(service, &PlaybackService::stateChanged);

    // 连续下发命令（模拟快速切换文件/暂停/停止）必须串行执行且不产生死锁
    for (int i = 0; i < 20; ++i) {
        service->start(QStringLiteral("clip.mp4"), WId{});
        service->pause();
        service->setSpeed(1.0f + (i % 3) * 0.5f);
        service->stop();
    }
    service->userStop();

    // 命令在命令线程串行执行：等待所有命令执行完毕
    QTRY_COMPARE_WITH_TIMEOUT(backend.stopCount, 20, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(backend.userStopCount, 1, 5000);
    // 最终状态必须是最后一个命令的结果
    QCOMPARE(service->state().status, PlaybackStatus::Idle);
    // 状态变化事件跨线程排队送达
    QTRY_VERIFY_WITH_TIMEOUT(stateSpy.count() > 0, 2000);

    // 销毁会话：命令线程随后退出，资源随线程结束释放
    QVERIFY(manager.destroySession(id));
    QTRY_VERIFY_WITH_TIMEOUT(manager.session(id) == nullptr, 2000);
}

void PlaybackServiceTest::renderTargetCommandIsForwarded()
{
    FakePlaybackBackend backend;
    PlaybackService service(&backend, &backend);

    const WId target = 12345;
    service.setRenderTarget(target);

    QCOMPARE(backend.renderTargetCount, 1);
    QCOMPARE(backend.lastRenderTarget, target);
}
