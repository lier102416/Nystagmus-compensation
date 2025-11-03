/********************************************************************************
** Form generated from reading UI file 'videoplayer.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VIDEOPLAYER_H
#define UI_VIDEOPLAYER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_videoPlayer
{
public:

    void setupUi(QWidget *videoPlayer)
    {
        if (videoPlayer->objectName().isEmpty())
            videoPlayer->setObjectName("videoPlayer");
        videoPlayer->resize(2200, 1400);

        retranslateUi(videoPlayer);

        QMetaObject::connectSlotsByName(videoPlayer);
    } // setupUi

    void retranslateUi(QWidget *videoPlayer)
    {
        videoPlayer->setWindowTitle(QCoreApplication::translate("videoPlayer", "Form", nullptr));
    } // retranslateUi

};

namespace Ui {
    class videoPlayer: public Ui_videoPlayer {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VIDEOPLAYER_H
