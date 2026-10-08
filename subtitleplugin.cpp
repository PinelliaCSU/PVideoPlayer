#include "subtitleplugin.h"

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
