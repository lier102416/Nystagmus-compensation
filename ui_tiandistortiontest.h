/********************************************************************************
** Form generated from reading UI file 'tiandistortiontest.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TIANDISTORTIONTEST_H
#define UI_TIANDISTORTIONTEST_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>
#include "tiandistortiontest.h"

QT_BEGIN_NAMESPACE

class Ui_tiandistortiontest
{
public:
    QWidget *widget_2;
    QPushButton *pushButton_lase;
    TianDistortionTest *widget;
    QComboBox *comboBox;
    QPushButton *pushButton_start;
    QTextEdit *textEdit;
    QLabel *displayLabel;
    QPushButton *pushButton;

    void setupUi(QWidget *tiandistortiontest)
    {
        if (tiandistortiontest->objectName().isEmpty())
            tiandistortiontest->setObjectName("tiandistortiontest");
        tiandistortiontest->resize(2200, 1400);
        widget_2 = new QWidget(tiandistortiontest);
        widget_2->setObjectName("widget_2");
        widget_2->setGeometry(QRect(10, 0, 1931, 1381));
        pushButton_lase = new QPushButton(widget_2);
        pushButton_lase->setObjectName("pushButton_lase");
        pushButton_lase->setGeometry(QRect(480, 1310, 180, 60));
        pushButton_lase->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        widget = new TianDistortionTest(widget_2);
        widget->setObjectName("widget");
        widget->setGeometry(QRect(0, 0, 1920, 1080));
        widget->setStyleSheet(QString::fromUtf8("background-color: rgb(254, 255, 243);"));
        comboBox = new QComboBox(widget_2);
        comboBox->addItem(QString());
        comboBox->setObjectName("comboBox");
        comboBox->setGeometry(QRect(10, 1310, 180, 60));
        comboBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
        comboBox->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        pushButton_start = new QPushButton(widget_2);
        pushButton_start->setObjectName("pushButton_start");
        pushButton_start->setGeometry(QRect(240, 1310, 180, 60));
        pushButton_start->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        textEdit = new QTextEdit(widget_2);
        textEdit->setObjectName("textEdit");
        textEdit->setGeometry(QRect(0, 1080, 1024, 231));
        textEdit->setStyleSheet(QString::fromUtf8("background-color:rgb(0, 255, 255);"));
        displayLabel = new QLabel(widget_2);
        displayLabel->setObjectName("displayLabel");
        displayLabel->setGeometry(QRect(1450, 1080, 471, 301));
        displayLabel->setStyleSheet(QString::fromUtf8("background-color:rgb(85, 255, 255)"));
        pushButton = new QPushButton(widget_2);
        pushButton->setObjectName("pushButton");
        pushButton->setGeometry(QRect(930, 1330, 371, 51));

        retranslateUi(tiandistortiontest);

        QMetaObject::connectSlotsByName(tiandistortiontest);
    } // setupUi

    void retranslateUi(QWidget *tiandistortiontest)
    {
        tiandistortiontest->setWindowTitle(QCoreApplication::translate("tiandistortiontest", "Form", nullptr));
        pushButton_lase->setText(QCoreApplication::translate("tiandistortiontest", "\344\270\213\344\270\200\344\270\252", nullptr));
        comboBox->setItemText(0, QCoreApplication::translate("tiandistortiontest", "        \350\256\276\345\244\207", nullptr));

        pushButton_start->setText(QCoreApplication::translate("tiandistortiontest", "\345\274\200\345\247\213", nullptr));
        displayLabel->setText(QCoreApplication::translate("tiandistortiontest", "TextLabel", nullptr));
        pushButton->setText(QCoreApplication::translate("tiandistortiontest", "PushButton", nullptr));
    } // retranslateUi

};

namespace Ui {
    class tiandistortiontest: public Ui_tiandistortiontest {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TIANDISTORTIONTEST_H
