#include "appcontroller.h"

#include <QFileInfo>
#include <QTimer>

#include "medialocator.h"
#include "playbackcoordinator.h"
#include "playbackservice.h"
#include "playbacksession.h"
#include "playlistmodel.h"
#include "playlistrepository.h"

namespace {
// 控制栏在播放中无操作后的自动隐藏延时
constexpr int kControlBarHideDelayMs = 3000;
// 历史记录续播时等待后端就绪的延时
constexpr int kSeekAfterStartDelayMs = 500;
}

AppController::AppController(PlaybackSession *session,
                             IPlaylistRepository *repository, QObject *parent)
    : QObject(parent)
    , m_playlistModel(new PlaylistModel(this))
    , m_playlistCoordinator(new PlaylistCoordinator(m_playlistModel, this))
    , m_session(session)
    , m_repository(repository)
{
    Q_ASSERT(m_session != nullptr);
    Q_ASSERT(m_session->isValid());
    m_service = m_session->service();
    m_playbackCoordinator = new PlaybackCoordinator(m_service, this);

    m_controlBarTimer.setSingleShot(true);
    m_controlBarTimer.setInterval(kControlBarHideDelayMs);
    connect(&m_controlBarTimer, &QTimer::timeout, this, &AppController::hideControlBarRequested);

    connect(m_playlistCoordinator, &PlaylistCoordinator::playRequested,
            this, [this](const QString &locator) { startPlayback(locator, m_renderTarget); });
    connect(m_playlistCoordinator, &PlaylistCoordinator::currentIndexChanged,
            this, &AppController::currentIndexChanged);

    wirePlaybackSignals();
}

AppController::AppController(PlaybackEventSource *events, IPlaybackBackend *backend,
                             IPlaylistRepository *repository, QObject *parent)
    : AppController(new PlaybackSession(events, backend), repository, parent)
{
    m_session->setParent(this);
}

AppController::~AppController() = default;

bool AppController::init()
{
    if (m_repository) {
        // 载入已保存的播放列表：失效条目静默跳过，不打扰用户
        for (const QString &locator : m_repository->load()) {
            appendLocator(locator, false, false);
        }
    }
    if (m_playlistModel->rowCount() > 0) {
        m_playlistCoordinator->setCurrentIndex(0);
    }
    return true;
}

void AppController::wirePlaybackSignals()
{
    connect(m_service, &PlaybackService::started, this, &AppController::started);
    connect(m_service, &PlaybackService::pauseChanged, this, &AppController::pauseChanged);
    connect(m_service, &PlaybackService::speedChanged, this, &AppController::speedChanged);
    connect(m_service, &PlaybackService::finished, this, &AppController::finished);
    connect(m_service, &PlaybackService::userStopped, this, &AppController::userStopped);
    connect(m_service, &PlaybackService::volumeChanged, this, &AppController::volumeChanged);
    connect(m_service, &PlaybackService::frameDimensionsChanged, this, &AppController::frameDimensionsChanged);
    connect(m_service, &PlaybackService::seekForwardCompleted, this, &AppController::seekForwardCompleted);
    connect(m_service, &PlaybackService::seekBackCompleted, this, &AppController::seekBackCompleted);
    connect(m_service, &PlaybackService::errorOccurred, this, &AppController::errorOccurred);
    connect(m_service, &PlaybackService::stateChanged, this, &AppController::stateChanged);
    connect(m_service, &PlaybackService::stateChanged, this, &AppController::onPlaybackStateChanged);
    connect(m_service, &PlaybackService::audioExtractionFinished,
            this, [this](bool success, const QString &inputFile, const QString &) {
                emit audioExtractionFinished(success,
                                             success ? tr("音频提取完成：%1").arg(inputFile)
                                                     : tr("音频提取失败：%1").arg(inputFile));
            });
    connect(m_service, &PlaybackService::screenshotFinished, this, &AppController::screenshotFinished);

    // 播放进度：转发统一播放状态中的位置与总时长
    connect(m_service, &PlaybackService::positionSecondsChanged, this, &AppController::positionSecondsChanged);
    connect(m_service, &PlaybackService::totalSecondsChanged, this, &AppController::totalSecondsChanged);

    connect(m_playbackCoordinator, &PlaybackCoordinator::resumeAvailable,
            this, &AppController::resumeAvailable);

    // 播放结束自动播放下一个（播放模式由播放列表协调器决定）
    connect(m_service, &PlaybackService::finished, m_playlistCoordinator, &PlaylistCoordinator::playNext);
}

