#include "playbacksessionmanager.h"

#include <QMetaObject>
#include <QThread>

PlaybackSessionManager::PlaybackSessionManager(QObject *parent)
    : QObject(parent)
{
}

PlaybackSessionManager::~PlaybackSessionManager()
{
    stopCommandThread();
}

int PlaybackSessionManager::createSession(PlaybackEventSource *events, IPlaybackBackend *backend)
{
    if (!events || !backend) {
        return -1;
    }

    if (!m_commandThread) {
        m_commandThread = new QThread(this);
        m_commandThread->setObjectName(QStringLiteral("PlaybackCommandThread"));
        m_commandThread->start();
    }

    const int id = m_nextSessionId++;

    /*
     * 服务不能有父对象，否则无法迁移线程；
     * 其生命周期由命令线程的 finished 信号统一收尾（Qt 会在所属线程结束时销毁它）。
     */
    auto *service = new PlaybackService(events, backend);
    service->moveToThread(m_commandThread);
    connect(m_commandThread, &QThread::finished, service, &QObject::deleteLater);

    m_sessions.insert(id, service);
    return id;
}

PlaybackService *PlaybackSessionManager::session(int sessionId) const
{
    return m_sessions.value(sessionId, nullptr);
}

bool PlaybackSessionManager::destroySession(int sessionId)
{
    PlaybackService *service = m_sessions.take(sessionId);
    if (!service) {
        return false;
    }

    // 会话对象位于命令线程，必须在其所属线程销毁
    QMetaObject::invokeMethod(service, [service]() { service->deleteLater(); },
                              Qt::QueuedConnection);
    return true;
}

QThread *PlaybackSessionManager::commandThread() const
{
    return m_commandThread;
}

void PlaybackSessionManager::stopCommandThread()
{
    // 会话对象由命令线程的 finished 信号负责销毁，这里只需停止线程并等待其退出
    m_sessions.clear();
    if (!m_commandThread) {
        return;
    }

    m_commandThread->quit();
    m_commandThread->wait();
}
