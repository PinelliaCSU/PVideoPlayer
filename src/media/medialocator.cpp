#include "medialocator.h"

#include <QFileInfo>
#include <QStringList>

namespace
{
bool hasSupportedExtension(const QString &locator)
{
    static const QStringList supportedExtensions = {
        ".mkv", ".rmvb", ".mp4", ".avi", ".flv", ".wmv", ".3gp"
    };
    for (const QString &extension : supportedExtensions) {
        if (locator.endsWith(extension, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
} // namespace

namespace MediaLocator
{
bool isNetworkStream(const QString &locator)
{
    return locator.startsWith("http://", Qt::CaseInsensitive) ||
           locator.startsWith("https://", Qt::CaseInsensitive) ||
           locator.startsWith("rtmp://", Qt::CaseInsensitive) ||
           locator.startsWith("rtsp://", Qt::CaseInsensitive) ||
           locator.startsWith("mms://", Qt::CaseInsensitive) ||
           locator.startsWith("mmsh://", Qt::CaseInsensitive) ||
           locator.startsWith("mmst://", Qt::CaseInsensitive) ||
           locator.startsWith("rtp://", Qt::CaseInsensitive) ||
           locator.startsWith("sdp://", Qt::CaseInsensitive);
}

bool isSupportedMediaFile(const QString &locator)
{
    return hasSupportedExtension(locator);
}

LocatorStatus normalize(const QString &input,
                        bool requireSupportedFormat,
                        QString *locator,
                        bool *isNetworkStream)
{
    if (input.isEmpty()) {
        if (locator) {
            locator->clear();
        }
        if (isNetworkStream) {
            *isNetworkStream = false;
        }
        return LocatorStatus::Empty;
    }

    const bool network = MediaLocator::isNetworkStream(input);
    if (isNetworkStream) {
        *isNetworkStream = network;
    }
    if (network) {
        if (locator) {
            *locator = input;
        }
        return LocatorStatus::Valid;
    }

    if (requireSupportedFormat && !hasSupportedExtension(input)) {
        return LocatorStatus::UnsupportedFormat;
    }

    const QFileInfo fileInfo(input);
    if (!fileInfo.exists()) {
        return LocatorStatus::FileNotExists;
    }

    if (locator) {
        *locator = fileInfo.absoluteFilePath();
    }
    return LocatorStatus::Valid;
}
} // namespace MediaLocator
