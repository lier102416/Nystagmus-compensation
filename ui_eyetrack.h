
#ifndef UI_EYETRACK_H
#define UI_EYETRACK_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_eyeTrack
{
public:
    QLabel *VideoLabel;
    QLabel *displayLabel;
    QTextEdit *textEdit;
    QPushButton *ReducePushButton;
    QPushButton *AddPushButton;
    QPushButton *DirectionPushButton;
    QComboBox *comboBox;
    QPushButton *OutPushButton;
    QPushButton *StarPushButton;
    QPushButton *OutSavePushButton;
    QPushButton *NystagmusSimulation;

    void setupUi(QWidget *eyeTrack)
    {
        if (eyeTrack->objectName().isEmpty())
            eyeTrack->setObjectName("eyeTrack");
        eyeTrack->resize(2200, 1400);
        eyeTrack->setStyleSheet(QString::fromUtf8("background-color:rgb(30,30,30);"));
        VideoLabel = new QLabel(eyeTrack);
        VideoLabel->setObjectName("VideoLabel");
        VideoLabel->setGeometry(QRect(0, 0, 1920, 1080));
        VideoLabel->setStyleSheet(QString::fromUtf8("background-color: rgb(234, 243, 255);"));
        displayLabel = new QLabel(eyeTrack);
        displayLabel->setObjectName("displayLabel");
        displayLabel->setGeometry(QRect(1920, 50, 400, 400));
        displayLabel->setStyleSheet(QString::fromUtf8("background-color: rgb(220, 255, 247);"));
        textEdit = new QTextEdit(eyeTrack);
        textEdit->setObjectName("textEdit");
        textEdit->setGeometry(QRect(0, 1080, 600, 300));
        textEdit->setStyleSheet(QString::fromUtf8("background-color: rgb(220, 255, 247);"));
        ReducePushButton = new QPushButton(eyeTrack);
        ReducePushButton->setObjectName("ReducePushButton");
        ReducePushButton->setGeometry(QRect(1950, 590, 171, 51));
        ReducePushButton->setStyleSheet(QString::fromUtf8("background-color:rgb(255, 255, 255);"));
        AddPushButton = new QPushButton(eyeTrack);
        AddPushButton->setObjectName("AddPushButton");
        AddPushButton->setGeometry(QRect(1950, 500, 171, 51));
        AddPushButton->setStyleSheet(QString::fromUtf8("background-color:rgb(255, 255, 255);"));
        DirectionPushButton = new QPushButton(eyeTrack);
        DirectionPushButton->setObjectName("DirectionPushButton");
        DirectionPushButton->setGeometry(QRect(1950, 780, 171, 51));
        DirectionPushButton->setStyleSheet(QString::fromUtf8("background-color:rgb(255, 255, 255);"));
        comboBox = new QComboBox(eyeTrack);
        comboBox->addItem(QString());
        comboBox->setObjectName("comboBox");
        comboBox->setGeometry(QRect(1950, 690, 171, 51));
        comboBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
        comboBox->setStyleSheet(QString::fromUtf8("color: rgb(255, 255, 255);\n"
"color: black;\n"
"background-color: rgb(250, 255, 246);"));
        OutPushButton = new QPushButton(eyeTrack);
        OutPushButton->setObjectName("OutPushButton");
        OutPushButton->setGeometry(QRect(1950, 860, 171, 51));
        OutPushButton->setStyleSheet(QString::fromUtf8("background-color:rgb(255, 255, 255);"));
        StarPushButton = new QPushButton(eyeTrack);
        StarPushButton->setObjectName("StarPushButton");
        StarPushButton->setGeometry(QRect(1950, 1030, 171, 51));
        StarPushButton->setStyleSheet(QString::fromUtf8("background-color:rgb(255, 255, 255);"));
        OutSavePushButton = new QPushButton(eyeTrack);
        OutSavePushButton->setObjectName("OutSavePushButton");
        OutSavePushButton->setGeometry(QRect(1950, 950, 171, 51));
        OutSavePushButton->setStyleSheet(QString::fromUtf8("background-color:rgb(255, 255, 255);"));
        NystagmusSimulation = new QPushButton(eyeTrack);
        NystagmusSimulation->setObjectName("NystagmusSimulation");
        NystagmusSimulation->setGeometry(QRect(1950, 1110, 171, 51));
        NystagmusSimulation->setStyleSheet(QString::fromUtf8("background-color:rgb(255, 255, 255);"));

        retranslateUi(eyeTrack);

        QMetaObject::connectSlotsByName(eyeTrack);
    } // setupUi

    void retranslateUi(QWidget *eyeTrack)
    {
        eyeTrack->setWindowTitle(QCoreApplication::translate("eyeTrack", "Form", nullptr));
        VideoLabel->setText(QCoreApplication::translate("eyeTrack", "TextLabel", nullptr));
        displayLabel->setText(QCoreApplication::translate("eyeTrack", "TextLabel", nullptr));
        ReducePushButton->setText(QCoreApplication::translate("eyeTrack", "\351\200\237\345\272\246-", nullptr));
        AddPushButton->setText(QCoreApplication::translate("eyeTrack", "\351\200\237\345\272\246+", nullptr));
        DirectionPushButton->setText(QCoreApplication::translate("eyeTrack", "\347\272\265\345\220\221", nullptr));
        comboBox->setItemText(0, QCoreApplication::translate("eyeTrack", "        \350\256\276\345\244\207", nullptr));

        OutPushButton->setText(QCoreApplication::translate("eyeTrack", "\351\200\200\345\207\272", nullptr));
        StarPushButton->setText(QCoreApplication::translate("eyeTrack", "\351\242\204\346\265\213\347\237\253\346\255\243", nullptr));
        OutSavePushButton->setText(QCoreApplication::translate("eyeTrack", "\344\277\235\345\255\230\351\200\200\345\207\272", nullptr));
        NystagmusSimulation->setText(QCoreApplication::translate("eyeTrack", "\347\234\274\351\234\207\346\250\241\346\213\237", nullptr));
    } // retranslateUi

};

namespace Ui {
    class eyeTrack: public Ui_eyeTrack {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EYETRACK_H
