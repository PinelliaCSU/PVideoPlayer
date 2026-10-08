#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QIcon>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include "guiutils.h"
#include "configutils.h"
#include <QShortcut>
#include "videoctrl.h"
#include "playbackservice.h"
#include "playbacksessionmanager.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
    _playlist(this),
    _title(this),
    _move_drag(false),
    _menu(this),
    _playback_service(nullptr),
    _session_manager(new PlaybackSessionManager(this))
{
    const int sessionId = _session_manager->createSession(VideoCtrl::GetInstance(),
                                                          VideoCtrl::GetInstance());
    _playback_service = _session_manager->session(sessionId);
    ui->setupUi(this);
    setWindowFlags(Qt::FramelessWindowHint);
    this->setWindowIcon(QIcon(":/res/icon.png"));
    setStyleSheet(GuiUtils::LoadQss(":/res/qss/mainwid.css"));
    this->setMouseTracking(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}

bool MainWindow::Init()
{
    QWidget *play_list_bar_wid = new QWidget(this);
    ui->Playlist->setTitleBarWidget(play_list_bar_wid);
    ui->Playlist->setWidget(&_playlist);

    QWidget *title_bar_wid = new QWidget(this);
    ui->Title->setTitleBarWidget(title_bar_wid);
    ui->Title->setWidget(&_title);

    if(_title.Init() == false ||
        _playlist.Init() == false ||
         ui->ctrlBar->Init() == false||
         ui->show->Init() == false ){
        return false;
    }

    ui->show->SetPlaybackService(_playback_service);
    initMenu();

    connectSignalSlots();

    return true;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if(event->button() & Qt::LeftButton) {
        //        只有鼠标在标题栏按下时，才可以拖动窗口
        if(ui->Title->geometry().contains(event->pos())) {
            _move_drag = true;
            _drag_position = event->globalPosition().toPoint() - this->pos();
        }
    }
    //    执行原有的鼠标事件
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    _move_drag = false;
    //    执行原有的鼠标事件
    QWidget::mouseReleaseEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(_move_drag) {
        move(event->globalPosition().toPoint() - _drag_position);
    }
    //    执行原有的鼠标事件
    QWidget::mouseMoveEvent(event);
}


void MainWindow::connectSignalSlots(){
    //标题栏的按钮功能
    connect(&_title, &Title::SigMinBtnClicked, this, &MainWindow::SlotOnMinBtnClicked);
    connect(&_title, &Title::SigMaxBtnClicked, this, &MainWindow::SlotOnMaxBtnClicked);
    connect(&_title, &Title::SigFullScreenBtnClicked, this, &MainWindow::SlotOnFullScreenBtnClicked);
    connect(&_title, &Title::SigCloseBtnClicked, this, &MainWindow::SlotOnCloseBtnClicked);
    connect(&_title, &Title::SigMenuBtnClicked, this, &MainWindow::SlotOnMenuBtnClicked);

    /*
     * 开启视频播放
     * 逻辑是：双击播放列表或者点击播放按钮的时候，调用 Playlist::SigPlay，然后触发 Show::SigPlay，去调用 VideoCtrl::start_play 播放视频
     * 然后 VideoCtrl::start_play 触发时 又会去调用 &Title::SlotOnPlay 修改标签栏的视频文件名称
     *
     */
    connect(&_playlist, &Playlist::SigPlay, ui->show, &Show::SigPlay);

    //    图片显示窗口的事件功能，比如拖拽、快捷键按下等
    connect(ui->show, &Show::SigOpenFile, &_playlist, &Playlist::OnAddFileAndPlay);

    // Show窗口: ESC退出全屏（其他快捷键已通过MainWindow全局QAction处理，ApplicationShortcut全屏时也生效）
    connect(ui->show, &Show::SigExitFullScreen, this, &MainWindow::SlotOnFullScreenBtnClicked);
    // 点击视频画面切换播放/暂停
    connect(ui->show, &Show::SigTogglePlay, _playback_service, &PlaybackService::pause);

    // MainWindow全局QAction快捷键功能（ApplicationShortcut，全屏时也生效）
    connect(this, &MainWindow::SigSeekForward, _playback_service, &PlaybackService::seekForward);
    connect(this, &MainWindow::SigSeekBack, _playback_service, &PlaybackService::seekBack);
    connect(this, &MainWindow::SigAddVolume, _playback_service, &PlaybackService::addVolume);
    connect(this, &MainWindow::SigSubVolume, _playback_service, &PlaybackService::subVolume);
    connect(this, &MainWindow::SigPlayOrPause, _playback_service, &PlaybackService::pause);
    connect(this, &MainWindow::SigStep, _playback_service, &PlaybackService::step);

    //    状态控制栏的按钮功能
    connect(ui->ctrlBar, &CtrlBar::SigPlayListCtlBtnClicked, this, &MainWindow::SlotOnPlayListCtrlBtnClicked);
    connect(ui->ctrlBar, &CtrlBar::SigBackBtnClicked, &_playlist, &Playlist::SlotOnBackPlay);
    connect(ui->ctrlBar, &CtrlBar::SigNextBtnClicked, &_playlist, &Playlist::SlotOnNextPlay);
    connect(ui->ctrlBar, &CtrlBar::SigSpeedChanged, _playback_service, &PlaybackService::setSpeed);
    connect(ui->ctrlBar, &CtrlBar::SigPlayOrPause, _playback_service, &PlaybackService::pause);
    connect(ui->ctrlBar, &CtrlBar::SigStop, _playback_service, &PlaybackService::userStop);

    connect(ui->ctrlBar, &CtrlBar::SigPlayVolume, _playback_service, &PlaybackService::setVolume);
    connect(ui->ctrlBar, &CtrlBar::SigPlaySeek, _playback_service, &PlaybackService::seek);
    connect(ui->ctrlBar, &CtrlBar::SigShowToast, ui->show, &Show::ShowToast);
    // 提取音频
    connect(ui->ctrlBar, &CtrlBar::SigExtractAudio, this, &MainWindow::SlotOnExtractAudio);

    // 设置按钮相关
    connect(ui->ctrlBar, &CtrlBar::SigPlayModeChanged, this, &MainWindow::SlotOnPlayModeChanged);
    connect(ui->ctrlBar, &CtrlBar::SigAlwaysOnTopToggled, this, &MainWindow::SlotOnAlwaysOnTopToggled);

    // 快捷键操作后显示 Toast（通过 VideoCtrl 的状态信号）
    connect(_playback_service, &PlaybackService::seekForwardCompleted, ui->ctrlBar, &CtrlBar::OnSeekForward);
    connect(_playback_service, &PlaybackService::seekBackCompleted, ui->ctrlBar, &CtrlBar::OnSeekBack);


    /*
     * 视频播放时，界面相关变化通知
     * - 有的是视频播放后才知道一点一点通知界面的，比如现在的播放时间；
     * - 有的是快捷键按下直接通知 VideoCtrl的，所以 VideCtl 需要反馈给界面,比如左右按键
     * - 有的是窗口大小发生改变了，我们的 SDL 窗口大小也会改变，所以 VideCtl 需要反馈给界面
     * 使用 DirectConnection 是因为需要 signal 任务结束时马上通知 slot 任务，不能扔在队列里面等待，这样会慢不及时。故直接在发送者线程执行这两个方法，相当于直接回调，即 this.signal, receiver.slot
     * 使用 QueuedConnection 是因为双方不在同一个线程，防止出现数据竞争或者崩溃，故直接丢给将当前方法丢给接受者线程执行。相当于观察者模式，在 receiver 线程执行 sender.signal，执行完成后 notify 当前线程执行 slot 方法
     *
     */
    connect(_playback_service, &PlaybackService::started, this, &MainWindow::SlotOnBeforeNewPlay);
    connect(_playback_service, &PlaybackService::started, &_title, &Title::SlotOnPlay);
    connect(_playback_service, &PlaybackService::started, ui->show, &Show::OnStartPlay);
    connect(_playback_service, &PlaybackService::speedChanged, ui->ctrlBar, &CtrlBar::OnSpeed);
    connect(_playback_service, &PlaybackService::pauseChanged, ui->ctrlBar, &CtrlBar::OnPauseStat);
    connect(_playback_service, &PlaybackService::finished, ui->ctrlBar, &CtrlBar::OnStopFinished);
    // 视频播放完毕后自动播放下一个
    connect(_playback_service, &PlaybackService::finished, &_playlist, &Playlist::SlotOnNextPlay);
    connect(_playback_service, &PlaybackService::finished, this, &MainWindow::SlotOnSavePlaybackPosition);

    connect(_playback_service, &PlaybackService::userStopped, ui->ctrlBar, &CtrlBar::OnUserStopFinished);
    connect(_playback_service, &PlaybackService::userStopped, &_title, &Title::SlotOnStop);
    // 用户主动停止时也保存播放位置
    connect(_playback_service, &PlaybackService::userStopped, this, &MainWindow::SlotOnSavePlaybackPosition);

    connect(_playback_service, &PlaybackService::totalSecondsChanged, ui->ctrlBar, &CtrlBar::OnVideoTotalSeconds);
    connect(_playback_service, &PlaybackService::totalSecondsChanged, this, &MainWindow::SlotOnCacheTotalSeconds);
    connect(_playback_service, &PlaybackService::positionSecondsChanged, ui->ctrlBar, &CtrlBar::OnVideoPlaySeconds);
    connect(_playback_service, &PlaybackService::positionSecondsChanged, this, &MainWindow::SlotOnCachePlaySeconds);
    // 视频开始播放后检查是否需要续播
    connect(_playback_service, &PlaybackService::totalSecondsChanged, this, &MainWindow::SlotOnCheckResume);

    connect(_playback_service, &PlaybackService::volumeChanged, ui->ctrlBar, &CtrlBar::OnVolumeChanged);
    connect(_playback_service, &PlaybackService::frameDimensionsChanged, ui->show, &Show::OnFrameDimensionsChanged);
    connect(_playback_service, &PlaybackService::errorOccurred, ui->show, &Show::ShowToast);

}

void MainWindow::SlotOnMinBtnClicked()
{
    this->showMinimized();
}

void MainWindow::SlotOnMaxBtnClicked()
{
    if(isMaximized()) {
        showNormal();
    } else {
        showMaximized();
    }
}

void MainWindow::SlotOnFullScreenBtnClicked(){
    // 检查主窗口是否处于全屏状态（不再检查 ui->show）
    if(this->isFullScreen()) {
        // ===== 退出全屏 =====
        this->showNormal();
        // 恢复之前隐藏的所有控制组件
        ui->ctrlBar->show();
        ui->Playlist->show();
        ui->Title->show();
        ui->statusBar->show();
        // 隐藏快捷键提示
        ui->show->HideShortcutHint();
        this->activateWindow();
        this->setFocus();
    } else {
        // ===== 进入全屏 =====
        // 隐藏所有控制组件，只保留视频画面
        ui->ctrlBar->hide();
        ui->Playlist->hide();
        ui->Title->hide();
        ui->statusBar->hide();
        // 主窗口直接进入全屏（show 始终是子控件，不改变其窗口标志）
        this->showFullScreen();
        // 显示快捷键提示
        ui->show->ShowShortcutHint(tr("按下 ESC/F11 即可退出全屏"));
        // 将焦点给 show，以便接收 ESC 键盘事件
        ui->show->setFocus();
    }
}

void MainWindow::SlotOnCloseBtnClicked()
{
    this->close();
}

void MainWindow::SlotOnMenuBtnClicked()
{
    //    在鼠标位置打开菜单
    _menu.exec(cursor().pos());
}

void MainWindow::SlotOnPlayListCtrlBtnClicked(){
    if(ui->Playlist->isHidden()) {
        int listWidth = ui->Playlist->width();
        ui->Playlist->show();
        // 窗口如果是常规状态，将其宽度加上列表宽度
        if (!this->isMaximized() && !this->isFullScreen()) {
            this->resize(this->width() + listWidth, this->height());
        }
    } else {
        int listWidth = ui->Playlist->width();
        ui->Playlist->hide();
        // 隐藏时，立刻缩小窗口宽度避免残影
        if (!this->isMaximized() && !this->isFullScreen()) {
            this->resize(this->width() - listWidth, this->height());
        } else {
            // 当最大化或全屏时，强制重绘来消除残影
            ui->show->setAttribute(Qt::WA_OpaquePaintEvent, false);
            ui->show->repaint();
            ui->show->setAttribute(Qt::WA_OpaquePaintEvent, true);
        }
    }

}


void MainWindow::initMenu(){
    //    添加菜单和行为
    QAction *act_about = _menu.addAction(tr("关于 \t Ctrl + A"));
    QMenu* open_menu = _menu.addMenu(tr("打开"));
    QAction* act_open_file = open_menu->addAction(tr("打开文件 \t Ctrl + F"));
    QAction* act_open_stream = open_menu->addAction(tr("打开视频流 \t Ctrl + L"));
    QAction* act_full_screen = _menu.addAction(tr("全屏/取消全屏 \t F11"));

    // ========== 最近播放子菜单 ==========
    QMenu* recent_menu = _menu.addMenu(tr("最近播放"));
    QList<ConfigUtils::PlayHistoryItem> history = ConfigUtils::LoadPlayHistory();
    if (history.isEmpty()) {
        QAction* empty = recent_menu->addAction(tr("(暂无记录)"));
        empty->setEnabled(false);
    } else {
        for (const auto& item : history) {
            // 提取简短显示名
            QString displayName = item.filePath;
            int lastSlash = item.filePath.lastIndexOf('/');
            if (lastSlash >= 0) {
                displayName = item.filePath.mid(lastSlash + 1);
            }
            if (displayName.isEmpty()) displayName = item.filePath;

            int posMin = item.lastPosition / 60;
            int posSec = item.lastPosition % 60;
            QString label = QString("%1  (%2:%3)").arg(displayName)
                                .arg(posMin, 2, 10, QLatin1Char('0'))
                                .arg(posSec, 2, 10, QLatin1Char('0'));

            QAction* act = recent_menu->addAction(label);
            QString filePath = item.filePath;  // 捕获副本
            int savedPos = item.lastPosition;
            connect(act, &QAction::triggered, this, [this, filePath, savedPos]() {
                _playlist.OnAddFileAndPlay(filePath);
                // 直接 seek 到保存位置（稍后在 SlotOnCheckResume 中处理，或延迟 seek）
                QTimer::singleShot(500, this, [this, savedPos]() {
                    if (_cached_total_seconds > 0 && savedPos > 0) {
                        double percent = (double)savedPos / _cached_total_seconds;
                        _playback_service->seek(percent);
                    }
                });
            });
        }
    }

    recent_menu->addSeparator();
    QAction *act_clear_history = recent_menu->addAction(tr("清除播放记录"));
    connect(act_clear_history, &QAction::triggered, this, &MainWindow::SlotOnClearPlayHistory);


    //    添加槽函数
    connect(act_about, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("关于"), tr("PVideo Player\n\n基于 Qt 和 FFmpeg 的多媒体播放器。"));
    });

    connect(act_open_file, &QAction::triggered, this, [this]() {
        QString strFileName = QFileDialog::getOpenFileName(this, tr("打开文件"), QDir::homePath(),
                                                           tr("媒体文件(*.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp *.mov *.mp3 *.wav *.flac);;所有文件(*.*)"));
        if (!strFileName.isEmpty()) {
            _playlist.OnAddFileAndPlay(strFileName);
        }
    });

    connect(act_open_stream, &QAction::triggered, this, [this]() {
        bool ok;
        QString text = QInputDialog::getText(this, tr("打开视频流"),
                                             tr("请输入网络流地址 (如 rtmp://...):"), QLineEdit::Normal,
                                             "", &ok);
        if (ok && !text.isEmpty()){
            _playlist.OnAddFileAndPlay(text);
        }
    });

    connect(act_full_screen, &QAction::triggered, this, [this]() {
        this->SlotOnFullScreenBtnClicked();
    });

    // 添加快捷键(由于QMenu没有焦点，故只能再次添加到当前窗口上)
    act_about->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_A));
    act_open_file->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    act_open_stream->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_L));
    act_full_screen->setShortcut(QKeySequence(Qt::Key_F11));

    // 将行为设置为应用全局有效（F11需要全局生效，因为全屏时焦点在Show窗口上）
    act_full_screen->setShortcutContext(Qt::ApplicationShortcut);

    // 上下左右方向键设置为全局action（全屏时焦点在Show窗口上，也需要生效）
    QAction *act_seek_back = new QAction(this);
    act_seek_back->setShortcut(Qt::Key_Left);
    act_seek_back->setShortcutContext(Qt::ApplicationShortcut);
    connect(act_seek_back, &QAction::triggered, this, [this]() {
        emit SigSeekBack();
    });
    this->addAction(act_seek_back);

    QAction *act_seek_forward = new QAction(this);
    act_seek_forward->setShortcut(Qt::Key_Right);
    act_seek_forward->setShortcutContext(Qt::ApplicationShortcut);
    connect(act_seek_forward, &QAction::triggered, this, [this]() {
        emit SigSeekForward();
    });
    this->addAction(act_seek_forward);

    QAction *act_add_volume = new QAction(this);
    act_add_volume->setShortcut(Qt::Key_Up);
    act_add_volume->setShortcutContext(Qt::ApplicationShortcut);
    connect(act_add_volume, &QAction::triggered, this, [this]() {
        emit SigAddVolume();
    });
    this->addAction(act_add_volume);

    QAction *act_sub_volume = new QAction(this);
    act_sub_volume->setShortcut(Qt::Key_Down);
    act_sub_volume->setShortcutContext(Qt::ApplicationShortcut);
    connect(act_sub_volume, &QAction::triggered, this, [this]() {
        emit SigSubVolume();
    });
    this->addAction(act_sub_volume);

    QAction *act_play_or_pause = new QAction(this);
    act_play_or_pause->setShortcut(Qt::Key_Space);
    act_play_or_pause->setShortcutContext(Qt::ApplicationShortcut);
    connect(act_play_or_pause, &QAction::triggered, this, [this]() {
        emit SigPlayOrPause();
    });
    this->addAction(act_play_or_pause);

    QAction *act_step = new QAction(this);
    act_step->setShortcut(Qt::Key_S);
    act_step->setShortcutContext(Qt::ApplicationShortcut);
    connect(act_step, &QAction::triggered, this, [this]() {
        emit SigStep();
    });
    this->addAction(act_step);

    // 把action添加到当前窗口上
    this->addAction(act_about);
    this->addAction(act_open_file);
    this->addAction(act_open_stream);
    this->addAction(act_full_screen);
}



