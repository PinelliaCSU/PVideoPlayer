QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/application/appcontroller.cpp \
    src/application/main.cpp \
    src/core/clockcontroller.cpp \
    src/core/configutils.cpp \
    src/core/settingsrepository.cpp \
    src/media/audioextractionservice.cpp \
    src/media/mediacomponents.cpp \
    src/media/medialocator.cpp \
    src/media/sonic.cpp \
    src/media/videoctrl.cpp \
    src/playback/playbackcoordinator.cpp \
    src/playback/playbackservice.cpp \
    src/playback/playbacksession.cpp \
    src/playback/playbacksession_default.cpp \
    src/playback/playbacksessionmanager.cpp \
    src/playback/playbacksettings.cpp \
    src/playlist/medialist.cpp \
    src/playlist/playlist.cpp \
    src/playlist/playlistcoordinator.cpp \
    src/playlist/playlistmodel.cpp \
    src/playlist/playlistrepository.cpp \
    src/subtitle/subtitleparser.cpp \
    src/subtitle/subtitleplugin.cpp \
    src/ui/ctrlbar.cpp \
    src/ui/customslider.cpp \
    src/ui/guiutils.cpp \
    src/ui/mainwindow.cpp \
    src/ui/show.cpp \
    src/ui/title.cpp

HEADERS += \
    src/application/appcontroller.h \
    src/core/clockcontroller.h \
    src/core/configutils.h \
    src/core/datactrl.h \
    src/core/media_raii.h \
    src/core/media_types.h \
    src/core/playbackbackend.h \
    src/core/settingsrepository.h \
    src/media/audioextractionservice.h \
    src/media/mediacomponents.h \
    src/media/medialocator.h \
    src/media/sonic.h \
    src/media/videoctrl.h \
    src/playback/playbackcoordinator.h \
    src/playback/playbackservice.h \
    src/playback/playbacksession.h \
    src/playback/playbacksessionmanager.h \
    src/playback/playbacksettings.h \
    src/playlist/medialist.h \
    src/playlist/playlist.h \
    src/playlist/playlistcoordinator.h \
    src/playlist/playlistmodel.h \
    src/playlist/playlistrepository.h \
    src/subtitle/subtitleparser.h \
    src/subtitle/subtitleplugin.h \
    src/ui/ctrlbar.h \
    src/ui/customslider.h \
    src/ui/guiutils.h \
    src/ui/mainwindow.h \
    src/ui/show.h \
    src/ui/title.h

FORMS += \
    src/playlist/playlist.ui \
    src/ui/ctrlbar.ui \
    src/ui/mainwindow.ui \
    src/ui/show.ui \
    src/ui/title.ui

INCLUDEPATH += \
    src/application \
    src/core \
    src/media \
    src/playback \
    src/playlist \
    src/subtitle \
    src/ui

win32 {
    # 根据编译器位数设置架构
    contains(QMAKE_HOST.arch, x86_64) {
        ARCH = x64
    } else {
        ARCH = x86
    }

    message("Target architecture: $$ARCH")

    LIBS += -L$$PWD/lib/SDL2/lib/$$ARCH \
        -L$$PWD/lib/ffmpeg-4.2.1/lib/$$ARCH \
        -L$$PWD/lib/windows-kits/lib/$$ARCH \
        -lSDL2 \
        -lavcodec \
        -lavdevice \
        -lavfilter \
        -lavformat \
        -lavutil \
        -lswresample \
        -lswscale \
        -lOle32

    INCLUDEPATH += lib/SDL2/include \
        lib/ffmpeg-4.2.1/include
}

win32 {
    # 指定要拷贝的DLL文件目录（根据架构）
    DllSourceDir = $${PWD}/dll/$$ARCH
    # 将输入目录中的"/"替换为"\"
    DllSourceDir = $$replace(DllSourceDir, /, \\)

    # Debug模式：exe在debug子目录下
    CONFIG(debug, debug|release) {
        OutputDir = $${OUT_PWD}/debug
    } else {
        # Release模式：exe在release子目录下
        OutputDir = $${OUT_PWD}/release
    }
    # 将输出目录中的"/"替换为"\"
    OutputDir = $$replace(OutputDir, /, \\)

    # 执行copy命令，复制所有DLL文件到exe所在目录
    QMAKE_POST_LINK += copy /Y \"$$DllSourceDir\*.dll\" \"$$OutputDir\"
}



###cmd install lib
#sudo apt-get install ffmpeg
#sudo apt-get install libavformat-dev
#sudo apt-get install libavutil-dev
#sudo apt-get install libavcodec-dev
#sudo apt-get install libswscale-dev
#sudo apt-get install libsdl2-dev
###
unix {
LIBS += \
    -lSDL2 \
    -lavcodec \
    -lavdevice \
    -lavfilter \
    -lavformat \
    -lavutil \
    -lswresample \
    -lswscale
}

mac {

}



RESOURCES += \
    res.qrc
