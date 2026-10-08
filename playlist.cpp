#include "playlist.h"
#include "ui_playlist.h"
#include "guiutils.h"
#include "configutils.h"
#include "medialist.h"
#include "medialocator.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QRandomGenerator>

Playlist::Playlist(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Playlist)
    , _current_media_index(0)
    , _model(this)
    , _play_mode(PLAYMODE_REPEAT_LIST)
{
    ui->setupUi(this);
}

Playlist::~Playlist()
{
    QStringList playlist;
    for (const MediaItem &item : _model.items()) {
        playlist.append(item.locator);
    }
    ConfigUtils::SavePlaylist(playlist);
    delete ui;
}

bool Playlist::Init()
{
    if (!initUi() || !ui->List->Init()) {
        return false;
    }

    connectSignalSlots();
    _model.clear();
    ui->List->clear();

    QStringList savedPlaylist;
    ConfigUtils::LoadPlaylist(savedPlaylist);
    for (const QString &locator : savedPlaylist) {
        SlotOnAddFile(locator);
    }
    if (_model.rowCount() > 0) {
        ui->List->setCurrentRow(0);
    }

    setAcceptDrops(true);
    return true;
}

void Playlist::connectSignalSlots()
{
    connect(ui->List, &MediaList::SigAddFile, this, &Playlist::SlotOnAddFile);
    connect(ui->List, &MediaList::itemDoubleClicked,
            this, &Playlist::SlotOnPlayVideoFile);
    connect(ui->List, &MediaList::SigRemoveFile, this, [this](int row) {
        _model.removeAt(row);
        if (_current_media_index >= _model.rowCount()) {
            _current_media_index = qMax(0, _model.rowCount() - 1);
        }
    });
    connect(ui->List, &MediaList::SigClearList, this, [this]() {
        _model.clear();
        _current_media_index = 0;
    });
}

bool Playlist::addMediaItem(const QString &locator, bool showDuplicateMessage)
{
    const bool networkStream = MediaLocator::isNetworkStream(locator);
    const QString displayName = networkStream
        ? locator
        : QFileInfo(locator).fileName();

    if (!_model.addItem({locator, displayName, networkStream})) {
        if (showDuplicateMessage) {
            QMessageBox::information(
                this, tr("重复添加"),
                tr("文件 \"%1\" 已经在播放列表中存在。").arg(displayName));
        }
        return false;
    }

    rebuildView();
    return true;
}

void Playlist::rebuildView()
{
    ui->List->clear();
    for (const MediaItem &item : _model.items()) {
        auto *widgetItem = new QListWidgetItem(item.displayName, ui->List);
        widgetItem->setData(Qt::UserRole, item.locator);
        widgetItem->setToolTip(item.locator);
    }
}

void Playlist::SlotOnAddFile(QString filePath)
{
    QString locator;
    switch (MediaLocator::normalize(filePath, false, &locator, nullptr)) {
    case MediaLocator::LocatorStatus::Empty:
        qWarning() << "Playlist::SlotOnAddFile: empty locator";
        return;
    case MediaLocator::LocatorStatus::FileNotExists:
        QMessageBox::warning(
            this, tr("文件不存在"),
            tr("文件 \"%1\" 不存在，请检查路径是否正确。").arg(QFileInfo(filePath).fileName()));
        return;
    default:
        break;
    }
    addMediaItem(locator, true);
}

void Playlist::SlotOnPlayVideoFile(QListWidgetItem *item)
{
    if (item == nullptr) {
        return;
    }
    const int row = ui->List->row(item);
    if (row < 0 || row >= _model.rowCount()) {
        return;
    }
    _current_media_index = row;
    ui->List->setCurrentItem(item);
    emit SigPlay(_model.itemAt(row).locator);
}

void Playlist::SlotOnBackPlay()
{
    const int count = _model.rowCount();
    if (count == 0) {
        return;
    }
    playByIndex(_current_media_index == 0
                    ? count - 1
                    : _current_media_index - 1);
}

void Playlist::SlotOnNextPlay()
{
    const int count = _model.rowCount();
    if (count == 0) {
        return;
    }

    int newIndex = _current_media_index;
    switch (_play_mode) {
    case PLAYMODE_REPEAT_ONE:
        break;
    case PLAYMODE_NORMAL:
        if (_current_media_index >= count - 1) {
            return;
        }
        ++newIndex;
        break;
    case PLAYMODE_SHUFFLE:
        if (count > 1) {
            do {
                newIndex = QRandomGenerator::global()->bounded(count);
            } while (newIndex == _current_media_index);
        }
        break;
    case PLAYMODE_REPEAT_LIST:
    default:
        newIndex = (_current_media_index + 1) % count;
        break;
    }
    playByIndex(newIndex);
}

void Playlist::OnAddFileAndPlay(QString fileName)
{
    QString locator;
    switch (MediaLocator::normalize(fileName, true, &locator, nullptr)) {
    case MediaLocator::LocatorStatus::Empty:
    case MediaLocator::LocatorStatus::UnsupportedFormat:
        // 拖入空地址或不支持的媒体格式时静默忽略
        return;
    case MediaLocator::LocatorStatus::FileNotExists:
        QMessageBox::warning(
            this, tr("文件不存在"),
            tr("文件 \"%1\" 不存在，请检查路径是否正确。").arg(QFileInfo(fileName).fileName()));
        return;
    default:
        break;
    }

    int row = _model.indexOf(locator);
    if (row < 0) {
        addMediaItem(locator, false);
        row = _model.rowCount() - 1;
    }
    playByIndex(row);
}

bool Playlist::initUi()
{
    setStyleSheet(GuiUtils::LoadQss(":/res/qss/playlist.css"));
    return true;
}

void Playlist::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        SlotOnAddFile(url.toLocalFile());
    }
}

void Playlist::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void Playlist::playByIndex(int index)
{
    if (index < 0 || index >= _model.rowCount()) {
        return;
    }
    _current_media_index = index;
    ui->List->setCurrentRow(index);
    emit SigPlay(_model.itemAt(index).locator);
}

void Playlist::SetPlayMode(PlayMode mode)
{
    _play_mode = mode;
}

PlayMode Playlist::GetPlayMode() const
{
    return _play_mode;
}
