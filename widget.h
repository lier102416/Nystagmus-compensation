#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLabel>
#include "pupildetect.h"
#include "videoplayer.h"
#include "fixationtest.h"
#include "tiandistortiontest.h"
#include "eyetrack.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    tiandistortiontest* getDistortionTest() {
        return qobject_cast<tiandistortiontest*>(stackedWidget->widget(2));
    }

    fixationTest * getFixationTesk(){
        return qobject_cast<fixationTest*>(stackedWidget->widget(3));
    }

    eyeTrack* getEyeTrack() {
        return qobject_cast<eyeTrack*>(stackedWidget->widget(4));
    }
private:
    Ui::Widget *ui;
    /*widget 部件 */
    QWidget *widget;
    /*水平布局 */
    QHBoxLayout *hBoxLayout;
    /*列表视图 */
    QListWidget *listWidget;
    /* 堆栈窗口部件 */
    QStackedWidget * stackedWidget;
    /* 3个标签 */
    QLabel *label[3];
    void windows_init();
private slots:
    void onListItemChanged(int index);


};
#endif // WIDGET_H
