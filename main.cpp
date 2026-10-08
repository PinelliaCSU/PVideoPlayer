#include "mainwindow.h"

#include <QApplication>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFontDatabase::addApplicationFont(":/res/fontawesome-webfont.ttf");

    MainWindow w;
    if(w.Init() == false) {
        return -1;
    }
    w.show();

    return a.exec();
}
