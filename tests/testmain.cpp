#include <QApplication>
#include <QtTest>

#include "tst_medialocator.h"
#include "tst_playbackservice.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    int status = 0;
    {
        MediaLocatorTest locatorTests;
        status |= QTest::qExec(&locatorTests, argc, argv);
    }
    {
        PlaybackServiceTest serviceTests;
        status |= QTest::qExec(&serviceTests, argc, argv);
    }
    return status;
}
