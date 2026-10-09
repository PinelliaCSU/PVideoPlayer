#include "playlist.h"
#include "ui_playlist.h"

#include "guiutils.h"
#include "medialist.h"
#include "playlistmodel.h"

Playlist::Playlist(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Playlist)
{
    ui->setupUi(this);
}

Playlist::~Playlist()
{
    delete ui;
}

bool Playlist::Init()
{
    if (!initUi() || !ui->List->Init()) {
        return false;
    }

    connectSignalSlots();
    setAcceptDrops(true);
    return true;
}

void Playlist::SetPlaylistModel(PlaylistModel *model)
{
    _model = model;
    ui->List->setModel(model);
}

void Playlist::SetCurrentIndex(int row)
{
    if (!_model || row < 0 || row >= _model->rowCount()) {
        return;
    }
    ui->List->setCurrentIndex(_model->index(row, 0));
}

void Playlist::connectSignalSlots()
{
    connect(ui->List, &MediaList::SigAddFile, this, &Playlist::SigAddRequested);
    connect(ui->List, &MediaList::doubleClicked, this, [this](const QModelIndex &index) {
        if (index.isValid()) {
            emit SigPlayIndexRequested(index.row());
        }
    });
    connect(ui->List, &MediaList::SigRemoveFile, this, &Playlist::SigRemoveRequested);
    connect(ui->List, &MediaList::SigClearList, this, &Playlist::SigClearRequested);
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
        emit SigAddRequested(url.toLocalFile());
    }
}

void Playlist::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}
