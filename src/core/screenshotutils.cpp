#include "screenshotutils.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

const char kFallbackName[] = "screenshot";
const char kFallbackExtension[] = "png";

// 文件名中不允许出现的字符（Windows 的限制最严格，按它处理最安全）
QString sanitizeName(const QString &name)
{
    QString sanitized;
    sanitized.reserve(name.size());
    for (const QChar character : name) {
        if (character.isNull() || character.unicode() < 0x20
            || QStringLiteral("<>:\"/\\|?*").contains(character)) {
            sanitized.append(QLatin1Char('_'));
        } else {
            sanitized.append(character);
        }
    }
    return sanitized.trimmed();
}

} // namespace

QString ScreenshotUtils::defaultDirectory()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    return QDir(base).filePath(QStringLiteral("PVideoPlayer"));
}

QString ScreenshotUtils::mediaDisplayName(const QString &mediaLocator)
{
    // 网络流地址常带查询参数或片段，先去掉，避免进入文件名
    QString locator = mediaLocator.section(QLatin1Char('?'), 0, 0);
    locator = locator.section(QLatin1Char('#'), 0, 0);

    QString name = QFileInfo(locator).fileName();
    if (name.isEmpty()) {
        // 形如 rtmp://host/app 的地址取不到文件名时，取路径最后一段
        name = locator.section(QLatin1Char('/'), -1);
    }

    // 文件名里的扩展名不再重复保留
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        name = name.left(dot);
    }
    return sanitizeName(name);
}

QString ScreenshotUtils::buildFileName(const QString &mediaLocator, const QDateTime &timestamp,
                                       const QString &extension)
{
    QString name = mediaDisplayName(mediaLocator);
    if (name.isEmpty()) {
        name = QString::fromLatin1(kFallbackName);
    }

    QString suffix = extension;
    if (suffix.startsWith(QLatin1Char('.'))) {
        suffix = suffix.mid(1);
    }
    if (suffix.isEmpty()) {
        suffix = QString::fromLatin1(kFallbackExtension);
    }

    return QStringLiteral("%1_%2.%3")
        .arg(name, timestamp.toString(QStringLiteral("yyyyMMdd_HHmmss")), suffix.toLower());
}

QString ScreenshotUtils::uniquePath(const QString &directory, const QString &fileName)
{
    const QDir dir(directory.isEmpty() ? defaultDirectory() : directory);
    const QString path = dir.filePath(fileName);
    if (!QFileInfo::exists(path)) {
        return path;
    }

    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();
    for (int index = 1; index < 1000; ++index) {
        const QString candidate =
            dir.filePath(QStringLiteral("%1-%2.%3").arg(base).arg(index).arg(suffix));
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return path;
}
