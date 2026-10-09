#ifndef PLAYLISTCOORDINATOR_H
#define PLAYLISTCOORDINATOR_H

#include <QObject>

#include "media_types.h"
#include "playlistmodel.h"

class PlaylistCoordinator final : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistCoordinator(PlaylistModel *model, QObject *parent = nullptr);

    int currentIndex() const;
    PlayMode playMode() const;

    void setCurrentIndex(int index);
    void setPlayMode(PlayMode mode);
    void playIndex(int index);
    void playPrevious();
    void playNext();

signals:
    void playRequested(const QString &locator);
    void currentIndexChanged(int index);

private:
    PlaylistModel *m_model;
    int m_currentIndex = -1;
    PlayMode m_playMode = PLAYMODE_REPEAT_LIST;
};

#endif // PLAYLISTCOORDINATOR_H
