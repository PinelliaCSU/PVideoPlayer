#include "mainwindow.h"
#include "appcontroller.h"
#include "playbacksession.h"
#include "playlistrepository.h"

#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFontDatabase::addApplicationFont(":/res/fontawesome-webfont.ttf");

    // 组合根：创建独立播放会话、播放列表仓储和应用控制器
    PlaybackSession *session = PlaybackSession::createDefault(&a);
    if (!session) {
        return -1;
    }

    SettingsPlaylistRepository playlistRepository;
    AppController controller(session, &playlistRepository);
    if (!controller.init()) {
        return -1;
    }

    MainWindow w(&controller);
    if(w.Init() == false) {
        return -1;
    }
    w.show();

    return a.exec();
}
