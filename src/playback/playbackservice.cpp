#include "playbackservice.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

PlaybackService::PlaybackService(PlaybackEventSource *events,
                                 IPlaybackBackend *backend,
                                 QObject *parent)
    : QObject(parent)
    , m_events(events)
    , m_backend(backend)
{
    Q_ASSERT(m_events != nullptr);
    Q_ASSERT(m_backend != nullptr);

    /*
     * 后端事件 → 状态转换。
     * 这些连接在后端线程（SDL 读取/渲染线程）发出信号时会自动排队到服务线程执行，
     * 因此状态写入始终发生在服务线程内，UI 线程只读取 state() 快照。
     */
    connect(m_events, &PlaybackEventSource::SigStartPlay, this, [this](const QString &locator) {
        mutateState([&locator](PlaybackState &state) {
            state.currentLocator = locator;
            state.status = PlaybackStatus::Opening;
            state.errorCode = PlaybackErrorCode::None;
            state.errorMessage.clear();
        });
        emit started(locator);
    });
    connect(m_events, &PlaybackEventSource::SigSpeed, this, [this](float speed) {
        mutateState([speed](PlaybackState &state) { state.speed = speed; });
        emit speedChanged(speed);
    });
    connect(m_events, &PlaybackEventSource::SigPauseStat, this, [this](bool paused) {
        mutateState([paused](PlaybackState &state) {
            state.status = paused ? PlaybackStatus::Paused : PlaybackStatus::Playing;
        });
        emit pauseChanged(paused);
    });
    connect(m_events, &PlaybackEventSource::SigStopFinished, this, [this]() {
        mutateState([](PlaybackState &state) { state.status = PlaybackStatus::Finished; });
        emit finished();
    });
    connect(m_events, &PlaybackEventSource::SigUserStopFinished, this, [this]() {
        mutateState([](PlaybackState &state) { state.status = PlaybackStatus::Idle; });
        emit userStopped();
    });
    connect(m_events, &PlaybackEventSource::SigVideoTotalSeconds, this, [this](int seconds) {
        mutateState([seconds](PlaybackState &state) { state.durationSeconds = seconds; });
        emit totalSecondsChanged(seconds);
    });
    connect(m_events, &PlaybackEventSource::SigVideoPlaySeconds, this, [this](int seconds) {
        mutateState([seconds](PlaybackState &state) { state.positionSeconds = seconds; });
        emit positionSecondsChanged(seconds);
    });
    connect(m_events, &PlaybackEventSource::SigVideoVolume, this, [this](double percent) {
        mutateState([percent](PlaybackState &state) { state.volume = percent; });
        emit volumeChanged(percent);
    });
    connect(m_events, &PlaybackEventSource::SigFrameDimensionsChanged,
            this, &PlaybackService::frameDimensionsChanged);
    connect(m_events, &PlaybackEventSource::SigSeekForwardCompleted,
            this, &PlaybackService::seekForwardCompleted);
    connect(m_events, &PlaybackEventSource::SigSeekBackCompleted,
            this, &PlaybackService::seekBackCompleted);
    connect(m_events, &PlaybackEventSource::SigError, this, [this](const QString &message) {
        reportError(PlaybackErrorCode::InternalError, message);
    });
}

PlaybackService::~PlaybackService()
{
    m_acceptCommands = false;
    m_events = nullptr;
    m_backend = nullptr;
}

void PlaybackService::start(const QString &locator, WId playWidgetId)
{
    if (locator.isEmpty()) {
        reportError(PlaybackErrorCode::InvalidLocator, tr("媒体地址为空"));
        return;
    }
    enqueue([this, locator, playWidgetId]() {
        m_backend->start_play(locator, playWidgetId);
    });
}

void PlaybackService::pause()
{
    enqueue([this]() { m_backend->OnPause(); });
}

void PlaybackService::stop()
{
    enqueue([this]() { m_backend->OnStop(); });
}

void PlaybackService::userStop()
{
    enqueue([this]() { m_backend->OnUserStop(); });
}

void PlaybackService::setVolume(double percent)
{
    enqueue([this, percent]() { m_backend->OnPlayVolume(percent); });
}

void PlaybackService::seek(double percent)
{
    enqueue([this, percent]() { m_backend->OnPlaySeek(percent); });
}

void PlaybackService::setSpeed(float speed)
{
    enqueue([this, speed]() { m_backend->OnSetSpeed(speed); });
}

void PlaybackService::seekForward()
{
    enqueue([this]() { m_backend->OnSeekForward(); });
}

void PlaybackService::seekBack()
{
    enqueue([this]() { m_backend->OnSeekBack(); });
}

void PlaybackService::addVolume()
{
    enqueue([this]() { m_backend->OnAddVolume(); });
}

void PlaybackService::subVolume()
{
    enqueue([this]() { m_backend->OnSubVolume(); });
}

void PlaybackService::step()
{
    enqueue([this]() { m_backend->OnStep(); });
}

void PlaybackService::extractAudio(const QString &inputFile, const QString &outputFile)
{
    enqueue([this, inputFile, outputFile]() {
        const bool success = m_backend->OnExtractAudio(inputFile, outputFile);
        emit audioExtractionFinished(success, inputFile, outputFile);
    });
}

void PlaybackService::captureFrame(const QString &outputFile)
{
    enqueue([this, outputFile]() {
        QString errorMessage;
        const bool success = m_backend->OnCaptureFrame(outputFile, &errorMessage);
        emit screenshotFinished(success, outputFile, errorMessage);
    });
}

void PlaybackService::setRenderTarget(WId playWidgetId)
{
    enqueue([this, playWidgetId]() { m_backend->OnSetRenderTarget(playWidgetId); });
}

PlaybackState PlaybackService::state() const
{
    QMutexLocker locker(&m_stateMutex);
    return m_state;
}

bool PlaybackService::isPlaying() const
{
    const PlaybackStatus status = state().status;
    return status == PlaybackStatus::Playing || status == PlaybackStatus::Seeking;
}

MediaInfo PlaybackService::mediaInfo() const
{
    return m_backend ? m_backend->mediaInfo() : MediaInfo{};
}

void PlaybackService::mutateState(const std::function<void(PlaybackState &)> &mutation)
{
    {
        QMutexLocker locker(&m_stateMutex);
        mutation(m_state);
    }
    publishState();
}

void PlaybackService::publishState()
{
    emit stateChanged(state());
}

void PlaybackService::reportError(PlaybackErrorCode code, const QString &message)
{
    mutateState([code, &message](PlaybackState &state) {
        state.status = PlaybackStatus::Error;
        state.errorCode = code;
        state.errorMessage = message;
    });
    emit errorOccurred(message);
    emit playbackError(code, message);
}

void PlaybackService::enqueue(std::function<void()> command)
{
    if (!m_acceptCommands) {
        return;
    }

    if (QThread::currentThread() == thread()) {
        command();
        return;
    }
    QMetaObject::invokeMethod(this, [command = std::move(command)]() {
        command();
    }, Qt::QueuedConnection);
}
