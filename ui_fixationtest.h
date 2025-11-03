/********************************************************************************
** Form generated from reading UI file 'fixationtest.ui'
**
** Created by: Qt User Interface Compiler version 6.9.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_FIXATIONTEST_H
#define UI_FIXATIONTEST_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QWidget>
#include <fixationtest.h>

QT_BEGIN_NAMESPACE

class Ui_fixationTest
{
public:
    QWidget *widget;
    FixationTest *widget_2;
    QComboBox *comboBox;
    QPushButton *pushButton_start;
    QLabel *displayLabel;
    QTextEdit *textEdit;
    QPushButton *pushButton_over;
    QPushButton *pushButton_last;

    void setupUi(QWidget *fixationTest)
    {
        if (fixationTest->objectName().isEmpty())
            fixationTest->setObjectName("fixationTest");
        fixationTest->resize(2200, 1400);
        widget = new QWidget(fixationTest);
        widget->setObjectName("widget");
        widget->setGeometry(QRect(0, 0, 1920, 1080));
        widget_2 = new FixationTest(widget);
        widget_2->setObjectName("widget_2");
        widget_2->setGeometry(QRect(0, 0, 1920, 1080));
        widget_2->setStyleSheet(QString::fromUtf8("background-color:rgb(85, 255, 255)"));
        comboBox = new QComboBox(fixationTest);
        comboBox->addItem(QString());
        comboBox->setObjectName("comboBox");
        comboBox->setGeometry(QRect(1970, 30, 180, 60));
        comboBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
        comboBox->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        pushButton_start = new QPushButton(fixationTest);
        pushButton_start->setObjectName("pushButton_start");
        pushButton_start->setGeometry(QRect(1970, 160, 180, 60));
        pushButton_start->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        displayLabel = new QLabel(fixationTest);
        displayLabel->setObjectName("displayLabel");
        displayLabel->setGeometry(QRect(1520, 1080, 401, 311));
        displayLabel->setStyleSheet(QString::fromUtf8("background-color: rgb(212, 255, 255);"));
        textEdit = new QTextEdit(fixationTest);
        textEdit->setObjectName("textEdit");
        textEdit->setGeometry(QRect(0, 1090, 871, 301));
        textEdit->setStyleSheet(QString::fromUtf8("background-color: rgb(220, 255, 247);"));
        pushButton_over = new QPushButton(fixationTest);
        pushButton_over->setObjectName("pushButton_over");
        pushButton_over->setGeometry(QRect(1970, 440, 180, 60));
        pushButton_over->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));
        pushButton_last = new QPushButton(fixationTest);
        pushButton_last->setObjectName("pushButton_last");
        pushButton_last->setGeometry(QRect(1970, 300, 180, 60));
        pushButton_last->setStyleSheet(QString::fromUtf8("background:transparent; \n"
"background:#3c3c3c;\n"
"color: white;\n"
"border-radius:20px;"));

        retranslateUi(fixationTest);

        QMetaObject::connectSlotsByName(fixationTest);
    } // setupUi

    void retranslateUi(QWidget *fixationTest)
    {
        fixationTest->setWindowTitle(QCoreApplication::translate("fixationTest", "Form", nullptr));
        comboBox->setItemText(0, QCoreApplication::translate("fixationTest", "        \350\256\276\345\244\207", nullptr));

        pushButton_start->setText(QCoreApplication::translate("fixationTest", "\345\274\200\345\247\213", nullptr));
        displayLabel->setText(QCoreApplication::translate("fixationTest", "TextLabel", nullptr));
        pushButton_over->setText(QCoreApplication::translate("fixationTest", "\347\273\223\346\235\237", nullptr));
        pushButton_last->setText(QCoreApplication::translate("fixationTest", "\344\270\213\344\270\200\344\270\252", nullptr));
    } // retranslateUi

};

namespace Ui {
    class fixationTest: public Ui_fixationTest {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_FIXATIONTEST_H
