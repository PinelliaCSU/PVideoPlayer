#include "playlistmodel.h"

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const MediaItem &item = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return item.displayName;
    case LocatorRole:
        return item.locator;
    case NetworkStreamRole:
        return item.isNetworkStream;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlaylistModel::roleNames() const
{
    return {
        {LocatorRole, "locator"},
        {DisplayNameRole, "displayName"},
        {NetworkStreamRole, "networkStream"}
    };
}

bool PlaylistModel::addItem(const MediaItem &item)
{
    if (item.locator.isEmpty() || indexOf(item.locator) >= 0) {
        return false;
    }

    const int row = m_items.size();
    beginInsertRows(QModelIndex(), row, row);
    m_items.append(item);
    endInsertRows();
    return true;
}

bool PlaylistModel::removeAt(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return false;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_items.removeAt(row);
    endRemoveRows();
    return true;
}

void PlaylistModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }
    beginResetModel();
    m_items.clear();
    endResetModel();
}

MediaItem PlaylistModel::itemAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }
    return m_items.at(row);
}

QList<MediaItem> PlaylistModel::items() const
{
    return m_items;
}

int PlaylistModel::indexOf(const QString &locator) const
{
    if (locator.isEmpty()) {
        return -1;
    }
    for (int row = 0; row < m_items.size(); ++row) {
        if (m_items.at(row).locator == locator) {
            return row;
        }
    }
    return -1;
}
