#include "show.h"
#include "ui_show.h"
#include <QDebug>
#include <QtMath>
#include <QMutex>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include "guiutils.h"
#include "videoctrl.h"


extern QMutex g_show_rect_mutex;

Show::Show(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Show)
{
    ui->setupUi(this);
    //    接受拖拽事件，可以直接把视频文件拖动到播放界面开始播放
    setAcceptDrops(true);

    this->setAttribute(Qt::WA_OpaquePaintEvent);
    //    防止 Qt 自动刷新 QLabel
    ui->label->setUpdatesEnabled(false);

    _nLastFrameWidth = 0;
    _nLastFrameHeight = 0;

    // 初始化提示标签（使用顶层窗口实现透明效果）
    _toastLabel = new QLabel(this);
    _toastLabel->setVisible(false);

    // 初始化定时器
    _toastTimer = new QTimer(this);
    _toastTimer->setSingleShot(true);

    // 初始化快捷键提示标签（居中显示）
    _shortcutHintLabel = new QLabel(this);
    _shortcutHintLabel->setVisible(false);

    // 初始化快捷键提示定时器
    _shortcutHintTimer = new QTimer(this);
    _shortcutHintTimer->setSingleShot(true);
}

Show::~Show()
{
    delete ui;
}


bool Show::Init(){

    if(initUi() == false) {
        return false;
    }
    if(connectionSignalSlots() == false) {
        return false;
    }

    return true;
}

void Show::OnPlay(QString strFile){
    // 防御性编程
    if(strFile.isEmpty()) {
        qDebug() << "Show::OnPlay: strFile is empty";
        return;
    }
    //    todo：在这里或者在其他地方必须校验必须是可播放的视频文件，而且当前文件得存在，不能突然被删除了。防止打开失败导致程序崩溃
    VideoCtrl::GetInstance()->start_play(strFile, ui->label->winId());
}


void Show::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if(urls.isEmpty()) {
        return;
    }

    for(const QUrl& url: urls) {
        QString strFileName = url.toLocalFile();
        emit SigOpenFile(strFileName);
        //        如果拖拽了多个，我们只播放第一个文件，不过这个合理吗？
        break;
    }
}

void Show::dragEnterEvent(QDragEnterEvent *event)
{
    event->acceptProposedAction();
}

void Show::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    ChangeShow();
}

bool Show::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->label && event->type() == QEvent::MouseButtonPress) {
        qDebug() << "Show::eventFilter - label clicked, toggle play/pause";
        emit SigTogglePlay();
        return true; // 事件已处理
    }
    return QWidget::eventFilter(obj, event);
}

bool Show::initUi(){
    //    加载qss
    setStyleSheet(GuiUtils::LoadQss(":/res/qss/show.css"));

    ui->label->clear();

    // 设置样式 - 只设置文字颜色
    _toastLabel->setStyleSheet(
        "QLabel {"
        "    color: #4169E1;"
        "    border: none;"
        "}"
        );

    // 设置为顶层窗口，避免继承父控件的样式表和背景
    _toastLabel->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    _toastLabel->setAttribute(Qt::WA_TranslucentBackground, true);


    // 设置字体
    QFont font = _toastLabel->font();
    font.setPointSize(14);
    _toastLabel->setFont(font);

    // 设置快捷键提示标签样式（居中显示）
    _shortcutHintLabel->setStyleSheet(
        "QLabel {"
        "    color: #1cdb20;"
        "    border: none;"
        "}"
        );

    // 设置为顶层窗口，避免继承父控件的样式表和背景
    _shortcutHintLabel->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    _shortcutHintLabel->setAttribute(Qt::WA_TranslucentBackground, true);

    // 设置字体
    _shortcutHintLabel->setFont(font);
    ui->label->installEventFilter(this);
    return true;
}

bool Show::connectionSignalSlots()
{
    bool bRet = true;

    bRet = connect(this, &Show::SigPlay, this, &Show::OnPlay);
    connect(_toastTimer, &QTimer::timeout, this, &Show::OnToastTimeout);
    connect(_shortcutHintTimer, &QTimer::timeout, this, &Show::OnShortcutHintTimeout);

    return bRet;
}

