#include "tst_playbackservice.h"

#include <QtTest>

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

    service.start(QString(), WId{});

    QCOMPARE(backend.startCount, 0);
    QCOMPARE(errors.count(), 1);
    QCOMPARE(service.state().status, PlaybackStatus::Idle);
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

void PlaybackServiceTest::renderTargetCommandIsForwarded()
{
    FakePlaybackBackend backend;
    PlaybackService service(&backend, &backend);

    const WId target = 12345;
    service.setRenderTarget(target);

    QCOMPARE(backend.renderTargetCount, 1);
    QCOMPARE(backend.lastRenderTarget, target);
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

    manager.session(firstId)->start("first.mp4", WId{});
    manager.session(secondId)->start("second.mp4", WId{});

    QCOMPARE(firstBackend.lastLocator, QString("first.mp4"));
    QCOMPARE(secondBackend.lastLocator, QString("second.mp4"));
    QVERIFY(manager.destroySession(firstId));
    QVERIFY(manager.session(secondId) != nullptr);
    QVERIFY(manager.destroySession(secondId));
}
