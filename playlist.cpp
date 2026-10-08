#include "playlist.h"
#include "ui_playlist.h"
#include "guiutils.h"
#include "configutils.h"
#include "medialist.h"
#include <QDir>
#include <QMessageBox>
#include <QRandomGenerator>


Playlist::Playlist(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Playlist),
    _current_media_index(0),
    _play_mode(PLAYMODE_REPEAT_LIST)
{
    ui->setupUi(this);
}

Playlist::~Playlist()
{
    QStringList strPlayList;
    for(int i = 0;i < ui->List->count();i++){
        strPlayList.append(ui->List->item(i)->toolTip());
    }
    ConfigUtils::SavePlaylist(strPlayList);

    delete ui;
}

bool Playlist::Init(){
    //UI初始化检测
    if(initUi() == false){
        return false;
    }

    if(ui->List->Init() == false){
        return false;
    }

    //连接信号与槽函数
    connectSignalSlots();

    //清空播放列表
    ui->List->clear();
    //从配置中获取文件列表
    QStringList strPlayList;

    ConfigUtils::LoadPlaylist(strPlayList);
    for(const QString & videoFile: strPlayList){
        //加入播放列表
        SlotOnAddFile(videoFile);
    }

    //暂时选取第一个视频播放
    if(strPlayList.length() > 0){
        ui->List->setCurrentRow(0);
    }

    //开启QT控制拖拽功能
    setAcceptDrops(true);

    return true;
}

void Playlist::connectSignalSlots()
{
    connect(ui->List, &MediaList::SigAddFile, this, &Playlist::SlotOnAddFile);
    connect(ui->List, &MediaList::itemDoubleClicked, this, &Playlist::SlotOnPlayVideoFile);
}

void Playlist::SlotOnAddFile(QString filePath){
    if(filePath.isEmpty()) {
        qDebug() << "filePath is empty";
        return;
    }

    // 判断是否为网络流
    bool isNetworkStream = GuiUtils::CheckNetworkStream(filePath);

    QString absolutePath;
    if (!isNetworkStream) {
        QFileInfo fileInfo(filePath);
        if(!fileInfo.exists()) {
            QMessageBox::warning(this, tr("文件不存在"),
                                 tr("文件 \"%1\" 不存在，请检查路径是否正确。").arg(fileInfo.fileName()));
            qDebug() << "文件不存在:" << filePath;
            return;
        }
        absolutePath = fileInfo.absoluteFilePath();
    } else {
        absolutePath = filePath;
    }

    // 查重
    bool isExist = false;
    for(int i = 0; i < ui->List->count(); i++) {
        QListWidgetItem *item = ui->List->item(i);
        QString existingPath = item->data(Qt::UserRole).toString();
        if(existingPath == absolutePath) {
            isExist = true;
            break;
        }
    }

    if(!isExist) {
        QListWidgetItem *p_item = new QListWidgetItem(ui->List);
        p_item->setData(Qt::UserRole, QVariant(absolutePath));
        p_item->setToolTip(absolutePath);
        if (isNetworkStream) {
            p_item->setText(absolutePath);  // 网络流直接显示完整 URL
        } else {
            p_item->setText(QFileInfo(filePath).fileName());
        }
        ui->List->addItem(p_item);
    } else {
        QString dupName = isNetworkStream ? absolutePath : QFileInfo(filePath).fileName();
        QMessageBox::information(this, tr("重复添加"),
                                 tr("文件 \"%1\" 已经在播放列表中存在。").arg(dupName));
    }
}


void Playlist::SlotOnPlayVideoFile(QListWidgetItem * item){
    //发送播放信号
    emit SigPlay(item->data(Qt::UserRole).toString());

    ui->List->setCurrentItem(item);
    _current_media_index = ui->List->row(item);

}

void Playlist::SlotOnBackPlay()
{
    int count = ui->List->count();
    if (count == 0) return;

    int newIndex;
    if (_current_media_index == 0) {
        newIndex = count - 1;  // 回到最后一个
    } else {
        newIndex = _current_media_index - 1;
    }
    playByIndex(newIndex);
}