void MainWindow::SlotOnPlayModeChanged(int mode)
{
    _playlist.SetPlayMode((PlayMode)mode);
}

void MainWindow::SlotOnAlwaysOnTopToggled(bool on)
{
    if (on) {
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
    } else {
        setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
    }
    show(); // 需要重新 show 才能生效
}

void MainWindow::SlotOnExtractAudio()
{
    // 获取当前正在播放的文件
    QString inputFile = ui->show->getCurrentFile();
    if (inputFile.isEmpty()) {
        ui->show->ShowToast("没有正在播放的视频");
        return;
    }

    // 弹出保存文件对话框
    QString outputFile = QFileDialog::getSaveFileName(
        this,
        "提取音频保存为",
        QFileInfo(inputFile).baseName() + ".mp3",
        "WAV 无损 (*.wav);;MP3 (仅源为MP3时可用) (*.mp3);;M4A/AAC (*.m4a);;AAC 裸流 (*.aac);;所有文件 (*)");

    if (outputFile.isEmpty())
        return;

    ui->show->ShowToast("正在提取音频...");
    QApplication::processEvents();

    // 调用 VideoCtrl 执行提取（同步）
    bool ok = _playback_service->extractAudio(inputFile, outputFile);

    if (ok)
        ui->show->ShowToast("音频提取完成！");
    else
        ui->show->ShowToast("音频提取失败");
}


