#include "mainwindow.h"
#include "appcontroller.h"
#include "playbackbackend.h"
#include "playlistrepository.h"

#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFontDatabase::addApplicationFont(":/res/fontawesome-webfont.ttf");

    // 组合根：在此创建播放后端、播放列表仓储和应用控制器，UI 层不再进行静态单例查找
    const PlaybackBackendBundle backend = CreateDefaultPlaybackBackend();
    if (backend.events == nullptr || backend.backend == nullptr) {
        return -1;
    }

    SettingsPlaylistRepository playlistRepository;
    AppController controller(backend.events, backend.backend, &playlistRepository);
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
