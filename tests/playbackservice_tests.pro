QT += core gui widgets testlib
CONFIG += console c++17 testcase
TEMPLATE = app

INCLUDEPATH += ..

SOURCES += \
    ../playbackservice.cpp \
    ../playbacksessionmanager.cpp \
    tst_playbackservice.cpp

HEADERS += \
    ../playbacksessionmanager.h \
    ../media_types.h \
    ../playbackbackend.h \
    ../playbackservice.h \
    fakeplaybackbackend.h
