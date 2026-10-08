QT += core gui widgets testlib
CONFIG += console c++17 testcase
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    testmain.cpp \
    tst_medialocator.cpp \
    tst_playbackservice.cpp \
    ../medialocator.cpp \
    ../playbackservice.cpp \
    ../playbacksessionmanager.cpp

HEADERS += \
    tst_medialocator.h \
    tst_playbackservice.h \
    fakeplaybackbackend.h \
    ../medialocator.h \
    ../playbacksessionmanager.h \
    ../media_types.h \
    ../playbackbackend.h \
    ../playbackservice.h
