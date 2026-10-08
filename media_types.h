#ifndef MEDIA_TYPES_H
#define MEDIA_TYPES_H

#include <QString>

struct MediaItem
{
    QString locator;
    QString displayName;
    bool isNetworkStream = false;
};

enum class PlaybackStatus
{
    Idle,
    Opening,
    Playing,
    Paused,
    Stopping,
    Finished,
    Error
};

struct PlaybackState
{
    PlaybackStatus status = PlaybackStatus::Idle;
    QString currentLocator;
    int positionSeconds = 0;
    int durationSeconds = 0;
    double volume = 0.0;
    float speed = 1.0f;
};

#endif // MEDIA_TYPES_H
