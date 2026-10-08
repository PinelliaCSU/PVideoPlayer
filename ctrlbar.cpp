#include "ctrlbar.h"
#include "ui_ctrlbar.h"
#include <QActionGroup>
#include "guiutils.h"
#include "configutils.h"
#include "playlist.h"

CtrlBar::CtrlBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CtrlBar),
    _last_volume_percent(0.5),
    _total_play_seconds(0),
    _last_play_seconds(-1)
{
    ui->setupUi(this);
}

CtrlBar::~CtrlBar()
{
    delete ui;
}


bool CtrlBar::Init(){
    if(initUi() == false){
        return false;
    }

    connectSignalSlots();

    double percent = -1.0;

    ConfigUtils::LoadVolume(percent);
    if(percent != -1.0) {
        OnVideopVolume(percent);
        emit SigPlayVolume(percent);
    } else {
        OnVideopVolume(_last_volume_percent);
        emit SigPlayVolume(_last_volume_percent);
    }


    return true;
}

void CtrlBar::OnSpeed(float speed)
{
    ui->SpeedBtn->setText(QString("倍数:%1").arg(speed));

    // 同步菜单选中状态
    for (QAction *action : _speed_menu->actions()) {
        action->setChecked(qFuzzyCompare(action->data().toFloat(), speed));
    }
}

bool CtrlBar::initUi(){
    setStyleSheet(GuiUtils::LoadQss(":/res/qss/ctrlbar.css"));

    //    设置按钮图片
    GuiUtils::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
    GuiUtils::SetIcon(ui->OverPlayBtn, 12, QChar(0xf04d));
    GuiUtils::SetIcon(ui->BackBtn, 12, QChar(0xf048));
    GuiUtils::SetIcon(ui->NextBtn, 12, QChar(0xf051));
    GuiUtils::SetIcon(ui->PlayListCtlBtn, 12, QChar(0xf036));
    GuiUtils::SetIcon(ui->SettingBtn, 12, QChar(0xf013));
    GuiUtils::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));

    //    设置鼠标悬浮提示
    ui->PlayOrPauseBtn->setToolTip("点击播放");
    ui->OverPlayBtn->setToolTip("结束播放");
    ui->BackBtn->setToolTip("上一个");
    ui->NextBtn->setToolTip("下一个");
    ui->PlayListCtlBtn->setToolTip("播放列表");
    ui->SettingBtn->setToolTip("设置");
    ui->VolumeBtn->setToolTip("点击静音");
    ui->SpeedBtn->setToolTip("倍速");

    _speed_menu = new QMenu(this);
    _speed_menu->setObjectName("SpeedMenu");

    // 使用 QActionGroup 实现互斥单选，防止点击已选中项时取消选中
    QActionGroup speedGroup(this);
    for (float speed = SPEED_MENU_MIN; speed <= SPEED_MENU_MAX + 0.001f; speed += SPEED_MENU_SCALE) {
        QString text = QString("%1x").arg(speed, 0, 'f', 1);
        QAction *action = _speed_menu->addAction(text);
        action->setData(speed);
        action->setCheckable(true);
        speedGroup.addAction(action);
        // 启动时默认选择1.0倍速
        if (qFuzzyCompare(speed, 1.0f)) {
            action->setChecked(true);
        }
    }
    connect(_speed_menu, &QMenu::triggered, this, &CtrlBar::OnSpeedMenuTriggered);

    //创建设置菜单
    _setting_menu = new QMenu(this);
    _setting_menu->setObjectName("SettingMenu");

    // ── 播放模式子菜单 ──
    QMenu *playModeMenu = _setting_menu->addMenu(tr("播放模式"));
    playModeMenu->setObjectName("PlayModeMenu");
    QActionGroup *playModeGroup = new QActionGroup(this);

    struct {
        QString text;
        int mode;
    } playModes[] = {
        {tr("列表循环"), PLAYMODE_REPEAT_LIST},
        {tr("顺序播放"), PLAYMODE_NORMAL},
        {tr("循环播放"), PLAYMODE_REPEAT_ONE},
        {tr("随机播放"), PLAYMODE_SHUFFLE}
    };

    for (const auto &item : playModes) {
        QAction *action = playModeMenu->addAction(item.text);
        action->setData(item.mode);
        action->setCheckable(true);
        playModeGroup->addAction(action);
        if (item.mode == PLAYMODE_REPEAT_LIST) {
            action->setChecked(true);
        }
    }

    // ── 分隔线 ──
    _setting_menu->addSeparator();

    // ── 窗口置顶 ──
    _always_on_top_action = _setting_menu->addAction(tr("窗口置顶"));
    _always_on_top_action->setCheckable(true);
    _always_on_top_action->setChecked(false);

    // 连接播放模式菜单触发
    connect(playModeMenu, &QMenu::triggered, this, [this](QAction *action) {
        int mode = action->data().toInt();
        emit SigPlayModeChanged(mode);
        // 显示 Toast 提示
        QString modeNames[] = {tr("列表循环"), tr("顺序播放"), tr("单曲循环"), tr("随机播放")};
        if (mode >= 0 && mode < 4) {
            emit SigShowToast(tr("播放模式：%1").arg(modeNames[mode]));
        }
    });

    //设置提取音频按钮相关
    ui->ExtractAudioBtn->setToolTip("提取视频中的音频并保存为文件");
    GuiUtils::SetIcon(ui->ExtractAudioBtn, 12, QChar(0xf025));  // fa-headphones 图标
    return true;
}