QString Show::getCurrentFile(){
    return _current_file;
}


void Show::OnStartPlay(QString filename) {
    _current_file = filename;
}
void Show::OnFrameDimensionsChanged(int nFrameWidth, int nFrameHeight)
{
    qDebug() << "Show::OnFrameDimensionsChanged" << nFrameWidth << nFrameHeight;

    // 缓存机制，必须真的改变
    if (_nLastFrameWidth == nFrameWidth && _nLastFrameHeight == nFrameHeight) {
        return;
    }

    _nLastFrameWidth = nFrameWidth;
    _nLastFrameHeight = nFrameHeight;

    ChangeShow();
}

void Show::ChangeShow(){
    g_show_rect_mutex.lock();
    qDebug() << "Show::ChangeShow - size:" << width() << "x" << height() << "frame:" << _nLastFrameWidth << "x" << _nLastFrameHeight;
    // 让 label 铺满整个 Show widget，SDL 内部自行处理等比缩放和居中
    ui->label->setGeometry(0, 0, width(), height());
    g_show_rect_mutex.unlock();
}

void Show::ShowToast(const QString &text)
{
    if (_toastLabel == nullptr) {
        return;
    }

    // 设置文本
    _toastLabel->setText(text);

    // 根据文本长度调整大小，添加一些边距
    QFontMetrics fm(_toastLabel->font());
    _toastLabel->setFixedSize(fm.horizontalAdvance(text) + 30, fm.height() + 16);  // 左右各15px，上下各8px的边距

    // 对于顶层窗口，需要计算全局位置并调用 show()
    // 将局部坐标转换为全局坐标（左上角位置）
    QPoint globalPos = this->mapToGlobal(QPoint(10, 10));
    _toastLabel->move(globalPos);

    // 显示标签并确保在最上层，但不获取焦点（避免影响键盘事件）
    _toastLabel->show();
    _toastLabel->raise();
    // 注意：不调用 activateWindow()，避免焦点转移

    // 启动定时器，2秒后隐藏
    _toastTimer->start(2000);
}

void Show::OnToastTimeout()
{
    if (_toastLabel) {
        _toastLabel->hide();  // 顶层窗口使用 hide()
    }
}

void Show::ShowShortcutHint(const QString &text)
{
    if (_shortcutHintLabel == nullptr) {
        return;
    }

    // 设置文本
    _shortcutHintLabel->setText(text);

    // 根据文本长度调整大小，添加一些边距
    QFontMetrics fm(_shortcutHintLabel->font());
    _shortcutHintLabel->setFixedSize(fm.horizontalAdvance(text) + 30, fm.height() + 16);  // 左右各15px，上下各8px的边距

    // 将文本显示在窗口正上方居中位置（距离顶部30px）
    int x = (this->width() - _shortcutHintLabel->width()) / 2;
    int y = 30; // 距离顶部30px
    QPoint globalPos = this->mapToGlobal(QPoint(x, y));
    _shortcutHintLabel->move(globalPos);

    // 显示标签并确保在最上层，但不获取焦点（避免影响键盘事件）
    _shortcutHintLabel->show();
    _shortcutHintLabel->raise();
    // 注意：不调用 activateWindow()，避免焦点转移

    // 启动定时器，2秒后隐藏
    _shortcutHintTimer->start(2000);
}


void Show::HideShortcutHint()
{
    if (_shortcutHintLabel) {
        _shortcutHintLabel->hide();
        _shortcutHintTimer->stop(); // 停止定时器
    }
}

void Show::OnShortcutHintTimeout()
{
    if (_shortcutHintLabel) {
        _shortcutHintLabel->hide();
    }
}

void Show::keyReleaseEvent(QKeyEvent *event)
{

    int key = event->key();

    // 使用全局工具函数获取按键名称
    QString keyName = GuiUtils::GetKeyName(key);

    qDebug() << "Show::keyReleaseEvent:" << keyName;

    // 发送相应的信号（上下左右、空格、S已通过MainWindow全局QAction处理）
    switch (key) {
    case Qt::Key_Escape:
        emit SigExitFullScreen(); // ESC退出全屏
        break;
    default:
        break;
    }

    QWidget::keyReleaseEvent(event);
}