// ==================== 断点续播 ====================

void MainWindow::SlotOnBeforeNewPlay(QString filename)
{
    // 在新视频开始播放前，用旧视频的缓存数据保存播放位置
    if (!_playing_file_path.isEmpty() && _cached_total_seconds > 0)
    {
        if (_cached_play_seconds >= _cached_total_seconds - 3)
        {
            ConfigUtils::ClearPlaybackPos(_playing_file_path);
        }
        else if (_cached_play_seconds > 0)
        {
            ConfigUtils::SavePlaybackPos(_playing_file_path, _cached_play_seconds);
        }
        UpdatePlayHistory(_playing_file_path);
    }

    // 切换到新文件
    _playing_file_path = filename;
    _cached_play_seconds = 0;
    _cached_total_seconds = 0;
    _resume_checked = false;
}


void MainWindow::SlotOnCacheTotalSeconds(int seconds)
{
    _cached_total_seconds = seconds;
    _resume_checked = false;  // 新视频开始，重置续播检查标志
}

void MainWindow::SlotOnCachePlaySeconds(int seconds)
{
    _cached_play_seconds = seconds;
}

void MainWindow::SlotOnSavePlaybackPosition()
{
    QString filePath = _playing_file_path;
    if (filePath.isEmpty() || _cached_total_seconds <= 0)
        return;

    // 如果播放到了末尾（剩余不到 3 秒），清除断点，不保存
    if (_cached_play_seconds >= _cached_total_seconds - 3) {
        ConfigUtils::ClearPlaybackPos(filePath);
    } else if (_cached_play_seconds > 0) {
        ConfigUtils::SavePlaybackPos(filePath, _cached_play_seconds);
    }

    // 更新播放历史
    UpdatePlayHistory(filePath);
}

