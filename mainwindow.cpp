#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QIcon>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QShortcut>

#include "appcontroller.h"
#include "guiutils.h"

MainWindow::MainWindow(AppController *controller, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , _controller(controller)
    , _playlist(this)
    , _title(this)
    , _menu(this)
    , _move_drag(false)
{
    Q_ASSERT(_controller != nullptr);

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

    // 视图通过接口访问播放能力，不再直接依赖 PlaybackService
    ui->show->SetPlaybackTarget(_controller);
    // 播放列表视图绑定应用控制器持有的数据模型
    _playlist.SetPlaylistModel(_controller->playlistModel());
    /*
     * 渲染目标延迟提供：只有真正开始播放时才向 Show 查询原生窗口句柄。
     * 启动阶段提前调用 winId() 会把视频容器变成原生窗口，此时还没有任何渲染，
     * 窗口内容未绘制会直接透出桌面背景（视频区域显示为空白）。
     */
    _controller->setRenderTargetProvider([this]() { return ui->show->renderTarget(); });

    initMenu();
    connectUiSignals();
    connectControllerSignals();

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

void MainWindow::connectUiSignals(){
    //标题栏的按钮功能
    connect(&_title, &Title::SigMinBtnClicked, this, &MainWindow::SlotOnMinBtnClicked);
    connect(&_title, &Title::SigMaxBtnClicked, this, &MainWindow::SlotOnMaxBtnClicked);
    connect(&_title, &Title::SigFullScreenBtnClicked, this, &MainWindow::SlotOnFullScreenBtnClicked);
    connect(&_title, &Title::SigCloseBtnClicked, this, &MainWindow::SlotOnCloseBtnClicked);
    connect(&_title, &Title::SigMenuBtnClicked, this, &MainWindow::SlotOnMenuBtnClicked);

    // 播放列表视图的用户意图统一交给应用控制器处理
    connect(&_playlist, &Playlist::SigAddRequested, _controller, &AppController::addLocator);
    connect(&_playlist, &Playlist::SigPlayIndexRequested, _controller, &AppController::playIndex);
    connect(&_playlist, &Playlist::SigRemoveRequested, _controller, &AppController::removeAt);
    connect(&_playlist, &Playlist::SigClearRequested, _controller, &AppController::clearPlaylist);

    // 显示窗口：拖入文件播放、点击画面切换暂停、ESC 退出全屏
    connect(ui->show, &Show::SigOpenFile, _controller, &AppController::addLocatorAndPlay);
    connect(ui->show, &Show::SigExitFullScreen, this, &MainWindow::SlotOnFullScreenBtnClicked);
    connect(ui->show, &Show::SigTogglePlay, _controller, &AppController::togglePause);
    connect(ui->show, &Show::SigUserInteraction, _controller, &AppController::notifyUserInteraction);
    connect(ui->ctrlBar, &CtrlBar::SigUserInteraction, _controller, &AppController::notifyUserInteraction);

    //    状态控制栏的按钮功能
    connect(ui->ctrlBar, &CtrlBar::SigPlayListCtlBtnClicked, this, &MainWindow::SlotOnPlayListCtrlBtnClicked);
    connect(ui->ctrlBar, &CtrlBar::SigBackBtnClicked, _controller, &AppController::playPrevious);
    connect(ui->ctrlBar, &CtrlBar::SigNextBtnClicked, _controller, &AppController::playNext);
    connect(ui->ctrlBar, &CtrlBar::SigSpeedChanged, _controller, &AppController::setSpeed);
    connect(ui->ctrlBar, &CtrlBar::SigPlayOrPause, _controller, &AppController::togglePause);
    connect(ui->ctrlBar, &CtrlBar::SigStop, _controller, &AppController::stop);
    connect(ui->ctrlBar, &CtrlBar::SigPlayVolume, _controller, &AppController::setVolume);
    connect(ui->ctrlBar, &CtrlBar::SigPlaySeek, _controller, &AppController::seek);
    connect(ui->ctrlBar, &CtrlBar::SigShowToast, ui->show, &Show::ShowToast);
    connect(ui->ctrlBar, &CtrlBar::SigExtractAudio, this, &MainWindow::SlotOnExtractAudio);

    // 设置按钮相关
    connect(ui->ctrlBar, &CtrlBar::SigPlayModeChanged, _controller, &AppController::setPlayMode);
    connect(ui->ctrlBar, &CtrlBar::SigAlwaysOnTopToggled, this, &MainWindow::SlotOnAlwaysOnTopToggled);
}

void MainWindow::connectControllerSignals(){
    /*
     * 控制器的信号全部由 PlaybackService 的统一播放状态派生，
     * 界面只负责把状态显示出来，不再自己维护播放业务状态。
     */
    connect(_controller, &AppController::started, &_title, &Title::SlotOnPlay);
    connect(_controller, &AppController::started, ui->show, &Show::OnStartPlay);
    connect(_controller, &AppController::started, this, &MainWindow::ShowControlBar);
    connect(_controller, &AppController::speedChanged, ui->ctrlBar, &CtrlBar::OnSpeed);
    connect(_controller, &AppController::pauseChanged, ui->ctrlBar, &CtrlBar::OnPauseStat);
    connect(_controller, &AppController::pauseChanged, this, &MainWindow::ShowControlBar);
    connect(_controller, &AppController::finished, ui->ctrlBar, &CtrlBar::OnStopFinished);
    connect(_controller, &AppController::finished, this, &MainWindow::ShowControlBar);
    connect(_controller, &AppController::userStopped, ui->ctrlBar, &CtrlBar::OnUserStopFinished);
    connect(_controller, &AppController::userStopped, this, &MainWindow::ShowControlBar);
    connect(_controller, &AppController::userStopped, &_title, &Title::SlotOnStop);

    connect(_controller, &AppController::totalSecondsChanged, ui->ctrlBar, &CtrlBar::OnVideoTotalSeconds);
    connect(_controller, &AppController::positionSecondsChanged, ui->ctrlBar, &CtrlBar::OnVideoPlaySeconds);
    connect(_controller, &AppController::volumeChanged, ui->ctrlBar, &CtrlBar::OnVolumeChanged);

    // 快捷键操作后的 Toast 提示
    connect(_controller, &AppController::seekForwardCompleted, ui->ctrlBar, &CtrlBar::OnSeekForward);
    connect(_controller, &AppController::seekBackCompleted, ui->ctrlBar, &CtrlBar::OnSeekBack);

    connect(_controller, &AppController::frameDimensionsChanged, ui->show, &Show::OnFrameDimensionsChanged);
    connect(_controller, &AppController::errorOccurred, ui->show, &Show::ShowToast);
    connect(_controller, &AppController::notificationRequested, this, [this](const QString &message) {
        QMessageBox::warning(this, tr("提示"), message);
    });

    // 控制栏显示策略由控制器决定
    connect(_controller, &AppController::showControlBarRequested, this, &MainWindow::ShowControlBar);
    connect(_controller, &AppController::hideControlBarRequested, ui->ctrlBar, &QWidget::hide);

    connect(_controller, &AppController::currentIndexChanged, &_playlist, &Playlist::SetCurrentIndex);
    connect(_controller, &AppController::resumeAvailable, this, &MainWindow::OnResumeAvailable);

    // 音频提取
    connect(_controller, &AppController::audioExtractionStarted, this, [this]() {
        ui->show->ShowToast(tr("正在提取音频..."));
    });
    connect(_controller, &AppController::audioExtractionFinished,
            this, [this](bool, const QString &message) {
                ui->show->ShowToast(message);
            });
}

void MainWindow::ShowControlBar()
{
    ui->ctrlBar->show();
}

void MainWindow::OnResumeAvailable(const QString &, int positionSeconds, int totalSeconds)
{
    const QString message = tr("检测到上次播放记录：\n进度 %1:%2 / %3:%4\n\n是否从上次位置继续播放？")
                                .arg(positionSeconds / 60, 2, 10, QLatin1Char('0'))
                                .arg(positionSeconds % 60, 2, 10, QLatin1Char('0'))
                                .arg(totalSeconds / 60, 2, 10, QLatin1Char('0'))
                                .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
    if (QMessageBox::question(this, tr("断点续播"), message,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::Yes) == QMessageBox::Yes) {
        _controller->resumePlaybackAt(positionSeconds, totalSeconds);
    }
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
        // 画中画与全屏互斥：先把画面收回主窗口
        if (ui->show->IsPipActive()) {
            ui->show->SetPipActive(false);
        }
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
    QAction* act_pip = _menu.addAction(tr("画中画 \t Ctrl + P"));
    act_pip->setCheckable(true);
    connect(act_pip, &QAction::triggered, this, [this](bool checked) {
        ui->show->SetPipActive(checked);
    });
    // 通过右键菜单或其它入口切换时，保持菜单勾选状态同步
    connect(ui->show, &Show::SigPipActiveChanged, act_pip, &QAction::setChecked);

    // ========== 最近播放子菜单 ==========
    QMenu* recent_menu = _menu.addMenu(tr("最近播放"));
    const QList<PlayHistoryEntry> history = _controller->history();
    if (history.isEmpty()) {
        QAction* empty = recent_menu->addAction(tr("(暂无记录)"));
        empty->setEnabled(false);
    } else {
        for (const PlayHistoryEntry &item : history) {
            // 提取简短显示名
            QString displayName = item.locator;
            const int lastSlash = item.locator.lastIndexOf('/');
            if (lastSlash >= 0) {
                displayName = item.locator.mid(lastSlash + 1);
            }
            if (displayName.isEmpty()) {
                displayName = item.locator;
            }

            const QString label = QString("%1  (%2:%3)").arg(displayName)
                                      .arg(item.positionSeconds / 60, 2, 10, QLatin1Char('0'))
                                      .arg(item.positionSeconds % 60, 2, 10, QLatin1Char('0'));

            QAction* act = recent_menu->addAction(label);
            const QString locator = item.locator;
            const int savedPos = item.positionSeconds;
            connect(act, &QAction::triggered, this, [this, locator, savedPos]() {
                _controller->playFromHistory(locator, savedPos);
            });
        }
    }

    recent_menu->addSeparator();
    QAction *act_clear_history = recent_menu->addAction(tr("清除播放记录"));
    connect(act_clear_history, &QAction::triggered, this, [this]() {
        _controller->clearHistory();
        QMessageBox::information(this, tr("已清除"), tr("播放记录已全部清除。"));
    });


    //    添加槽函数
    connect(act_about, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("关于"), tr("PVideo Player\n\n基于 Qt 和 FFmpeg 的多媒体播放器。"));
    });

    connect(act_open_file, &QAction::triggered, this, [this]() {
        QString strFileName = QFileDialog::getOpenFileName(this, tr("打开文件"), QDir::homePath(),
                                                           tr("媒体文件(*.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp *.mov *.mp3 *.wav *.flac);;所有文件(*.*)"));
        if (!strFileName.isEmpty()) {
            _controller->addLocatorAndPlay(strFileName);
        }
    });

    connect(act_open_stream, &QAction::triggered, this, [this]() {
        bool ok;
        QString text = QInputDialog::getText(this, tr("打开视频流"),
                                             tr("请输入网络流地址 (如 rtmp://...):"), QLineEdit::Normal,
                                             "", &ok);
        if (ok && !text.isEmpty()){
            _controller->addLocatorAndPlay(text);
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
    act_pip->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_P));

    // 将行为设置为应用全局有效（F11需要全局生效，因为全屏时焦点在Show窗口上）
    act_full_screen->setShortcutContext(Qt::ApplicationShortcut);
    // 画中画时焦点可能在悬浮小窗上，快捷键同样需要全局生效
    act_pip->setShortcutContext(Qt::ApplicationShortcut);

    registerShortcuts();

    // 把action添加到当前窗口上
    this->addAction(act_about);
    this->addAction(act_open_file);
    this->addAction(act_open_stream);
    this->addAction(act_full_screen);
    this->addAction(act_pip);
}

void MainWindow::registerShortcuts()
{
    // 方向键、音量键等设置为全局 action（全屏时焦点在 Show 窗口上，也需要生效）
    const struct {
        Qt::Key key;
        void (AppController::*command)();
    } shortcuts[] = {
        {Qt::Key_Left,  &AppController::seekBack},
        {Qt::Key_Right, &AppController::seekForward},
        {Qt::Key_Up,    &AppController::addVolume},
        {Qt::Key_Down,  &AppController::subVolume},
        {Qt::Key_Space, &AppController::togglePause},
        {Qt::Key_S,     &AppController::step},
    };

    for (const auto &shortcut : shortcuts) {
        QAction *action = new QAction(this);
        action->setShortcut(shortcut.key);
        action->setShortcutContext(Qt::ApplicationShortcut);
        connect(action, &QAction::triggered, _controller, shortcut.command);
        this->addAction(action);
    }
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
        ui->show->ShowToast(tr("没有正在播放的视频"));
        return;
    }

    // 弹出保存文件对话框
    QString outputFile = QFileDialog::getSaveFileName(
        this,
        tr("提取音频保存为"),
        QFileInfo(inputFile).baseName() + ".mp3",
        tr("WAV 无损 (*.wav);;MP3 (仅源为MP3时可用) (*.mp3);;M4A/AAC (*.m4a);;AAC 裸流 (*.aac);;所有文件 (*)"));

    if (outputFile.isEmpty())
        return;

    // 提取在播放服务线程执行，结果通过 audioExtractionFinished 通知，避免阻塞 UI
    _controller->extractAudio(inputFile, outputFile);
}
