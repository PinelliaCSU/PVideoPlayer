#include "title.h"
#include "ui_title.h"
#include <QFileInfo>
#include <QMessageBox>
#include "guiutils.h"
#include "medialocator.h"

Title::Title(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Title)
{
    ui->setupUi(this);
}

Title::~Title()
{
    delete ui;
}

void Title::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
}

void Title::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
}

bool Title::Init()
{
    if(initUi() == false) {
        return false;
    }

    connectSignalSlots();

    return true;
}

void Title::SlotOnPlay(QString filePath)
{
    bool isNetworkStream = MediaLocator::isNetworkStream(filePath);

    qDebug() << "Title::SlotOnPlay";
    QFileInfo fileInfo(filePath);
    if(fileInfo.isFile() || isNetworkStream) {
        ui->MovieNameLab->setText(fileInfo.fileName());
        //        todo：加一个异常情况判断，即文件被删除了
    } else {
        QMessageBox::warning(this, tr("文件不存在"),
                             tr("文件 \"%1\" 不存在，请检查路径是否正确。").arg(fileInfo.fileName()));
        ui->MovieNameLab->setText("文件不存在");
    }

}

void Title::SlotOnStop()
{
    ui->MovieNameLab->clear();
}

bool Title::initUi()
{
    //    鼠标悬浮在按钮上显示的文本
    ui->MenuBtn->setToolTip("显示主菜单");
    ui->MinBtn->setToolTip("最小化");
    ui->MaxBtn->setToolTip("最大化");
    ui->FullScreenBtn->setToolTip("全屏");
    ui->CloseBtn->setToolTip("关闭");
    //    清空媒体视频名称
    ui->MovieNameLab->clear();
    //    保证窗口不被绘制上的部分透明
        setAttribute(Qt::WA_TranslucentBackground);
    //    设置样式表
    setStyleSheet(GuiUtils::LoadQss(":/res/qss/title.css"));
    //    设置按钮图标
    GuiUtils::SetIcon(ui->MinBtn, 9, QChar(0xf2d1));
    GuiUtils::SetIcon(ui->MaxBtn, 9, QChar(0xf2d0));
    GuiUtils::SetIcon(ui->FullScreenBtn, 9, QChar(0xf065));
    GuiUtils::SetIcon(ui->CloseBtn, 9, QChar(0xf00d));

    return true;
}

void Title::connectSignalSlots()
{
    connect(ui->MinBtn, &QPushButton::clicked, this, &Title::SigMinBtnClicked);
    connect(ui->MaxBtn, &QPushButton::clicked, this, &Title::SigMaxBtnClicked);
    connect(ui->FullScreenBtn, &QPushButton::clicked, this, &Title::SigFullScreenBtnClicked);
    connect(ui->CloseBtn, &QPushButton::clicked, this, &Title::SigCloseBtnClicked);
    connect(ui->MenuBtn, &QPushButton::clicked, this, &Title::SigMenuBtnClicked);
}