void MainWindow::SlotOnCheckResume(int totalSeconds)
{
    Q_UNUSED(totalSeconds);
    if (_resume_checked) return;  // 已检查过，不重复弹窗
    _resume_checked = true;

    QString filePath = _playing_file_path;
    if (filePath.isEmpty()) return;

    int savedPos = ConfigUtils::LoadPlaybackPos(filePath);
    if (savedPos <= 5) return;    // 进度不足 5 秒不提示
    if (_cached_total_seconds <= 0) return;
    if (savedPos >= _cached_total_seconds - 5) return;  // 接近结尾不提示

    int savedMin = savedPos / 60;
    int savedSec = savedPos % 60;
    int totalMin = _cached_total_seconds / 60;
    int totalSec = _cached_total_seconds % 60;

    QString msg = QString("检测到上次播放记录：\n"
                          "进度 %1:%2 / %3:%4\n\n"
                          "是否从上次位置继续播放？")
                      .arg(savedMin, 2, 10, QLatin1Char('0'))
                      .arg(savedSec, 2, 10, QLatin1Char('0'))
                      .arg(totalMin, 2, 10, QLatin1Char('0'))
                      .arg(totalSec, 2, 10, QLatin1Char('0'));

    QMessageBox::StandardButton reply =
        QMessageBox::question(this, tr("断点续播"), msg,
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (reply == QMessageBox::Yes) {
        // 用百分比 seek（VideoCtrl::OnPlaySeek 接受 0.0~1.0 的百分比）
        double percent = (double)savedPos / _cached_total_seconds;
        _playback_service->seek(percent);
    }
}

void MainWindow::UpdatePlayHistory(const QString& filePath)
{
    // 加载现有历史
    QList<ConfigUtils::PlayHistoryItem> history = ConfigUtils::LoadPlayHistory();

    // 去掉同一文件旧记录
    history.erase(std::remove_if(history.begin(), history.end(),
                                 [&](const ConfigUtils::PlayHistoryItem& item) {
                                     return item.filePath == filePath;
                                 }),
                  history.end());

    // 插入新记录到头部
    ConfigUtils::PlayHistoryItem item;
    item.filePath      = filePath;
    item.lastPosition  = _cached_play_seconds;
    item.totalDuration = _cached_total_seconds;
    item.lastPlayed    = QDateTime::currentDateTime();
    history.prepend(item);

    // 最多保留 10 条
    while (history.size() > 10)
        history.removeLast();

    ConfigUtils::SavePlayHistory(history);
}

void MainWindow::SlotOnClearPlayHistory()
{
    ConfigUtils::SavePlayHistory({});  // 写入空列表
    QMessageBox::information(this, tr("已清除"), tr("播放记录已全部清除。"));
}
