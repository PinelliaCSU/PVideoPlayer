#include "tst_screenshotutils.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "../src/core/screenshotutils.h"

namespace {

QDateTime fixedTimestamp()
{
    return QDateTime(QDate(2026, 10, 10), QTime(8, 1, 30));
}

} // namespace

void ScreenshotUtilsTest::fileNameContainsMediaNameAndTimestamp()
{
    QCOMPARE(ScreenshotUtils::buildFileName(QStringLiteral("D:/media/sample.mkv"),
                                            fixedTimestamp(), QStringLiteral("png")),
             QStringLiteral("sample_20261010_080130.png"));

    // 扩展名带点号或不带点号都应得到同样的结果，格式大小写不敏感
    QCOMPARE(ScreenshotUtils::buildFileName(QStringLiteral("D:/media/sample.mkv"),
                                            fixedTimestamp(), QStringLiteral(".JPG")),
             QStringLiteral("sample_20261010_080130.jpg"));

    // 未指定扩展名时默认 PNG
    QCOMPARE(ScreenshotUtils::buildFileName(QStringLiteral("D:/media/sample.mkv"),
                                            fixedTimestamp(), QString()),
             QStringLiteral("sample_20261010_080130.png"));
}

void ScreenshotUtilsTest::fileNameStripsDirectoryAndExtension()
{
    QCOMPARE(ScreenshotUtils::mediaDisplayName(QStringLiteral("D:/movies/My Movie.mp4")),
             QStringLiteral("My Movie"));
    // 多个点号时只去掉最后一个扩展名
    QCOMPARE(ScreenshotUtils::mediaDisplayName(QStringLiteral("D:/movies/v1.2.final.mp4")),
             QStringLiteral("v1.2.final"));
}

void ScreenshotUtilsTest::fileNameSanitizesIllegalCharacters()
{
    QCOMPARE(ScreenshotUtils::mediaDisplayName(QStringLiteral("D:/movies/a:b*c.mp4")),
             QStringLiteral("a_b_c"));
}

void ScreenshotUtilsTest::fileNameFallsBackForNetworkStream()
{
    QCOMPARE(ScreenshotUtils::mediaDisplayName(QStringLiteral("rtmp://host/app/stream?token=1")),
             QStringLiteral("stream"));

    // 完全取不到名字时使用固定前缀，保证仍能得到合法文件名
    QCOMPARE(ScreenshotUtils::buildFileName(QString(), fixedTimestamp(), QStringLiteral("png")),
             QStringLiteral("screenshot_20261010_080130.png"));
}

void ScreenshotUtilsTest::uniquePathAvoidsOverwriting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString fileName = QStringLiteral("sample_20261010_080130.png");
    const QString first = ScreenshotUtils::uniquePath(dir.path(), fileName);
    QCOMPARE(first, QDir(dir.path()).filePath(fileName));

    QFile file(first);
    QVERIFY(file.open(QIODevice::WriteOnly));

    const QString second = ScreenshotUtils::uniquePath(dir.path(), fileName);
    QCOMPARE(second, QDir(dir.path()).filePath(QStringLiteral("sample_20261010_080130-1.png")));

    // 目录为空时回退到默认截图目录，不会返回相对路径
    QVERIFY(QDir::isAbsolutePath(ScreenshotUtils::uniquePath(QString(), fileName)));
}

void ScreenshotUtilsTest::defaultDirectoryIsNotEmpty()
{
    const QString directory = ScreenshotUtils::defaultDirectory();
    QVERIFY(!directory.isEmpty());
    QVERIFY(QDir::isAbsolutePath(directory));
}
