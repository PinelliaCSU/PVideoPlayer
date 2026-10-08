#ifndef SUBTITLEPLUGIN_H
#define SUBTITLEPLUGIN_H

#include <QString>
#include <QVector>

struct SubtitleCue
{
    qint64 startMilliseconds = 0;
    qint64 endMilliseconds = 0;
    QString text;
};

class ISubtitleProvider
{
public:
    virtual ~ISubtitleProvider() = default;
    virtual QVector<SubtitleCue> cuesFor(const QString &mediaLocator) = 0;
};

class SubtitlePluginRegistry final
{
public:
    void registerProvider(ISubtitleProvider *provider);
    QVector<SubtitleCue> cuesFor(const QString &mediaLocator) const;

private:
    QVector<ISubtitleProvider *> m_providers;
};

#endif // SUBTITLEPLUGIN_H