void CtrlBar::connectSignalSlots()
{
    //    todo：SettingBtn 是显示出来调节视频的参数，比如编码格式，编码效率等，等后续再开发
    connect(ui->PlayListCtlBtn, &QPushButton::clicked, this, &CtrlBar::SigPlayListCtlBtnClicked);
    connect(ui->VolumeBtn, &QPushButton::clicked, this, &CtrlBar::SlotOnVolumeBtnClicked);
    connect(ui->BackBtn, &QPushButton::clicked, this, &CtrlBar::SigBackBtnClicked);
    connect(ui->NextBtn, &QPushButton::clicked, this, &CtrlBar::SigNextBtnClicked);
    connect(ui->VolumeSlider, &CustomSlider::SigSliderValueChanged, this, &CtrlBar::OnVolumeSliderValueChanged);
    connect(ui->PlaySlider, &CustomSlider::SigSliderValueChanged, this, &CtrlBar::OnPlaySliderValueChanged);

    // 连接 SettingBtn 点击 → 弹出设置菜单
    connect(ui->SettingBtn, &QPushButton::clicked, this, [this]() {
        _setting_menu->exec(ui->SettingBtn->mapToGlobal(
            QPoint(0, ui->SettingBtn->height())));
    });


    // 连接窗口置顶切换
    connect(_always_on_top_action, &QAction::toggled, this, &CtrlBar::SigAlwaysOnTopToggled);

    //连接获取音频按钮
    connect(ui->ExtractAudioBtn,&QPushButton::clicked,this,&CtrlBar::SigExtractAudio);
}


void CtrlBar::OnPauseStat(bool paused){
    if(paused){
        GuiUtils::SetIcon(ui->PlayOrPauseBtn,12,QChar(0xf04b));
        ui->PlayOrPauseBtn->setToolTip("点击播放");
    }
    else
    {
        //        此时状态为播放状态，按钮变为暂停按钮，提示为点击暂停
        GuiUtils::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04c));
        ui->PlayOrPauseBtn->setToolTip("点击暂停");
    }
}

void CtrlBar::OnStopFinished()
{
    ui->PlaySlider->setValue(0);
    QTime stopTime(0, 0, 0);
    ui->VideoPlayTimeTimeEdit->setTime(stopTime);
    GuiUtils::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
    ui->PlayOrPauseBtn->setToolTip("点击播放");

    _last_play_seconds = -1;
}

void CtrlBar::OnUserStopFinished()
{
    ui->PlaySlider->setValue(0);
    QTime stopTime(0, 0, 0);
    ui->VideoTotalTimeTimeEdit->setTime(stopTime);
    ui->VideoPlayTimeTimeEdit->setTime(stopTime);
    GuiUtils::SetIcon(ui->PlayOrPauseBtn, 12, QChar(0xf04b));
    ui->PlayOrPauseBtn->setToolTip("点击播放");

    _last_play_seconds = -1;
    _total_play_seconds = 0;
}

void CtrlBar::OnVideopVolume(double percent)
{
    ui->VolumeSlider->setValue(percent * ConfigUtils::MAX_SLIDER_VALUE);
    _last_volume_percent = percent;

    if (_last_volume_percent == 0)
    {
        GuiUtils::SetIcon(ui->VolumeBtn, 12, QChar(0xf026));
        ui->VolumeBtn->setToolTip("点击恢复音量");
    }
    else
    {
        GuiUtils::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
        ui->VolumeBtn->setToolTip("点击静音");
    }
    ConfigUtils::SaveVolume(percent);

    // 显示音量提示
    int volumePercent = static_cast<int>(percent * 100);
    emit SigShowToast(QString("声音：%1%").arg(volumePercent));

}


void CtrlBar::OnVideoTotalSeconds(int seconds)
{
    _total_play_seconds = seconds;

    int thh, tmm, tss;
    thh = seconds / 3600;
    tmm = (seconds % 3600) / 60;
    tss = seconds % 60;
    QTime totalTime(thh, tmm, tss);
    ui->VideoTotalTimeTimeEdit->setTime(totalTime);
}

