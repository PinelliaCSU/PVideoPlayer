#ifndef TST_SUBTITLEPLUGIN_H
#define TST_SUBTITLEPLUGIN_H

#include <QObject>

class SubtitlePluginTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesSrtCues();
    void parsesWebVttCues();
    void ignoresMalformedBlocks();
    void parsesSidecarSubtitleFile();
    void missingSidecarReturnsNoCues();
    void parsesFileByExtension();
};

#endif // TST_SUBTITLEPLUGIN_H
