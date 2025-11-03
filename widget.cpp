#include "widget.h"
#include "ui_widget.h"
#include <opencv2/opencv.hpp>

struct WidgetItemData {
    QString title;
    typedef QWidget* (*CreateInstanceFunc)();  // 定义返回 QWidget* 类型的函数指针类型
    CreateInstanceFunc createInstance;  // 函数指针成员
};

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    // ui->setupUi(this);
    widget = new QWidget(this);
    this->setGeometry(0, 0, 2560, 1500);
    this->setObjectName("project");
    widget->resize(this->size());
    this->windows_init();

}

Widget::~Widget()
{
    delete ui;
}

void Widget::windows_init()
{
    /* 垂直布局实例化 */
    hBoxLayout = new QHBoxLayout();
    /* 堆栈部件实例化 */
    stackedWidget = new QStackedWidget();
    /* 列表实例化 */
    listWidget = new QListWidget(this);
    listWidget->setStyleSheet("background-color: black;");
    WidgetItemData items[] = {
        {"视频播放器",  []() -> QWidget * { return new videoPlayer(); } },
        {"瞳孔光斑检测", []() -> QWidget * { return new PupilDetect(); } },
        {"校验点测试", []() -> QWidget * { return new tiandistortiontest(); } },
        {"注视点测试", []() -> QWidget * { return new fixationTest(); } },
        {"诱导眼震测试", []() -> QWidget * { return new eyeTrack(); } },
    };
    for(auto &itemDate :items)
    {
       QListWidgetItem *item = new QListWidgetItem(itemDate.title);
       item->setForeground(Qt::white);
       item->setTextAlignment(Qt::AlignCenter);
       item->setSizeHint(QSize(100,150));
       listWidget->addItem(item);
       // 如果有对应的工厂函数，添加到stackedWidget中
       if (itemDate.createInstance) {
           stackedWidget->addWidget(itemDate.createInstance());
       }
    }

    // 将焦点设置为第一个项目
    if(listWidget->count() > 0) {
        QListWidgetItem* item1 = listWidget->item(0);
        listWidget->setCurrentItem(item1);
        listWidget->addItem(item1);
        item1->setSelected(true);
        listWidget->setFocus();
    }

    /* 设置列表的最大宽度 */
    listWidget->setMaximumWidth(150);
    /* 添加到水平布局 */
    hBoxLayout->addWidget(listWidget);
    hBoxLayout->addWidget(stackedWidget);



    /* 将widget的布局设置成hboxLayout */
    widget->setLayout(hBoxLayout);


    connect(listWidget, SIGNAL(currentRowChanged(int)),
            this, SLOT(onListItemChanged(int)));
    listWidget->setCurrentRow(0);

}

void Widget::onListItemChanged(int index)
{
    // 如果切换到诱导眼震测试页面
    tiandistortiontest* distortionTest = getDistortionTest();
    eyeTrack* eyeTrackInstance = getEyeTrack();
    fixationTest * fixationTestInstance = getFixationTesk();
    if (index == 3 ) {
        if (distortionTest && fixationTestInstance) {
            // 传递数据
            fixationTestInstance->acceptanceCoefficient(distortionTest->m_mappingCoefficients, distortionTest->m_combinedMappingCoefficients);
        }
    }
    else if(index == 4){
        if (distortionTest && eyeTrackInstance) {
            // 传递数据
            eyeTrackInstance->acceptanceCoefficient(distortionTest->m_mappingCoefficients, distortionTest->m_combinedMappingCoefficients);
    }
    }
    qDebug()<<"test123";
    stackedWidget->setCurrentIndex(index);
}
