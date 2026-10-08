#include "mainwindow.h"
#include "playbackbackend.h"

#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFontDatabase::addApplicationFont(":/res/fontawesome-webfont.ttf");

    // 组合根：在此创建播放后端并注入窗口，UI 层不再进行静态单例查找
    const PlaybackBackendBundle backend = CreateDefaultPlaybackBackend();
    if (backend.events == nullptr || backend.backend == nullptr) {
        return -1;
    }

    MainWindow w(backend.events, backend.backend);
    if(w.Init() == false) {
        return -1;
    }
    w.show();

    return a.exec();
}
