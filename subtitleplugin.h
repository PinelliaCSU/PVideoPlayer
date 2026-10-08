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

// 内置 provider：查找媒体文件同目录下的同名 .srt / .vtt 字幕文件并解析
class FileSubtitleProvider final : public ISubtitleProvider
{
public:
    QVector<SubtitleCue> cuesFor(const QString &mediaLocator) override;
};

// 字幕 provider 注册表。构造时自动注册内置的本地字幕文件 provider，
// 外部实现可以通过 registerProvider() 追加（注册表不接管外部对象的所有权）。
class SubtitlePluginRegistry final
{
public:
    SubtitlePluginRegistry();
    ~SubtitlePluginRegistry();

    void registerProvider(ISubtitleProvider *provider);
    QVector<SubtitleCue> cuesFor(const QString &mediaLocator) const;

private:
    FileSubtitleProvider m_fileProvider;
    QVector<ISubtitleProvider *> m_providers;
};

#endif // SUBTITLEPLUGIN_H
