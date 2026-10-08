#ifndef SHOW_H
#define SHOW_H

#include <QWidget>
#include<QDropEvent>
#include<QDragEnterEvent>
#include<QMimeData>
#include<QResizeEvent>
#include<QLabel>
#include<QTimer>
#include<QEvent>

class PlaybackService;
class QVBoxLayout;
namespace Ui {
class Show;
}

class Show : public QWidget
{
    Q_OBJECT

public:
    explicit Show(QWidget *parent = nullptr);
    ~Show();
    bool Init();
    void SetPlaybackService(PlaybackService *service);
    void OnPlay(QString strFile);
    void OnFrameDimensionsChanged(int nFrameWidth, int nFrameHeight);
    void ShowToast(const QString &text);
    void ShowShortcutHint(const QString &text); // 全屏快捷键提示（居中显示）
    void HideShortcutHint(); // 强制隐藏快捷键提示
    QString getCurrentFile();
    void OnStartPlay(QString filename);

    // 画中画：把视频画面移入可拖动的置顶悬浮小窗，主窗口继续可用，播放不中断
    bool IsPipActive() const;
    void SetPipActive(bool active);
protected:
    void dropEvent(QDropEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void resizeEvent(QResizeEvent *event);
    void keyReleaseEvent(QKeyEvent *event); // 键盘事件处理
    bool eventFilter(QObject *obj, QEvent *event);
private:
    void ChangeShow();
    bool initUi();
    bool connectionSignalSlots();
    void OnVideoContextMenuRequested(const QPoint &pos);

signals:
    void SigPlay(QString strFile);
    void SigOpenFile(QString strFile);
    void SigExitFullScreen();
    void SigTogglePlay();   //点击视频实现暂停
    void SigPipActiveChanged(bool active);
private slots:
    void OnToastTimeout();
    void OnShortcutHintTimeout();
private:
    Ui::Show *ui;

    int _nLastFrameWidth; //缓存当前播放视频帧的原始宽度和高度，与保持原始图像的宽高比
    int _nLastFrameHeight;

    QString _current_file; // 当前播放的文件路径
    PlaybackService *_playback_service = nullptr;

    QWidget *_pipWindow = nullptr;       // 画中画悬浮窗口（无边框、置顶）
    QVBoxLayout *_pipLayout = nullptr;   // 悬浮窗口内的视频容器布局
    bool _pipActive = false;             // 是否处于画中画模式
    bool _pipUserMoved = false;          // 用户是否拖动过悬浮窗（拖动后不再自动定位）
    bool _pipDragging = false;           // 是否正在拖动悬浮窗
    bool _pipDragMoved = false;          // 本次按下是否产生了拖动
    QPoint _pipDragOffset;               // 拖动时鼠标相对悬浮窗左上角的偏移

    QLabel *_toastLabel;  // 提示标签（使用顶层窗口实现透明，右上角显示）
    QTimer *_toastTimer;  // 提示定时器

    QLabel *_shortcutHintLabel;  // 快捷键提示标签（居中显示）
    QTimer *_shortcutHintTimer;  // 快捷键提示定时器
};

#endif // SHOW_H
