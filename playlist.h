#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QWidget>
#include <QListWidget>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>

namespace Ui {
class Playlist;
}


// 播放模式枚举
enum PlayMode {
    PLAYMODE_REPEAT_LIST = 0,   // 列表循环（默认）
    PLAYMODE_NORMAL,            // 顺序播放
    PLAYMODE_REPEAT_ONE,        // 循环播放
    PLAYMODE_SHUFFLE            // 随机播放
};

class Playlist : public QWidget
{
    Q_OBJECT

public:
    explicit Playlist(QWidget *parent = nullptr);
    ~Playlist();
    bool Init();

    void SlotOnAddFile(QString filePath);
    void SlotOnPlayVideoFile(QListWidgetItem * item); //双击播放
    void SlotOnBackPlay();
    void SlotOnNextPlay();
    void OnAddFileAndPlay(QString strFileName);
    //播放模式
    void SetPlayMode(PlayMode mode);   //设置播放模式
    PlayMode GetPlayMode() const;

protected:
    //    鼠标拖拽放下事件
    void dropEvent(QDropEvent *event);
    //    鼠标拖动事件
    void dragEnterEvent(QDragEnterEvent *event);
private:
    bool initUi();
    void connectSignalSlots();

    void playByIndex(int index);//播放指定索引
signals:
    void SigPlay(QString filePath);
private:
    Ui::Playlist *ui;
    int _current_media_index;

    PlayMode _play_mode;//当前播放模式
};

#endif // PLAYLIST_H
