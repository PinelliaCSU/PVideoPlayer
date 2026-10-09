#include <QApplication>
#include <QFileInfo>
#include <QtTest>

#include "tst_appcontroller.h"
#include "tst_medialocator.h"
#include "tst_playbackservice.h"
#include "tst_playbacksettings.h"
#include "tst_playlist.h"
#include "tst_subtitleplugin.h"

namespace {

/*
 * 一个进程内运行多个测试套件时，QtTest 的 -o 日志文件会被后一个套件覆盖。
 * 这里为每个套件生成独立的日志文件：player.txt -> player-MediaLocatorTest.txt
 */
QVector<QByteArray> suiteArgs(int argc, char *argv[], const char *suiteName)
{
    QStringList args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.append(QString::fromLocal8Bit(argv[i]));
    }

    const int optionIndex = args.indexOf(QStringLiteral("-o"));
    if (optionIndex >= 0 && optionIndex + 1 < args.size()) {
        const QString option = args.at(optionIndex + 1);
        const int commaIndex = option.indexOf(QLatin1Char(','));
        const QString fileName = commaIndex >= 0 ? option.left(commaIndex) : option;
        const QString format = commaIndex >= 0 ? option.mid(commaIndex) : QString();
        if (!fileName.isEmpty() && fileName != QLatin1String("-")) {
            const QFileInfo info(fileName);
            const QString suffixed = info.completeBaseName() + QLatin1Char('-')
                + QString::fromLatin1(suiteName) + QLatin1Char('.') + info.suffix();
            args[optionIndex + 1] = info.absolutePath() + QLatin1Char('/') + suffixed + format;
        }
    }

    QVector<QByteArray> storage;
    storage.reserve(args.size());
    for (const QString &arg : args) {
        storage.append(arg.toLocal8Bit());
    }
    return storage;
}

// QTest::qExec 需要可写且在整个调用期间保持有效的 argv
QVector<char *> writableArgs(QVector<QByteArray> &storage)
{
    QVector<char *> raw;
    raw.reserve(storage.size());
    for (QByteArray &arg : storage) {
        raw.append(arg.data());
    }
    return raw;
}

} // namespace

#define PVP_RUN_SUITE(SuiteType)                                                   \
    do {                                                                           \
        QVector<QByteArray> storage = suiteArgs(argc, argv, #SuiteType);           \
        QVector<char *> raw = writableArgs(storage);                               \
        SuiteType suite;                                                           \
        status |= QTest::qExec(&suite, raw.size(), raw.data());                    \
    } while (false)

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    PVP_RUN_SUITE(MediaLocatorTest);
    PVP_RUN_SUITE(SubtitlePluginTest);
    PVP_RUN_SUITE(PlaybackServiceTest);
    PVP_RUN_SUITE(PlaybackSettingsTest);
    PVP_RUN_SUITE(PlaylistTest);
    PVP_RUN_SUITE(AppControllerTest);
    return status;
}
