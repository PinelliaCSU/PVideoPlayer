#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMouseEvent>
#include <QPoint>
#include <QMenu>

#include "playlist.h"
#include "title.h"

class AppController;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/*
 * 主窗口：只负责窗口级行为和 UI 编排。
 * 播放业务（播放列表、历史、续播、控制栏策略）全部由 AppController 承担，
 * 本类只把界面信号接到控制器，并把控制器信号接到界面显示。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(AppController *controller, QWidget *parent = nullptr);
    ~MainWindow() override;
    bool Init();

protected:
    //    解决窗口无法拖动
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void connectUiSignals();
    void connectControllerSignals();
    void registerShortcuts();

    void SlotOnMinBtnClicked();
    void SlotOnMaxBtnClicked();
    void SlotOnFullScreenBtnClicked();
    void SlotOnCloseBtnClicked();
    void SlotOnMenuBtnClicked();
    void SlotOnPlayListCtrlBtnClicked();

    //    初始化菜单，并添加 action 的槽函数
    void initMenu();
    void SlotOnAlwaysOnTopToggled(bool on);
    void SlotOnExtractAudio();
    void SlotOnShowMediaInfo();
    void ShowControlBar();
    void OnResumeAvailable(const QString &locator, int positionSeconds, int totalSeconds);

private:
    Ui::MainWindow *ui;
    AppController *_controller;
    Playlist _playlist;
    Title _title;
    QMenu _menu;
    bool _move_drag;
    QPoint _drag_position;
};

#endif // MAINWINDOW_H
