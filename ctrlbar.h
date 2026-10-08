#ifndef CTRLBAR_H
#define CTRLBAR_H

#include <QWidget>
#include <QMenu>

// 倍速菜单参数
#define SPEED_MENU_MIN           (0.5)     // 菜单最小倍速
#define SPEED_MENU_MAX           (3.0)     // 菜单最大倍速
#define SPEED_MENU_SCALE         (0.5)     // 菜单倍速步长

namespace Ui {
class CtrlBar;
}

class CtrlBar : public QWidget
{
    Q_OBJECT

public:
    explicit CtrlBar(QWidget *parent = nullptr);
    ~CtrlBar();
    bool Init();
    void OnSpeed(float speed);
    void OnPauseStat(bool paused);
    void OnStopFinished();
    void OnUserStopFinished();//主动停止

    void OnVideopVolume(double percent);
    void OnVideoTotalSeconds(int seconds);
    void OnVideoPlaySeconds(int seconds);
    void OnPlaySliderValueChanged();

    // 快捷键触发的 Toast 提示
    void OnSeekForward(int targetSeconds);
    void OnSeekBack(int targetSeconds);
    void OnVolumeChanged(double percent);  // 快捷键音量变化
    void OnAudioModeChanged(int mode);     // 音频模式切换回调（更新按钮文字）
private:
    bool initUi();
    void connectSignalSlots();
    void SlotOnVolumeBtnClicked();
    void OnVolumeSliderValueChanged();
signals:
    void SigPlayListCtlBtnClicked();
    void SigBackBtnClicked();
    void SigNextBtnClicked();
    void SigSpeedChanged(float speed); // 选择指定倍速
    void SigPlayOrPause();
    void SigStop();
    void SigPlayVolume(double percent);
    void SigPlaySeek(double percent);
    void SigShowToast(const QString &text);  // 显示提示信息的信号

    void SigPlayModeChanged(int mode);        // 播放模式切换
    void SigAlwaysOnTopToggled(bool on);    //窗口置顶切换

    void SigExtractAudio(); //获取音频信号
private slots:
    void on_SpeedBtn_clicked();
    void OnSpeedMenuTriggered(QAction* action);


    void on_PlayOrPauseBtn_clicked();
    void on_OverPlayBtn_clicked();
private:
    Ui::CtrlBar *ui;
    double _last_volume_percent;    //变为静音前的音量大小
    int _total_play_seconds;
    int _last_play_seconds; //上次播放时间
    QMenu *_speed_menu;     // 倍速下拉菜单
    QMenu *_setting_menu; //设置菜单

    QAction *_always_on_top_action; // 窗口置顶动作
};

#endif // CTRLBAR_H
