QT += core gui widgets testlib
CONFIG += console c++17 testcase
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    testmain.cpp \
    tst_medialocator.cpp \
    tst_playbackservice.cpp \
    tst_subtitleplugin.cpp \
    ../medialocator.cpp \
    ../playbackservice.cpp \
    ../playbacksessionmanager.cpp \
    ../subtitleparser.cpp \
    ../subtitleplugin.cpp

HEADERS += \
    tst_medialocator.h \
    tst_playbackservice.h \
    tst_subtitleplugin.h \
    fakeplaybackbackend.h \
    ../medialocator.h \
    ../playbacksessionmanager.h \
    ../media_types.h \
    ../playbackbackend.h \
    ../playbackservice.h \
    ../subtitleparser.h \
    ../subtitleplugin.h
