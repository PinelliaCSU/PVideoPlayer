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
    void OnPlay(QString strFile);
    void OnFrameDimensionsChanged(int nFrameWidth, int nFrameHeight);
    void ShowToast(const QString &text);
    void ShowShortcutHint(const QString &text); // 全屏快捷键提示（居中显示）
    void HideShortcutHint(); // 强制隐藏快捷键提示
    QString getCurrentFile();
    void OnStartPlay(QString filename);
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

signals:
    void SigPlay(QString strFile);
    void SigOpenFile(QString strFile);
    void SigExitFullScreen();
    void SigTogglePlay();   //点击视频实现暂停
private slots:
    void OnToastTimeout();
    void OnShortcutHintTimeout();
private:
    Ui::Show *ui;

    int _nLastFrameWidth; //缓存当前播放视频帧的原始宽度和高度，与保持原始图像的宽高比
    int _nLastFrameHeight;

    QString _current_file; // 当前播放的文件路径

    QLabel *_toastLabel;  // 提示标签（使用顶层窗口实现透明，右上角显示）
    QTimer *_toastTimer;  // 提示定时器

    QLabel *_shortcutHintLabel;  // 快捷键提示标签（居中显示）
    QTimer *_shortcutHintTimer;  // 快捷键提示定时器
};

#endif // SHOW_H
