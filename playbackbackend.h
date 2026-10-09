#ifndef PLAYBACKBACKEND_H
#define PLAYBACKBACKEND_H

#include <QString>
#include <QObject>
#include <QWidget>

class PlaybackEventSource : public QObject
{
    Q_OBJECT

signals:
    void SigStartPlay(QString locator);
    void SigSpeed(float speed);
    void SigPauseStat(bool paused);
    void SigStopFinished();
    void SigUserStopFinished();
    void SigVideoTotalSeconds(int seconds);
    void SigVideoPlaySeconds(int seconds);
    void SigVideoVolume(double percent);
    void SigFrameDimensionsChanged(int width, int height);
    void SigSeekForwardCompleted(int targetSeconds);
    void SigSeekBackCompleted(int targetSeconds);
    void SigError(QString message);

protected:
    explicit PlaybackEventSource(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
};

class IPlaybackBackend
{
public:
    virtual ~IPlaybackBackend() = default;

    virtual void start_play(QString filename, WId playWidgetId) = 0;
    virtual void OnSetSpeed(float speed) = 0;
    virtual void OnPause() = 0;
    virtual void OnStop() = 0;
    virtual void OnUserStop() = 0;
    virtual void OnPlayVolume(double percent) = 0;
    virtual void OnPlaySeek(double percent) = 0;
    virtual void OnSeekForward() = 0;
    virtual void OnSeekBack() = 0;
    virtual void OnAddVolume() = 0;
    virtual void OnSubVolume() = 0;
    virtual void OnStep() = 0;
    virtual bool OnExtractAudio(const QString &inputFile, const QString &outputFile) = 0;
    // 切换视频渲染目标的原生窗口句柄（画中画等场景复用当前播放会话，不中断播放）
    virtual void OnSetRenderTarget(WId playWidgetId) = 0;
};

/*
 * 播放目标接口：视图（Show 等）通过它发起播放和切换渲染窗口，
 * 从而不依赖 PlaybackService 的具体实现。
 */
class IPlaybackTarget
{
public:
    virtual ~IPlaybackTarget() = default;

    virtual void startPlayback(const QString &locator, WId renderTarget) = 0;
    virtual void setRenderTarget(WId renderTarget) = 0;
};

// 默认播放后端（真实实现由 VideoCtrl 提供）。
// 通过工厂函数创建后端，使组合根（main）无需包含 FFmpeg/SDL 等具体后端头文件。
struct PlaybackBackendBundle
{
    PlaybackEventSource *events = nullptr;
    IPlaybackBackend *backend = nullptr;
};

PlaybackBackendBundle CreateDefaultPlaybackBackend();

#endif // PLAYBACKBACKEND_H
