QT += core gui widgets testlib
CONFIG += console c++17 testcase
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    testmain.cpp \
    tst_appcontroller.cpp \
    tst_medialocator.cpp \
    tst_playbackservice.cpp \
    tst_playbacksettings.cpp \
    tst_playlist.cpp \
    tst_subtitleplugin.cpp \
    ../appcontroller.cpp \
    ../configutils.cpp \
    ../medialocator.cpp \
    ../playbackcoordinator.cpp \
    ../playbackservice.cpp \
    ../playbacksessionmanager.cpp \
    ../playbacksettings.cpp \
    ../playlistcoordinator.cpp \
    ../playlistmodel.cpp \
    ../playlistrepository.cpp \
    ../settingsrepository.cpp \
    ../subtitleparser.cpp \
    ../subtitleplugin.cpp

HEADERS += \
    fakeplaybackbackend.h \
    tst_appcontroller.h \
    tst_medialocator.h \
    tst_playbackservice.h \
    tst_playbacksettings.h \
    tst_playlist.h \
    tst_subtitleplugin.h \
    ../appcontroller.h \
    ../configutils.h \
    ../media_types.h \
    ../medialocator.h \
    ../playbackbackend.h \
    ../playbackcoordinator.h \
    ../playbackservice.h \
    ../playbacksessionmanager.h \
    ../playbacksettings.h \
    ../playlistcoordinator.h \
    ../playlistmodel.h \
    ../playlistrepository.h \
    ../settingsrepository.h \
    ../subtitleparser.h \
    ../subtitleplugin.h
