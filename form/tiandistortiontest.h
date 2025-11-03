#ifndef TIANDISTORTIONTEST_H
#define TIANDISTORTIONTEST_H

#include <QWidget>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QDebug>
#include <QWidget>
#include <QPainter>
#include <qmath.h>
#include <QTimer>
#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <QKeyEvent>
#include <QDebug>
#include <QPushButton>
#include <QCameraDevice>
#include <opencv2/opencv.hpp>
 #include "class.h"
#include <QMediaDevices>
#include <pipline.h>
#include "videocapturepip.h"
#include "rolextractionpip.h"
#include "pupilextractionpip.h"
#include "spotextractionpip.h"
#include <QLabel>
#include <QComboBox>
#include "mergedprocessingpip.h"
#include "datesave.h"
namespace Ui {
class tiandistortiontest;
}



enum class PointType {
    LIGHT1,
    LIGHT2,
    LIGHT3,
    LIGHT4,
    PUPIL
};

struct MeasurementPoint {
    cv::Point light1;
    cv::Point light2;
    cv::Point light3;
    cv::Point light4;
    cv::Point pupil;
};



class TianDistortionTest : public QWidget
{
    Q_OBJECT

public:
    typedef struct RectROI {
        int row;            //行
        int col;            //列
        float width;        //宽
        float height;       //高
        bool isDistortional;//是否变形

    }RectROI;
    QTimer* timer; //图像重绘定时器
    std::vector<MappingCoefficients> mappingCoefficients; // 映射系数向量，对应两个光斑组
    MappingCoefficients combinedMappingCoefficients;
    bool start = true; //绘制开始标志
    std::vector<cv::Point> Calculate_light1; //光斑点集
    std::vector<cv::Point> Calculate_light2; //光斑点集
    std::vector<cv::Point> Calculate_light3; //光斑点集
    std::vector<cv::Point> Calculate_light4; //光斑点集
    std::vector<cv::Point> Calculate_pupil; //瞳孔点集
    QString file ;
    int cameraIndex;
    bool equipMentFlag = 0; //设备标志位，为0是摄像头，为1则是文件
    int count = 0;            //计数检验的次数
    pipline *pip;
    MergedProcessingPip * mergedPip;
    videoCapturePip * cameraPipe;

    dateSave ImageSave;


public:
    explicit TianDistortionTest(QWidget *parent = nullptr);
    ~TianDistortionTest();
    template <int N>
    void initRectROI(RectROI (&RectROIs)[N] , float w, float h);
    void paintEvent(QPaintEvent *event)override;//继承于QWidget类，需要重写此函数，定时器执行update函数时对图像进行绘制
    void setShow(bool show);   //通过ui界面按键更改类中show变量的值
    void setStart(bool show);  //通过ui界面按键更改类中start变量的值
    void AverageValueCalculation(void); //计算一次绘制时注视点的平均值
    void enhancedMappingCalculation();

    // std::vector<MappingCoefficients> mappingCoefficients; // 映射系数（4组）
    void onButton1Clicked();//下一次绘制
    void startTest();        //启动检测函数
    void SaveCollectingData();

    void CalculateSimpleAverage(int cnt, int currentCount);
    void CalculateAverageFromPoints(const std::vector<MeasurementPoint>& points, int currentCount);
    cv::Point CalculateMean(const std::vector<MeasurementPoint>& points, PointType type);
    double CalculateVariance(const std::vector<MeasurementPoint>& points,
                             const cv::Point& mean, PointType type);
    double CalculateDistance(const cv::Point& p1, const cv::Point& p2);

private slots:
    void updateWidget();     //更新widget绘制图像槽函数
    void nextLineOrROI();    //执行下一次划线
    void mappingCalculation(); //计算映射函数

private:
    void paintROITest(QPainter &painter);   //绘制
    void initDotLine();
    RectROI m_RectROIs[15*9]; //
    std::vector<int> m_RoiIndex;
    RectROI *m_RectROI = nullptr;
    int m_TestIndex = 0;
    int m_LineIndex = 0;
    bool m_ROITestFinish = false;
    bool m_dotLineIsH = true;

    float stepX = 1920.0 / 15.0;//1000是这个widget的宽，因为不会改变，就直接写1000了
    float stepY = 1080.0 / 9.0;//1000是这个widget的高，因为不会改变，就直接写1000了
    bool setshow = false;
    bool detectionFlag = 0;
    std::vector<cv::Point2f> fixationSet;
    std::vector<cv::Point> lightRol_1; //光斑点集
    std::vector<cv::Point> lightRol_2; //光斑点集
    std::vector<cv::Point> lightRol_3; //光斑点集
    std::vector<cv::Point> lightRol_4; //光斑点集
    std::vector<cv::Point> pupilRol;   //瞳孔点集
    std::vector<std::vector <MeasurementPoint>> collectingData;



signals:
    void boolVariableChanged(bool newValue);
    void countReached29();

};

class tiandistortiontest : public QWidget
{
    Q_OBJECT

public:
    explicit tiandistortiontest(QWidget *parent = nullptr);
    ~tiandistortiontest();
    std::vector<MappingCoefficients> m_mappingCoefficients;
    MappingCoefficients m_combinedMappingCoefficients;

    void scanCreamDevice();//寻找设备


    // void saveData(int count, const cv::Point& light1, const cv::Point& light2, const cv::Point& pupil);
private slots:

    void stopWidget();
    void processVideoFrame(int frameId, bool success);

    void on_pushButton_start_clicked();

    void on_pushButton_lase_clicked();

private:
    Ui::tiandistortiontest *ui;
    QList<QCameraDevice> cameras;// 摄像头列表
    bool cameraFlag = false;
    QMutex m_dataMutex;  // 保护共享数据的互斥锁

};

#endif // TIANDISTORTIONTEST_H
