#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractListModel>
#include <QList>

#include "media_types.h"

class PlaylistModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        LocatorRole = Qt::UserRole + 1,
        DisplayNameRole,
        NetworkStreamRole
    };

    explicit PlaylistModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool addItem(const MediaItem &item);
    bool removeAt(int row);
    void clear();
    MediaItem itemAt(int row) const;
    QList<MediaItem> items() const;
    int indexOf(const QString &locator) const;

private:
    QList<MediaItem> m_items;
};

#endif // PLAYLISTMODEL_H
