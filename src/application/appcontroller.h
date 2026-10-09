#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QTimer>

#include <functional>

#include "media_types.h"
#include "playbackbackend.h"
#include "playlistcoordinator.h"

class PlaybackService;
class PlaybackSession;
class PlaybackCoordinator;
class PlaylistModel;
class IPlaylistRepository;

/*
 * 应用级控制器：UI 与播放服务之间的唯一边界。
 *
 * 职责：
 *  - 接收 Show / CtrlBar / Playlist 的用户意图并转成播放命令；
 *  - 协调播放列表、自动下一首和播放模式；
 *  - 管理播放历史和断点续播；
 *  - 发布统一的播放状态与错误；
 *  - 控制栏自动隐藏策略。
 *
 * MainWindow 只负责布局、窗口行为和把 UI 组件连接到本控制器。
 */
class AppController final : public QObject, public IPlaybackTarget
{
    Q_OBJECT

public:
    AppController(PlaybackSession *session,
                  IPlaylistRepository *repository, QObject *parent = nullptr);
    AppController(PlaybackEventSource *events, IPlaybackBackend *backend,
                  IPlaylistRepository *repository, QObject *parent = nullptr);
    ~AppController() override;

    bool init();

    PlaybackService *playbackService() const { return m_service; }
    PlaylistModel *playlistModel() const { return m_playlistModel; }
    PlaybackState state() const;
    bool isPlaying() const;

    // ===== 播放列表 =====
    void addLocator(const QString &rawLocator);   // 校验、去重、持久化
    void addLocatorAndPlay(const QString &rawLocator);
    void removeAt(int row);
    void clearPlaylist();
    void playIndex(int row);
    void playNext();
    void playPrevious();
    void setPlayMode(int mode);

    // ===== 播放意图 =====
    void togglePause();
    void stop();
    void setVolume(double percent);
    void seek(double percent);
    void setSpeed(float speed);
    void seekForward();
    void seekBack();
    void addVolume();
    void subVolume();
    void step();
    void extractAudio(const QString &inputFile, const QString &outputFile);

    // ===== 播放历史与续播 =====
    QList<PlayHistoryEntry> history() const;
    void playFromHistory(const QString &locator, int positionSeconds);
    void resumePlaybackAt(int positionSeconds, int totalSeconds);
    void clearHistory();

    // ===== IPlaybackTarget =====
    void startPlayback(const QString &locator, WId renderTarget) override;
    void setRenderTarget(WId renderTarget) override;

    /*
     * 渲染目标提供者：视图在真正开始播放时才需要原生窗口句柄。
     * 使用延迟查询而不是启动时提前查询，避免视频容器在还没开始渲染时
     * 就变成原生窗口而出现未绘制的空白区域。
     */
    using RenderTargetProvider = std::function<WId()>;
    void setRenderTargetProvider(RenderTargetProvider provider);

    // 用户交互：显示控制栏并重新开始自动隐藏计时
    void notifyUserInteraction();

    // 控制栏自动隐藏延时（毫秒），供设置项与测试使用
    void setControlBarHideDelay(int milliseconds);

signals:
    void started(const QString &locator);
    void pauseChanged(bool paused);
    void speedChanged(float speed);
    void finished();
    void userStopped();
    void totalSecondsChanged(int seconds);
    void positionSecondsChanged(int seconds);
    void volumeChanged(double percent);
    void frameDimensionsChanged(int width, int height);
    void seekForwardCompleted(int targetSeconds);
    void seekBackCompleted(int targetSeconds);
    void errorOccurred(const QString &message);
    void stateChanged(const PlaybackState &state);
    void resumeAvailable(const QString &locator, int positionSeconds, int totalSeconds);
    void currentIndexChanged(int row);
    void audioExtractionStarted();
    void audioExtractionFinished(bool success, const QString &message);

    // 控制栏显示策略（由控制器根据播放状态决定）
    void showControlBarRequested();
    void hideControlBarRequested();

    // 需要界面提示的信息（重复添加、文件不存在等）
    void notificationRequested(const QString &message);

private:
    void wirePlaybackSignals();
    void onPlaybackStateChanged();
    void updateControlBarPolicy();
    void restartControlBarTimer();
    // 解析当前渲染目标：优先向视图延迟查询
    WId resolveRenderTarget();
    bool appendLocator(const QString &rawLocator, bool requireSupportedFormat, bool notify);
    int rowForLocator(const QString &rawLocator, bool requireSupportedFormat, bool notify);
    QStringList locators() const;
    void persistPlaylist();

    PlaybackSession *m_session = nullptr;
    PlaybackService *m_service = nullptr;
    PlaybackCoordinator *m_playbackCoordinator = nullptr;
    PlaylistModel *m_playlistModel = nullptr;
    PlaylistCoordinator *m_playlistCoordinator = nullptr;
    IPlaylistRepository *m_repository = nullptr;

    WId m_renderTarget = 0;
    RenderTargetProvider m_renderTargetProvider;
    // 当前是否处于“应自动隐藏控制栏”的会话状态
    bool m_autoHideControlBar = false;
    QTimer m_controlBarTimer;
};

#endif // APPCONTROLLER_H