void CtrlBar::OnVideoPlaySeconds(int seconds)
{
    // 优化，如果当前 seconds 和上一次 seconds 一样，那么就不用更新ui了
    if(_last_play_seconds == seconds) {
        return;
    }
    _last_play_seconds = seconds;

    int thh, tmm, tss;
    thh = seconds / 3600;
    tmm = (seconds % 3600) / 60;
    tss = seconds % 60;
    QTime totalTime(thh, tmm, tss);
    ui->VideoPlayTimeTimeEdit->setTime(totalTime);

    if(seconds >= 0) {
        ui->PlaySlider->setValue(seconds * 1.0 / _total_play_seconds * ConfigUtils::MAX_SLIDER_VALUE);
    }
}


void CtrlBar::OnPlaySliderValueChanged()
{
    double percent = ui->PlaySlider->value() * 1.0 / ui->PlaySlider->maximum();
    emit SigPlaySeek(percent);

    // 计算跳转的时间
    int seconds = static_cast<int>(percent * _total_play_seconds);
    int thh = seconds / 3600;
    int tmm = (seconds % 3600) / 60;
    int tss = seconds % 60;
    QString timeStr = QString("%1:%2:%3")
                          .arg(thh, 2, 10, QChar('0'))
                          .arg(tmm, 2, 10, QChar('0'))
                          .arg(tss, 2, 10, QChar('0'));

    emit SigShowToast(QString("跳转到 %1").arg(timeStr));
}

void CtrlBar::SlotOnVolumeBtnClicked()
{
    // 静音和恢复静音并不会改变 _last_volume_percent 的值，因为 _last_volume_percent 只会被滑动条滑动改变
    //    如果此时是非静音状态
    if(ui->VolumeBtn->text() == QChar(0xf028)) {
        ui->VolumeSlider->setValue(0);
        GuiUtils::SetIcon(ui->VolumeBtn, 12, QChar(0xf026));
        ui->VolumeBtn->setToolTip("点击恢复音量");
        emit SigPlayVolume(0);
        emit SigShowToast("声音: 0%");
    } else {
        //        恢复之前的音量百分比
        ui->VolumeSlider->setValue(_last_volume_percent * ConfigUtils::MAX_SLIDER_VALUE);
        GuiUtils::SetIcon(ui->VolumeBtn, 12, QChar(0xf028));
        ui->VolumeBtn->setToolTip("点击静音");
        emit SigPlayVolume(_last_volume_percent);

        int volumePercent = static_cast<int>(_last_volume_percent * 100);
        emit SigShowToast(QString("声音: %1%").arg(volumePercent));
    }
}

void CtrlBar::OnVolumeSliderValueChanged()
{
    double percent = ui->VolumeSlider->value() * 1.0 / ui->VolumeSlider->maximum();
    emit SigPlayVolume(percent);

    OnVideopVolume(percent);

    // 显示音量提示
    int volumePercent = static_cast<int>(percent * 100);
    emit SigShowToast(QString("声音：%1%").arg(volumePercent));
}

void CtrlBar::on_SpeedBtn_clicked()
{
    // 弹出倍速下拉菜单，显示在按钮上方
    QPoint pos = ui->SpeedBtn->mapToGlobal(QPoint(0, 0));
    pos.setY(pos.y() - _speed_menu->sizeHint().height());
    _speed_menu->exec(pos);
}

void CtrlBar::OnSpeedMenuTriggered(QAction* action)
{
    float speed = action->data().toFloat();
    emit SigSpeedChanged(speed);
    emit SigShowToast(QString("倍速：%1x").arg(speed, 0, 'f', speed == int(speed) ? 1 : 2));
}

void CtrlBar::on_PlayOrPauseBtn_clicked()
{
    emit SigPlayOrPause();
}

void CtrlBar::on_OverPlayBtn_clicked()
{
    emit SigStop();
}

// 快捷键：快进
void CtrlBar::OnSeekForward(int targetSeconds)
{
    int hours = targetSeconds / 3600;
    int minutes = (targetSeconds % 3600) / 60;
    int seconds = targetSeconds % 60;
    emit SigShowToast(QString("快进: %1:%2:%3")
                          .arg(hours, 2, 10, QChar('0'))
                          .arg(minutes, 2, 10, QChar('0'))
                          .arg(seconds, 2, 10, QChar('0')));
}

// 快捷键：快退
void CtrlBar::OnSeekBack(int targetSeconds)
{
    int hours = targetSeconds / 3600;
    int minutes = (targetSeconds % 3600) / 60;
    int seconds = targetSeconds % 60;
    emit SigShowToast(QString("快退: %1:%2:%3")
                          .arg(hours, 2, 10, QChar('0'))
                          .arg(minutes, 2, 10, QChar('0'))
                          .arg(seconds, 2, 10, QChar('0')));
}

// 快捷键：音量变化
void CtrlBar::OnVolumeChanged(double percent)
{
    // 更新 UI
    OnVideopVolume(percent);

    // 显示音量提示
    int volumePercent = static_cast<int>(percent * 100);
    emit SigShowToast(QString("声音：%1%").arg(volumePercent));
}