PlaybackState AppController::state() const
{
    return m_service ? m_service->state() : PlaybackState{};
}

bool AppController::isPlaying() const
{
    return m_service && m_service->isPlaying();
}

MediaInfo AppController::mediaInfo() const
{
    return m_service ? m_service->mediaInfo() : MediaInfo{};
}

void AppController::onPlaybackStateChanged()
{
    updateControlBarPolicy();
}

void AppController::updateControlBarPolicy()
{
    /*
     * 播放状态会被高频刷新（每次播放进度上报都会发布 stateChanged），
     * 因此这里只在“是否应自动隐藏”发生翻转时才操作计时器，
     * 否则进度刷新会不断推迟隐藏时间，控制栏就永远不会自动隐藏。
     */
    const PlaybackStatus status = state().status;
    const bool shouldAutoHide = status == PlaybackStatus::Opening
        || status == PlaybackStatus::Playing
        || status == PlaybackStatus::Seeking;

    if (shouldAutoHide == m_autoHideControlBar) {
        return;
    }

    m_autoHideControlBar = shouldAutoHide;
    if (shouldAutoHide) {
        m_controlBarTimer.start();
    } else {
        m_controlBarTimer.stop();
    }
}

void AppController::restartControlBarTimer()
{
    m_controlBarTimer.stop();
    if (m_autoHideControlBar) {
        m_controlBarTimer.start();
    }
}

void AppController::notifyUserInteraction()
{
    emit showControlBarRequested();
    restartControlBarTimer();
}

void AppController::setControlBarHideDelay(int milliseconds)
{
    m_controlBarTimer.setInterval(milliseconds > 0 ? milliseconds : 1);
}

// ===== 播放列表 =====

QStringList AppController::locators() const
{
    QStringList result;
    for (const MediaItem &item : m_playlistModel->items()) {
        result.append(item.locator);
    }
    return result;
}

void AppController::persistPlaylist()
{
    if (m_repository) {
        m_repository->save(locators());
    }
}

bool AppController::appendLocator(const QString &rawLocator, bool requireSupportedFormat, bool notify)
{
    QString locator;
    switch (MediaLocator::normalize(rawLocator, requireSupportedFormat, &locator, nullptr)) {
    case MediaLocator::LocatorStatus::Empty:
        return false;
    case MediaLocator::LocatorStatus::UnsupportedFormat:
        if (notify) {
            emit notificationRequested(tr("不支持的文件格式：%1").arg(rawLocator));
        }
        return false;
    case MediaLocator::LocatorStatus::FileNotExists:
        if (notify) {
            emit notificationRequested(tr("文件 \"%1\" 不存在，请检查路径是否正确。").arg(rawLocator));
        }
        return false;
    default:
        break;
    }

    const bool networkStream = MediaLocator::isNetworkStream(locator);
    const QString displayName = networkStream ? locator : QFileInfo(locator).fileName();
    if (!m_playlistModel->addItem({locator, displayName, networkStream})) {
        if (notify) {
            emit notificationRequested(tr("文件 \"%1\" 已经在播放列表中存在。").arg(displayName));
        }
        return false;
    }
    persistPlaylist();
    return true;
}

int AppController::rowForLocator(const QString &rawLocator, bool requireSupportedFormat, bool notify)
{
    QString locator;
    if (MediaLocator::normalize(rawLocator, requireSupportedFormat, &locator, nullptr) !=
        MediaLocator::LocatorStatus::Valid) {
        return -1;
    }

    appendLocator(rawLocator, requireSupportedFormat, notify);
    return m_playlistModel->indexOf(locator);
}

