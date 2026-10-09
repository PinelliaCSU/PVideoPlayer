#ifndef PLAYBACKCOORDINATOR_H
#define PLAYBACKCOORDINATOR_H

#include <QObject>
#include <QList>
#include <QString>

#include "media_types.h"

class PlaybackService;

/*
 * 播放协调器：负责播放位置、播放历史和断点续播策略，
 * 使播放历史与断点续播逻辑不再落在 MainWindow 中。
 */
class PlaybackCoordinator final : public QObject
{
    Q_OBJECT

public:
    explicit PlaybackCoordinator(PlaybackService *service, QObject *parent = nullptr);

    void saveCurrentPlayback();
    void clearHistory();

    // 已保存的播放位置（秒），无有效断点时返回 -1
    int savedPosition(const QString &locator) const;
    // 是否需要在开始播放后提示续播
    bool shouldResume(const QString &locator, int totalSeconds) const;

    QList<PlayHistoryEntry> history() const;
    int totalSeconds() const { return m_totalSeconds; }

signals:
    void resumeAvailable(const QString &locator, int positionSeconds, int totalSeconds);
    void historyCleared();

private:
    void onStarted(const QString &locator);
    void onTotalSecondsChanged(int seconds);
    void onPositionSecondsChanged(int seconds);
    void savePlayback(const QString &locator, int positionSeconds, int totalSeconds);
    void updateHistory(const QString &locator, int positionSeconds, int totalSeconds);

    PlaybackService *m_service;
    QString m_locator;
    int m_positionSeconds = 0;
    int m_totalSeconds = 0;
    bool m_resumeChecked = false;
};

#endif // PLAYBACKCOORDINATOR_H