// ===== 改造：下一首（根据模式决定行为） =====
void Playlist::SlotOnNextPlay()
{
    int count = ui->List->count();
    if (count == 0) return;

    int newIndex;
    switch (_play_mode) {
    case PLAYMODE_REPEAT_ONE:
        // 单曲循环：重新播放当前项
        newIndex = _current_media_index;
        break;

    case PLAYMODE_NORMAL:
        // 顺序播放：不是最后一个就继续，否则停止
        if (_current_media_index >= count - 1) {
            return; // 播完了，不做任何操作
        }
        newIndex = _current_media_index + 1;
        break;

    case PLAYMODE_SHUFFLE:
        // 随机播放：随机选一个（排除当前项，只有1项时不变）
        if (count == 1) {
            newIndex = 0;
        } else {
            do {
                newIndex = QRandomGenerator::global()->bounded(count);
            } while (newIndex == _current_media_index);
        }
        break;

    case PLAYMODE_REPEAT_LIST:
    default:
        // 列表循环：到末尾就回到开头（原有行为）
        if (_current_media_index >= count - 1) {
            newIndex = 0;
        } else {
            newIndex = _current_media_index + 1;
        }
        break;
    }
    playByIndex(newIndex);
}

void Playlist::OnAddFileAndPlay(QString strFileName)
{
    // 判断是否为网络流
    bool isNetworkStream = GuiUtils::CheckNetworkStream(strFileName);

    if (!isNetworkStream) {
        // 本地文件：扩展名校验
        bool supportMovie = strFileName.endsWith(".mkv", Qt::CaseInsensitive) ||
                            strFileName.endsWith(".rmvb", Qt::CaseInsensitive) ||
                            strFileName.endsWith(".mp4", Qt::CaseInsensitive) ||
                            strFileName.endsWith(".avi", Qt::CaseInsensitive) ||
                            strFileName.endsWith(".flv", Qt::CaseInsensitive) ||
                            strFileName.endsWith(".wmv", Qt::CaseInsensitive) ||
                            strFileName.endsWith(".3gp", Qt::CaseInsensitive);
        if (!supportMovie) {
            return;
        }

        QFileInfo fileInfo(strFileName);
        if(!fileInfo.exists()) {
            QMessageBox::warning(this, tr("文件不存在"),
                                 tr("文件 \"%1\" 不存在，请检查路径是否正确。").arg(fileInfo.fileName()));
            qDebug() << "文件不存在:" << strFileName;
            return;
        }

        QString absolutePath = fileInfo.absoluteFilePath();

        QListWidgetItem *pItem = nullptr;
        bool isExist = false;
        for(int i = 0; i < ui->List->count(); i++) {
            QListWidgetItem *item = ui->List->item(i);
            QString existingPath = item->data(Qt::UserRole).toString();
            if(existingPath == absolutePath) {
                pItem = item;
                isExist = true;
                break;
            }
        }

        if(!isExist) {
            pItem = new QListWidgetItem(ui->List);
            pItem->setData(Qt::UserRole, QVariant(absolutePath));
            pItem->setText(fileInfo.fileName());
            pItem->setToolTip(absolutePath);
            ui->List->addItem(pItem);
        }

        SlotOnPlayVideoFile(pItem);
    } else {
        // 网络流：直接添加并播放
        QListWidgetItem *pItem = nullptr;
        bool isExist = false;
        for(int i = 0; i < ui->List->count(); i++) {
            QListWidgetItem *item = ui->List->item(i);
            QString existingPath = item->data(Qt::UserRole).toString();
            if(existingPath == strFileName) {
                pItem = item;
                isExist = true;
                break;
            }
        }

        if(!isExist) {
            pItem = new QListWidgetItem(ui->List);
            pItem->setData(Qt::UserRole, QVariant(strFileName));
            pItem->setText(strFileName);   // 显示完整 URL
            pItem->setToolTip(strFileName);
            ui->List->addItem(pItem);
        }

        SlotOnPlayVideoFile(pItem);
    }
}
bool Playlist::initUi(){
    setStyleSheet(GuiUtils::LoadQss(":/res/qss/playlist.css"));
    return true;
}

void Playlist::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if(urls.isEmpty()) {
        return;
    }
    for(const QUrl& url: urls) {
        QString strFileName = url.toLocalFile();
        SlotOnAddFile(strFileName);
    }
}

void Playlist::dragEnterEvent(QDragEnterEvent *event)
{
    //    允许当前拖拽生效，使得事件往后续传递，即给 dropEvent 可以响应
    event->acceptProposedAction();
}


//播放模式相关

// 提取公共的"按索引播放"方法
void Playlist::playByIndex(int index)
{
    if (index < 0 || index >= ui->List->count()) {
        return;
    }
    _current_media_index = index;

    SlotOnPlayVideoFile(ui->List->item(index));
}

// 模式设置/获取
void Playlist::SetPlayMode(PlayMode mode)
{
    _play_mode = mode;
}

PlayMode Playlist::GetPlayMode() const
{
    return _play_mode;
}


