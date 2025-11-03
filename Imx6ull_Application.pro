QT += core gui multimedia widgets concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport

CONFIG += c++17




# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    widget.cpp

HEADERS += \
    widget.h

FORMS += \
    widget.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target


# 添加库文件路径

#pc

#opencv
INCLUDEPATH += F:\infor\opencv4-5-4\include
LIBS +=F:\infor\opencv4-5-4\x64\mingw\lib\libopencv_*
LIBS += F:\infor\opencv4-5-4\x64\mingw\bin\libopencv_*.dll


win32 {
    message("Window Platform Of FFMpeg")
    INCLUDEPATH += F:/infor/ffmpeg/include
    DEPENDPATH += F:/infor/ffmpeg/include

    LIBS += -LF:/infor/ffmpeg/lib/     			\
                              -lavcodec         \
                              -lavdevice        \
                              -lavformat        \
                              -lavfilter        \
                              -lavutil          \
                              -lswresample      \
                              -lswscale
}

INCLUDEPATH += F:\Program\GraductionCode\QT\Imx6ull_Application-7-24\eigen-3.4.0

#pri文件
INCLUDEPATH += $$PWD/form
INCLUDEPATH += $$PWD/Pip
INCLUDEPATH += $$PWD/qCustomPlot
INCLUDEPATH += $$PWD/ImageProcessing
INCLUDEPATH += $$PWD/predict

include(./form/form.pri)
include(./Pip/Pip.pri)
include(./qCustomPlot/qCustomPlot.pri)
include(./ImageProcessing/ImageProcessing.pri)
include(./predict/predict.pri)

