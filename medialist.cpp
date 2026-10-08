#include "medialist.h"
#include<QFileDialog>
#include<QSettings>
#include<QFileInfo>

MediaList::MediaList(QWidget *parent)
    : QListWidget{parent},
    _menu(this),
    _act_add_file(this),
    _act_remove_file(this),
    _act_clear_list(this)
{}


MediaList::~MediaList(){

}


bool MediaList::Init(){
    if(initUi() == false){
        return false;
    }

    //    将行为添加到菜单上
    _menu.addAction(&_act_add_file);
    _menu.addAction(&_act_remove_file);
    _menu.addAction(&_act_clear_list);
    //    添加信号槽
    connect(&_act_add_file, &QAction::triggered, this, &MediaList::addFile);
    connect(&_act_remove_file, &QAction::triggered, this, &MediaList::removeFile);
    connect(&_act_clear_list, &QAction::triggered, this, &QListWidget::clear);
    return true;

}


void MediaList::contextMenuEvent(QContextMenuEvent *event){

    _menu.exec(event->globalPos());
    event->accept();
}

bool MediaList::initUi()
{
    // 设置文本
    _act_add_file.setText("添加");
    _act_remove_file.setText("移除所选项");
    _act_clear_list.setText("清空列表");
    return true;
}

void MediaList::addFile()
{

    // 将 settings.ini 文件存储在应用程序可执行文件所在的目录
    QString configFilePath = QCoreApplication::applicationDirPath() + "/settings.ini";
    QSettings settings(configFilePath, QSettings::IniFormat);

    QString lastPath = settings.value("lastPath", QDir::homePath()).toString(); // 读取路径，如果为空则使用 homePath

    QStringList filePathList = QFileDialog::getOpenFileNames(this, "打开文件", lastPath, "视频文件(*.mkv *.rmvb *.mp4 *.avi *.flv *.wmv *.3gp)");

    if (!filePathList.isEmpty()) {
        // 如果用户选择了文件，则保存当前目录
        QFileInfo fileInfo(filePathList.first());
        settings.setValue("lastPath", fileInfo.absolutePath());

        for(const QString &filePath : filePathList) {
            emit SigAddFile(filePath);
        }
    }
}

void MediaList::removeFile()
{
    takeItem(currentRow());
}