void AppController::addLocator(const QString &rawLocator)
{
    appendLocator(rawLocator, false, true);
}

void AppController::addLocatorAndPlay(const QString &rawLocator)
{
    const int row = rowForLocator(rawLocator, true, true);
    if (row >= 0) {
        m_playlistCoordinator->playIndex(row);
    }
}

void AppController::removeAt(int row)
{
    if (!m_playlistModel->removeAt(row)) {
        return;
    }
    if (m_playlistCoordinator->currentIndex() >= m_playlistModel->rowCount()) {
        m_playlistCoordinator->setCurrentIndex(qMax(-1, m_playlistModel->rowCount() - 1));
    }
    persistPlaylist();
}

void AppController::clearPlaylist()
{
    m_playlistModel->clear();
    m_playlistCoordinator->setCurrentIndex(-1);
    persistPlaylist();
}

void AppController::playIndex(int row)
{
    m_playlistCoordinator->playIndex(row);
}

void AppController::playNext()
{
    m_playlistCoordinator->playNext();
}

void AppController::playPrevious()
{
    m_playlistCoordinator->playPrevious();
}

void AppController::setPlayMode(int mode)
{
    m_playlistCoordinator->setPlayMode(static_cast<PlayMode>(mode));
}

// ===== 播放意图 =====

void AppController::startPlayback(const QString &locator, WId renderTarget)
{
    if (renderTarget != 0) {
        m_renderTarget = renderTarget;
    }
    m_service->start(locator, resolveRenderTarget());
}

void AppController::setRenderTargetProvider(RenderTargetProvider provider)
{
    m_renderTargetProvider = std::move(provider);
}

WId AppController::resolveRenderTarget()
{
    if (m_renderTargetProvider) {
        const WId provided = m_renderTargetProvider();
        if (provided != 0) {
            m_renderTarget = provided;
        }
    }
    return m_renderTarget;
}

void AppController::setRenderTarget(WId renderTarget)
{
    if (renderTarget == 0) {
        return;
    }
    m_renderTarget = renderTarget;
    m_service->setRenderTarget(renderTarget);
}

void AppController::togglePause()
{
    m_service->pause();
}

void AppController::stop()
{
    m_service->userStop();
}

void AppController::setVolume(double percent)
{
    m_service->setVolume(percent);
}

void AppController::seek(double percent)
{
    m_service->seek(percent);
}

void AppController::setSpeed(float speed)
{
    m_service->setSpeed(speed);
}

void AppController::seekForward()
{
    m_service->seekForward();
}

void AppController::seekBack()
{
    m_service->seekBack();
}

void AppController::addVolume()
{
    m_service->addVolume();
}

void AppController::subVolume()
{
    m_service->subVolume();
}

void AppController::step()
{
    m_service->step();
}

void AppController::extractAudio(const QString &inputFile, const QString &outputFile)
{
    emit audioExtractionStarted();
    m_service->extractAudio(inputFile, outputFile);
}

void AppController::captureFrame(const QString &outputFile)
{
    if (m_service) {
        m_service->captureFrame(outputFile);
    }
}

// ===== 播放历史与续播 =====

QList<PlayHistoryEntry> AppController::history() const
{
    return m_playbackCoordinator->history();
}

void AppController::playFromHistory(const QString &locator, int positionSeconds)
{
    startPlayback(locator, m_renderTarget);
    if (positionSeconds <= 0) {
        return;
    }
    // 等待后端打开媒体并上报总时长后再定位
    QTimer::singleShot(kSeekAfterStartDelayMs, this, [this, positionSeconds]() {
        const int totalSeconds = m_playbackCoordinator->totalSeconds();
        if (totalSeconds > 0) {
            m_service->seek(static_cast<double>(positionSeconds) / totalSeconds);
        }
    });
}

void AppController::resumePlaybackAt(int positionSeconds, int totalSeconds)
{
    if (positionSeconds <= 0 || totalSeconds <= 0) {
        return;
    }
    m_service->seek(static_cast<double>(positionSeconds) / totalSeconds);
}

void AppController::clearHistory()
{
    m_playbackCoordinator->clearHistory();
}
