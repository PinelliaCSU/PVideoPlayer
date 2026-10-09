#include "playlistcoordinator.h"

#include <QRandomGenerator>

PlaylistCoordinator::PlaylistCoordinator(PlaylistModel *model, QObject *parent)
    : QObject(parent)
    , m_model(model)
{
    Q_ASSERT(m_model != nullptr);
}

int PlaylistCoordinator::currentIndex() const
{
    return m_currentIndex;
}

PlayMode PlaylistCoordinator::playMode() const
{
    return m_playMode;
}

void PlaylistCoordinator::setCurrentIndex(int index)
{
    if (index < -1 || index >= m_model->rowCount()) {
        return;
    }
    if (m_currentIndex == index) {
        return;
    }
    m_currentIndex = index;
    emit currentIndexChanged(m_currentIndex);
}

void PlaylistCoordinator::setPlayMode(PlayMode mode)
{
    m_playMode = mode;
}

void PlaylistCoordinator::playIndex(int index)
{
    if (index < 0 || index >= m_model->rowCount()) {
        return;
    }
    setCurrentIndex(index);
    emit playRequested(m_model->itemAt(index).locator);
}

void PlaylistCoordinator::playPrevious()
{
    const int count = m_model->rowCount();
    if (count == 0) {
        return;
    }
    playIndex(m_currentIndex <= 0 ? count - 1 : m_currentIndex - 1);
}

void PlaylistCoordinator::playNext()
{
    const int count = m_model->rowCount();
    if (count == 0) {
        return;
    }

    int nextIndex = m_currentIndex < 0 ? 0 : m_currentIndex;
    switch (m_playMode) {
    case PLAYMODE_REPEAT_ONE:
        break;
    case PLAYMODE_NORMAL:
        if (nextIndex >= count - 1) {
            return;
        }
        ++nextIndex;
        break;
    case PLAYMODE_SHUFFLE:
        if (count > 1) {
            do {
                nextIndex = QRandomGenerator::global()->bounded(count);
            } while (nextIndex == m_currentIndex);
        }
        break;
    case PLAYMODE_REPEAT_LIST:
    default:
        nextIndex = (nextIndex + 1) % count;
        break;
    }
    playIndex(nextIndex);
}
