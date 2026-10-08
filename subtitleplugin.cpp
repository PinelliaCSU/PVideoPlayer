#include "subtitleplugin.h"
#include "subtitleparser.h"
#include "medialocator.h"

#include <QDir>
#include <QFileInfo>

QVector<SubtitleCue> FileSubtitleProvider::cuesFor(const QString &mediaLocator)
{
    if (mediaLocator.isEmpty() || MediaLocator::isNetworkStream(mediaLocator)) {
        return {};
    }

    const QFileInfo mediaInfo(mediaLocator);
    if (!mediaInfo.exists()) {
        return {};
    }

    const QString basePath = mediaInfo.absolutePath() + QDir::separator() + mediaInfo.completeBaseName();
    static const QStringList candidateExtensions = {".srt", ".vtt"};
    for (const QString &extension : candidateExtensions) {
        const QString subtitlePath = basePath + extension;
        if (!QFileInfo::exists(subtitlePath)) {
            continue;
        }
        const QVector<SubtitleCue> cues = SubtitleParser::parseFile(subtitlePath);
        if (!cues.isEmpty()) {
            return cues;
        }
    }
    return {};
}

SubtitlePluginRegistry::SubtitlePluginRegistry()
{
    m_providers.append(&m_fileProvider);
}

SubtitlePluginRegistry::~SubtitlePluginRegistry()
{
    // 仅移除内部登记，不释放 provider：所有权始终属于注册方
    m_providers.clear();
}

void SubtitlePluginRegistry::registerProvider(ISubtitleProvider *provider)
{
    if (provider && !m_providers.contains(provider)) {
        m_providers.append(provider);
    }
}

QVector<SubtitleCue> SubtitlePluginRegistry::cuesFor(const QString &mediaLocator) const
{
    for (ISubtitleProvider *provider : m_providers) {
        const QVector<SubtitleCue> cues = provider->cuesFor(mediaLocator);
        if (!cues.isEmpty()) {
            return cues;
        }
    }
    return {};
}
