#include "tst_subtitleplugin.h"

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "../src/subtitle/subtitleparser.h"
#include "../src/subtitle/subtitleplugin.h"

void SubtitlePluginTest::parsesSrtCues()
{
    const QString content =
        "1\r\n"
        "00:00:01,000 --> 00:00:04,500\r\n"
        "第一行\r\n"
        "第二行\r\n"
        "\r\n"
        "2\r\n"
        "00:01:05,250 --> 00:01:07,000\r\n"
        "Next cue\r\n";

    const QVector<SubtitleCue> cues = SubtitleParser::parse(content);

    QCOMPARE(cues.size(), 2);
    QCOMPARE(cues.at(0).startMilliseconds, 1000);
    QCOMPARE(cues.at(0).endMilliseconds, 4500);
    QCOMPARE(cues.at(0).text, QString("第一行\n第二行"));
    QCOMPARE(cues.at(1).startMilliseconds, 65250);
    QCOMPARE(cues.at(1).endMilliseconds, 67000);
    QCOMPARE(cues.at(1).text, QString("Next cue"));
}

void SubtitlePluginTest::parsesWebVttCues()
{
    const QString content =
        "WEBVTT\n"
        "\n"
        "00:00:02.000 --> 00:00:03.000 line:0 position:20%\n"
        "Hello VTT\n"
        "\n"
        "01:02:03.400 --> 01:02:05.000\n"
        "Later\n";

    const QVector<SubtitleCue> cues = SubtitleParser::parse(content);

    QCOMPARE(cues.size(), 2);
    QCOMPARE(cues.at(0).startMilliseconds, 2000);
    QCOMPARE(cues.at(0).endMilliseconds, 3000);
    QCOMPARE(cues.at(0).text, QString("Hello VTT"));
    QCOMPARE(cues.at(1).startMilliseconds, 3723400);
    QCOMPARE(cues.at(1).text, QString("Later"));
}

void SubtitlePluginTest::ignoresMalformedBlocks()
{
    const QString content =
        "not a timestamp\n"
        "00:00:xx,000 --> 00:00:04,000\n"
        "broken\n"
        "\n"
        "00:00:05,000 --> 00:00:06,000\n"
        "\n"
        "00:00:07,000 --> 00:00:08,000\n"
        "valid\n";

    const QVector<SubtitleCue> cues = SubtitleParser::parse(content);

    QCOMPARE(cues.size(), 1);
    QCOMPARE(cues.at(0).startMilliseconds, 7000);
    QCOMPARE(cues.at(0).text, QString("valid"));
}

void SubtitlePluginTest::parsesSidecarSubtitleFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString mediaPath = dir.filePath("movie.mp4");
    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    QFile subtitleFile(dir.filePath("movie.srt"));
    QVERIFY(subtitleFile.open(QIODevice::WriteOnly | QIODevice::Text));
    subtitleFile.write("1\n00:00:10,000 --> 00:00:12,000\nsidecar\n");
    subtitleFile.close();

    SubtitlePluginRegistry registry;
    const QVector<SubtitleCue> cues = registry.cuesFor(mediaPath);

    QCOMPARE(cues.size(), 1);
    QCOMPARE(cues.at(0).startMilliseconds, 10000);
    QCOMPARE(cues.at(0).text, QString("sidecar"));
}

void SubtitlePluginTest::missingSidecarReturnsNoCues()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString mediaPath = dir.filePath("lonely.mp4");
    QFile mediaFile(mediaPath);
    QVERIFY(mediaFile.open(QIODevice::WriteOnly));
    mediaFile.close();

    SubtitlePluginRegistry registry;
    QVERIFY(registry.cuesFor(mediaPath).isEmpty());
    QVERIFY(registry.cuesFor(QString()).isEmpty());
    QVERIFY(registry.cuesFor("rtsp://host/stream").isEmpty());
}

void SubtitlePluginTest::parsesFileByExtension()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString srtPath = dir.filePath("sample.srt");
    QFile srtFile(srtPath);
    QVERIFY(srtFile.open(QIODevice::WriteOnly | QIODevice::Text));
    srtFile.write("1\n00:00:01,000 --> 00:00:02,000\nok\n");
    srtFile.close();

    QCOMPARE(SubtitleParser::parseFile(srtPath).size(), 1);

    const QString unknownPath = dir.filePath("sample.txt");
    QFile unknownFile(unknownPath);
    QVERIFY(unknownFile.open(QIODevice::WriteOnly | QIODevice::Text));
    unknownFile.write("1\n00:00:01,000 --> 00:00:02,000\nok\n");
    unknownFile.close();

    QVERIFY(SubtitleParser::parseFile(unknownPath).isEmpty());
}
