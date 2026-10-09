#ifndef MEDIA_TYPES_H
#define MEDIA_TYPES_H

#include <QString>
#include <QDateTime>

#include <atomic>

/*
 * 会话代际标记：用于判定来自某个播放会话的异步事件是否仍然有效。
 * 每次开始新的播放（切换文件或重新开始）都会开启新代际，
 * 属于旧代际的事件必须被丢弃，避免上一个文件的事件覆盖新文件的状态。
 */
class SessionEpoch
{
public:
    // 开启新会话，返回新代际号
    int begin()
    {
        return ++m_epoch;
    }

    int current() const
    {
        return m_epoch.load();
    }

    bool matches(int epoch) const
    {
        return epoch != 0 && epoch == m_epoch.load();
    }

private:
    std::atomic<int> m_epoch{0};
};

struct MediaItem
{
    QString locator;
    QString displayName;
    bool isNetworkStream = false;
};

// 播放模式（播放列表领域概念，界面与协调器共用）
enum PlayMode
{
    PLAYMODE_REPEAT_LIST = 0,   // 列表循环（默认）
    PLAYMODE_NORMAL,            // 顺序播放
    PLAYMODE_REPEAT_ONE,        // 循环播放
    PLAYMODE_SHUFFLE            // 随机播放
};

// 播放历史条目（领域类型，避免 UI 直接依赖持久化层的数据结构）
struct PlayHistoryEntry
{
    QString locator;
    int positionSeconds = 0;
    int durationSeconds = 0;
    QDateTime lastPlayed;
};

enum class PlaybackStatus
{
    Idle,
    Opening,
    Playing,
    Paused,
    Seeking,
    Stopping,
    Finished,
    Error
};

enum class PlaybackErrorCode
{
    None,
    InvalidLocator,
    OpenFailed,
    DecoderOpenFailed,
    AudioOutputFailed,
    VideoOutputFailed,
    SeekFailed,
    NetworkError,
    Cancelled,
    InternalError
};

struct PlaybackState
{
    PlaybackStatus status = PlaybackStatus::Idle;
    PlaybackErrorCode errorCode = PlaybackErrorCode::None;
    QString currentLocator;
    QString errorMessage;
    int positionSeconds = 0;
    int durationSeconds = 0;
    double volume = 0.0;
    float speed = 1.0f;
};

#endif // MEDIA_TYPES_H
