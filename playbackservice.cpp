#include "playbackservice.h"

#include <QMetaObject>
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

    connect(m_events, &PlaybackEventSource::SigStartPlay, this, [this](const QString &locator) {
        m_state.currentLocator = locator;
        m_state.status = PlaybackStatus::Opening;
        emit started(locator);
    });
    connect(m_events, &PlaybackEventSource::SigSpeed, this, [this](float speed) {
        m_state.speed = speed;
        emit speedChanged(speed);
    });
    connect(m_events, &PlaybackEventSource::SigPauseStat, this, [this](bool paused) {
        m_state.status = paused ? PlaybackStatus::Paused : PlaybackStatus::Playing;
        emit pauseChanged(paused);
    });
    connect(m_events, &PlaybackEventSource::SigStopFinished, this, [this]() {
        m_state.status = PlaybackStatus::Finished;
        emit finished();
    });
    connect(m_events, &PlaybackEventSource::SigUserStopFinished, this, [this]() {
        m_state.status = PlaybackStatus::Idle;
        emit userStopped();
    });
    connect(m_events, &PlaybackEventSource::SigVideoTotalSeconds, this, [this](int seconds) {
        m_state.durationSeconds = seconds;
        emit totalSecondsChanged(seconds);
    });
    connect(m_events, &PlaybackEventSource::SigVideoPlaySeconds, this, [this](int seconds) {
        m_state.positionSeconds = seconds;
        emit positionSecondsChanged(seconds);
    });
    connect(m_events, &PlaybackEventSource::SigVideoVolume, this, [this](double percent) {
        m_state.volume = percent;
        emit volumeChanged(percent);
    });
    connect(m_events, &PlaybackEventSource::SigFrameDimensionsChanged,
            this, &PlaybackService::frameDimensionsChanged);
    connect(m_events, &PlaybackEventSource::SigSeekForwardCompleted,
            this, &PlaybackService::seekForwardCompleted);
    connect(m_events, &PlaybackEventSource::SigSeekBackCompleted,
            this, &PlaybackService::seekBackCompleted);
    connect(m_events, &PlaybackEventSource::SigError, this, [this](const QString &message) {
        m_state.status = PlaybackStatus::Error;
        emit errorOccurred(message);
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
        emit errorOccurred(tr("媒体地址为空"));
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

bool PlaybackService::extractAudio(const QString &inputFile, const QString &outputFile)
{
    return m_backend->OnExtractAudio(inputFile, outputFile);
}

PlaybackState PlaybackService::state() const
{
    return m_state;
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
