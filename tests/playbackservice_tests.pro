QT += core gui widgets testlib
CONFIG += console c++17 testcase
TEMPLATE = app

INCLUDEPATH += \
    .. \
    ../src/application \
    ../src/core \
    ../src/media \
    ../src/playback \
    ../src/playlist \
    ../src/subtitle \
    ../src/ui

SOURCES += \
    testmain.cpp \
    tst_appcontroller.cpp \
    tst_mediainfodialog.cpp \
    tst_mediainfoservice.cpp \
    tst_medialocator.cpp \
    tst_playbackservice.cpp \
    tst_playbacksettings.cpp \
    tst_playlist.cpp \
    tst_screenshotutils.cpp \
    tst_subtitleplugin.cpp \
    ../src/application/appcontroller.cpp \
    ../src/core/configutils.cpp \
    ../src/core/screenshotutils.cpp \
    ../src/core/settingsrepository.cpp \
    ../src/media/mediainfoservice.cpp \
    ../src/media/medialocator.cpp \
    ../src/playback/playbackcoordinator.cpp \
    ../src/playback/playbackservice.cpp \
    ../src/playback/playbacksession.cpp \
    ../src/playback/playbacksessionmanager.cpp \
    ../src/playback/playbacksettings.cpp \
    ../src/playlist/playlistcoordinator.cpp \
    ../src/playlist/playlistmodel.cpp \
    ../src/playlist/playlistrepository.cpp \
    ../src/subtitle/subtitleparser.cpp \
    ../src/subtitle/subtitleplugin.cpp \
    ../src/ui/guiutils.cpp \
    ../src/ui/mediainfodialog.cpp

HEADERS += \
    fakeplaybackbackend.h \
    tst_appcontroller.h \
    tst_mediainfodialog.h \
    tst_mediainfoservice.h \
    tst_medialocator.h \
    tst_playbackservice.h \
    tst_playbacksettings.h \
    tst_playlist.h \
    tst_screenshotutils.h \
    tst_subtitleplugin.h \
    ../src/application/appcontroller.h \
    ../src/core/configutils.h \
    ../src/core/media_types.h \
    ../src/core/playbackbackend.h \
    ../src/core/screenshotutils.h \
    ../src/core/settingsrepository.h \
    ../src/media/mediainfoservice.h \
    ../src/media/medialocator.h \
    ../src/playback/playbackcoordinator.h \
    ../src/playback/playbackservice.h \
    ../src/playback/playbacksession.h \
    ../src/playback/playbacksessionmanager.h \
    ../src/playback/playbacksettings.h \
    ../src/playlist/playlistcoordinator.h \
    ../src/playlist/playlistmodel.h \
    ../src/playlist/playlistrepository.h \
    ../src/subtitle/subtitleparser.h \
    ../src/subtitle/subtitleplugin.h \
    ../src/ui/mediainfodialog.h
