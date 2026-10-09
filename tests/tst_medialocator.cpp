#include "tst_medialocator.h"

#include <QtTest>

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "../src/media/medialocator.h"

using MediaLocator::LocatorStatus;

void MediaLocatorTest::networkStreamsAreDetected()
{
    QVERIFY(MediaLocator::isNetworkStream("http://example.com/a.mp4"));
    QVERIFY(MediaLocator::isNetworkStream("HTTPS://example.com/a.mp4"));
    QVERIFY(MediaLocator::isNetworkStream("rtsp://host/stream"));
    QVERIFY(MediaLocator::isNetworkStream("rtmp://host/live"));
    QVERIFY(!MediaLocator::isNetworkStream("C:/videos/a.mp4"));
    QVERIFY(!MediaLocator::isNetworkStream("sample.mp4"));
}

void MediaLocatorTest::supportedMediaFilesAreDetected()
{
    QVERIFY(MediaLocator::isSupportedMediaFile("movie.MKV"));
    QVERIFY(MediaLocator::isSupportedMediaFile("clip.mp4"));
    QVERIFY(!MediaLocator::isSupportedMediaFile("notes.txt"));
    QVERIFY(!MediaLocator::isSupportedMediaFile("movie"));
}

void MediaLocatorTest::emptyLocatorIsRejected()
{
    QString locator = "unchanged";
    bool network = true;
    QCOMPARE(MediaLocator::normalize(QString(), true, &locator, &network), LocatorStatus::Empty);
    QVERIFY(locator.isEmpty());
    QCOMPARE(network, false);
}

void MediaLocatorTest::networkStreamBypassesFormatRequirement()
{
    QString locator;
    bool network = false;
    QCOMPARE(MediaLocator::normalize("rtsp://host/stream", true, &locator, &network), LocatorStatus::Valid);
    QCOMPARE(locator, QString("rtsp://host/stream"));
    QCOMPARE(network, true);
}

void MediaLocatorTest::unsupportedFormatIsRejectedBeforeExistenceCheck()
{
    QString locator;
    // 即使文件不存在，格式不受支持时也应先返回 UnsupportedFormat
    QCOMPARE(MediaLocator::normalize("missing/notes.txt", true, &locator, nullptr),
             LocatorStatus::UnsupportedFormat);
}

void MediaLocatorTest::unsupportedFormatIsAcceptedWhenNotRequired()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("notes.txt");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QString locator;
    QCOMPARE(MediaLocator::normalize(path, false, &locator, nullptr), LocatorStatus::Valid);
    QCOMPARE(locator, QFileInfo(path).absoluteFilePath());
}

void MediaLocatorTest::missingFileIsReported()
{
    QString locator;
    bool network = true;
    QCOMPARE(MediaLocator::normalize("missing/movie.mp4", true, &locator, &network),
             LocatorStatus::FileNotExists);
    QCOMPARE(network, false);
}

void MediaLocatorTest::existingFileIsNormalizedToAbsolutePath()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("sample.mp4");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();

    QString locator;
    bool network = true;
    QCOMPARE(MediaLocator::normalize(path, true, &locator, &network), LocatorStatus::Valid);
    QCOMPARE(locator, QFileInfo(path).absoluteFilePath());
    QCOMPARE(network, false);
}
