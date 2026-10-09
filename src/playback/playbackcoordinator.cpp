#include "playbackcoordinator.h"

#include <algorithm>

#include "configutils.h"
#include "playbackservice.h"

namespace {
// 距开头/结尾过近的位置没有续播价值
constexpr int kMinResumeSeconds = 5;
constexpr int kMaxResumeTailSeconds = 5;
// 播放到结尾附近时视为已看完，清除断点
constexpr int kCompletedTailSeconds = 3;
constexpr int kMaxHistoryItems = 10;
}

PlaybackCoordinator::PlaybackCoordinator(PlaybackService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
    Q_ASSERT(m_service != nullptr);

    connect(m_service, &PlaybackService::started,
            this, &PlaybackCoordinator::onStarted);
    connect(m_service, &PlaybackService::totalSecondsChanged,
            this, &PlaybackCoordinator::onTotalSecondsChanged);
    connect(m_service, &PlaybackService::positionSecondsChanged,
            this, &PlaybackCoordinator::onPositionSecondsChanged);
    connect(m_service, &PlaybackService::finished,
            this, &PlaybackCoordinator::saveCurrentPlayback);
    connect(m_service, &PlaybackService::userStopped,
            this, &PlaybackCoordinator::saveCurrentPlayback);
}

void PlaybackCoordinator::onStarted(const QString &locator)
{
    // 切换文件前先保存上一个文件的播放位置
    savePlayback(m_locator, m_positionSeconds, m_totalSeconds);

    m_locator = locator;
    m_positionSeconds = 0;
    m_totalSeconds = 0;
    m_resumeChecked = false;
}

void PlaybackCoordinator::onTotalSecondsChanged(int seconds)
{
    m_totalSeconds = seconds;
    if (m_resumeChecked || m_locator.isEmpty() || m_totalSeconds <= 0) {
        return;
    }

    m_resumeChecked = true;
    if (!shouldResume(m_locator, m_totalSeconds)) {
        return;
    }

    emit resumeAvailable(m_locator, savedPosition(m_locator), m_totalSeconds);
}

void PlaybackCoordinator::onPositionSecondsChanged(int seconds)
{
    m_positionSeconds = seconds;
}

int PlaybackCoordinator::savedPosition(const QString &locator) const
{
    if (locator.isEmpty()) {
        return -1;
    }
    return ConfigUtils::LoadPlaybackPos(locator);
}

bool PlaybackCoordinator::shouldResume(const QString &locator, int totalSeconds) const
{
    if (locator.isEmpty() || totalSeconds <= 0) {
        return false;
    }
    const int position = savedPosition(locator);
    return position > kMinResumeSeconds && position < totalSeconds - kMaxResumeTailSeconds;
}

QList<PlayHistoryEntry> PlaybackCoordinator::history() const
{
    QList<PlayHistoryEntry> entries;
    const QList<ConfigUtils::PlayHistoryItem> items = ConfigUtils::LoadPlayHistory();
    entries.reserve(items.size());
    for (const ConfigUtils::PlayHistoryItem &item : items) {
        PlayHistoryEntry entry;
        entry.locator = item.filePath;
        entry.positionSeconds = item.lastPosition;
        entry.durationSeconds = item.totalDuration;
        entry.lastPlayed = item.lastPlayed;
        entries.append(entry);
    }
    return entries;
}

void PlaybackCoordinator::saveCurrentPlayback()
{
    savePlayback(m_locator, m_positionSeconds, m_totalSeconds);
}

void PlaybackCoordinator::savePlayback(const QString &locator, int positionSeconds, int totalSeconds)
{
    if (locator.isEmpty() || totalSeconds <= 0) {
        return;
    }

    if (positionSeconds >= totalSeconds - kCompletedTailSeconds) {
        ConfigUtils::ClearPlaybackPos(locator);
    } else if (positionSeconds > 0) {
        ConfigUtils::SavePlaybackPos(locator, positionSeconds);
    }

    updateHistory(locator, positionSeconds, totalSeconds);
}

void PlaybackCoordinator::updateHistory(const QString &locator, int positionSeconds, int totalSeconds)
{
    QList<ConfigUtils::PlayHistoryItem> history = ConfigUtils::LoadPlayHistory();
    history.erase(std::remove_if(history.begin(), history.end(),
                                 [&locator](const ConfigUtils::PlayHistoryItem &item) {
                                     return item.filePath == locator;
                                 }),
                  history.end());

    ConfigUtils::PlayHistoryItem item;
    item.filePath = locator;
    item.lastPosition = positionSeconds;
    item.totalDuration = totalSeconds;
    item.lastPlayed = QDateTime::currentDateTime();
    history.prepend(item);
    while (history.size() > kMaxHistoryItems) {
        history.removeLast();
    }
    ConfigUtils::SavePlayHistory(history);
}

void PlaybackCoordinator::clearHistory()
{
    ConfigUtils::SavePlayHistory({});
    emit historyCleared();
}
