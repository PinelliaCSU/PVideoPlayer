#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QMouseEvent>
#include <QPoint>
#include <QMenu>


#include "playlist.h"
#include "title.h"

class PlaybackService;
class PlaybackSessionManager;
class PlaybackEventSource;
class IPlaybackBackend;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(PlaybackEventSource *events, IPlaybackBackend *backend, QWidget *parent = nullptr);
    ~MainWindow();
    bool Init();
protected:
    //    解决窗口无法拖动
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    //    键盘事件已通过全局QAction处理
private:
    void connectSignalSlots();

    void SlotOnMinBtnClicked();
    void SlotOnMaxBtnClicked();
    void SlotOnFullScreenBtnClicked();
    void SlotOnCloseBtnClicked();
    void SlotOnMenuBtnClicked();

    void SlotOnPlayListCtrlBtnClicked();

    //    初始化菜单，并添加 action 的槽函数
    void initMenu();
    //设置菜单切换播放模式与窗口置顶
    void SlotOnPlayModeChanged(int mode);
    void SlotOnAlwaysOnTopToggled(bool on);
    //获取音频按钮
    void SlotOnExtractAudio();

    void SlotOnBeforeNewPlay(QString filename);
    void SlotOnSavePlaybackPosition();             // 停止时保存播放位置
    void SlotOnCachePlaySeconds(int seconds);       // 缓存当前播放时间
    void SlotOnCacheTotalSeconds(int seconds);      // 缓存总时长
    void SlotOnCheckResume(int totalSeconds); // 开始播放后检查续播
    void UpdatePlayHistory(const QString& filePath); // 更新播放历史
     void SlotOnClearPlayHistory(); //清除全部播放记录
signals:
    void SigSeekForward();
    void SigSeekBack();
    void SigAddVolume();
    void SigSubVolume();
    void SigPlayOrPause();
    void SigStep(); // 逐帧播放
private:
    Ui::MainWindow *ui;
public:
    Playlist _playlist;
    Title _title;
private:
    bool _move_drag;
    QPoint _drag_position;
    QMenu _menu;
    PlaybackService *_playback_service;
    PlaybackSessionManager *_session_manager;

    QString _playing_file_path;      // 当前正在跟踪播放位置的文件路径
    int  _cached_total_seconds = 0;   // 当前视频总时长
    int  _cached_play_seconds  = 0;   // 当前播放位置（实时缓存）
    bool _resume_checked = false;     // 是否已经检查过续播（防止重复弹窗）


};
#endif // MAINWINDOW_H
