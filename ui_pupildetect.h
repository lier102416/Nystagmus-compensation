/********************************************************************************
** Form generated from reading UI file 'pupildetect.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PUPILDETECT_H
#define UI_PUPILDETECT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_PupilDetect
{
public:
    QLabel *displayLabel;
    QPushButton *LightRecognition;
    QComboBox *comboBox;
    QPushButton *PupilRecognition;
    QPushButton *start;
    QLabel *displayLabel_2;
    QPushButton *DarknessPushButton;
    QPushButton *start_2;
    QPushButton *stop;
    QPushButton *start3;

    void setupUi(QWidget *PupilDetect)
    {
        if (PupilDetect->objectName().isEmpty())
            PupilDetect->setObjectName("PupilDetect");
        PupilDetect->resize(2200, 1400);
        PupilDetect->setLayoutDirection(Qt::LayoutDirection::RightToLeft);
        PupilDetect->setStyleSheet(QString::fromUtf8("background-color: rgb(30, 30, 30);"));
        displayLabel = new QLabel(PupilDetect);
        displayLabel->setObjectName("displayLabel");
        displayLabel->setGeometry(QRect(0, 0, 1920, 1080));
        displayLabel->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        displayLabel->setAlignment(Qt::AlignmentFlag::AlignCenter);
        LightRecognition = new QPushButton(PupilDetect);
        LightRecognition->setObjectName("LightRecognition");
        LightRecognition->setGeometry(QRect(500, 1200, 200, 60));
        LightRecognition->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        comboBox = new QComboBox(PupilDetect);
        comboBox->addItem(QString());
        comboBox->setObjectName("comboBox");
        comboBox->setGeometry(QRect(20, 1200, 200, 60));
        comboBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
        comboBox->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        PupilRecognition = new QPushButton(PupilDetect);
        PupilRecognition->setObjectName("PupilRecognition");
        PupilRecognition->setGeometry(QRect(750, 1200, 200, 60));
        PupilRecognition->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        start = new QPushButton(PupilDetect);
        start->setObjectName("start");
        start->setGeometry(QRect(990, 1200, 200, 60));
        start->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:30px;"));
        displayLabel_2 = new QLabel(PupilDetect);
        displayLabel_2->setObjectName("displayLabel_2");
        displayLabel_2->setGeometry(QRect(1310, 1090, 441, 321));
        DarknessPushButton = new QPushButton(PupilDetect);
        DarknessPushButton->setObjectName("DarknessPushButton");
        DarknessPushButton->setGeometry(QRect(250, 1200, 200, 60));
        DarknessPushButton->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        start_2 = new QPushButton(PupilDetect);
        start_2->setObjectName("start_2");
        start_2->setGeometry(QRect(990, 1280, 200, 60));
        start_2->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:30px;"));
        stop = new QPushButton(PupilDetect);
        stop->setObjectName("stop");
        stop->setGeometry(QRect(500, 1280, 200, 60));
        stop->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:30px;"));
        start3 = new QPushButton(PupilDetect);
        start3->setObjectName("start3");
        start3->setGeometry(QRect(740, 1280, 200, 60));
        start3->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:30px;"));

        retranslateUi(PupilDetect);

        QMetaObject::connectSlotsByName(PupilDetect);
    } // setupUi

    void retranslateUi(QWidget *PupilDetect)
    {
        PupilDetect->setWindowTitle(QCoreApplication::translate("PupilDetect", "Form", nullptr));
        displayLabel->setText(QCoreApplication::translate("PupilDetect", "src", nullptr));
        LightRecognition->setText(QCoreApplication::translate("PupilDetect", "\345\205\211\346\226\221\350\257\206\345\210\253", nullptr));
        comboBox->setItemText(0, QCoreApplication::translate("PupilDetect", "        \350\256\276\345\244\207", nullptr));

        PupilRecognition->setText(QCoreApplication::translate("PupilDetect", "\347\236\263\345\255\224\350\257\206\345\210\253", nullptr));
        start->setText(QCoreApplication::translate("PupilDetect", "\345\274\200\345\247\213\346\265\201", nullptr));
        displayLabel_2->setText(QCoreApplication::translate("PupilDetect", "TextLabel", nullptr));
        DarknessPushButton->setText(QCoreApplication::translate("PupilDetect", "\346\234\200\346\232\227\347\202\271", nullptr));
        start_2->setText(QCoreApplication::translate("PupilDetect", "\345\205\250\351\203\250\345\274\200\345\247\213", nullptr));
        stop->setText(QCoreApplication::translate("PupilDetect", "\346\232\202\345\201\234", nullptr));
        start3->setText(QCoreApplication::translate("PupilDetect", "\345\274\200\345\247\213", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PupilDetect: public Ui_PupilDetect {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PUPILDETECT_H
