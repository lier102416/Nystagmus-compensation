#ifndef FIXATIONTEST_H
#define FIXATIONTEST_H


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
#include <QThread>
#include <QPainterPath>
#include <cmath>
#include <fstream>
#include <QFileDialog>
#include <pipline.h>
#include "videocapturepip.h"
#include "rolextractionpip.h"
#include "pupilextractionpip.h"
#include "spotextractionpip.h"
#include "mergedprocessingpip.h"
namespace Ui {
class fixationTest;
}

class fixationTest : public QWidget
{
    Q_OBJECT

public:
    explicit fixationTest(QWidget *parent = nullptr);
    ~fixationTest();

    //设备扫描
    void scanCreamDevice();
    //图像更新显示



    //接受处理好的映射系数
    void acceptanceCoefficient(const std::vector<MappingCoefficients> & coefficients, const MappingCoefficients & coefficient);
    void initializeDefaultMappingCoefficients();

    //打印系数
    void printceCoefficient(const std::vector<MappingCoefficients> & coeffs, const MappingCoefficients & coeff);
    //多映射函数注视点计算
    std::vector<cv::Point2f> calculateGazePoint(
        const cv::Point &light1Rol,  // 左上光斑
        const cv::Point &light2Rol,  // 右上光斑
        const cv::Point &light3Rol,  // 左下光斑
        const cv::Point &light4Rol,  // 右下光斑
        const cv::Point &pupil);      // 瞳孔中心
    //单模型注视点映射
    cv::Point2f calculateGazePointWithCombinedModel(const cv::Point& pupil,
                                                    const cv::Point& light1,
                                                    const cv::Point& light2,
                                                    const cv::Point& light3,
                                                    const cv::Point& light4);
private slots:

    void on_pushButton_start_clicked();
    void on_pushButton_over_clicked();

    void on_pushButton_last_clicked();

    void processVideoFrame(int frameId,bool success);

signals:

    void sendGazePoint(std::vector<cv::Point2f> Gaze);

private:
    QElapsedTimer QEtimer;
    Ui::fixationTest *ui;
    QList<QCameraDevice> cameras;// 摄像头列表
    QStringList cameraIndexList;  // 存储摄像头索引
    bool cameraFlag = false;
    std::vector<MappingCoefficients> m_mappingCoefficients;//映射函数系数
    MappingCoefficients combinedMappingCoefficients;//映射函数系数
    std::vector<cv::Point2f> Gaze;
};

class FixationTest : public QWidget
{
    Q_OBJECT

public:
    typedef struct RectROI{
        int row; //行
        int col; //列
        float width; //宽
        float height; // 高
    }RectROI;


    explicit FixationTest(QWidget *parent = nullptr);
    ~FixationTest();
    void paintEvent(QPaintEvent * event)override; //继承于QWidget类，重写paintEvent函数
    template <int N>
    void initRectROI(RectROI (&RectROIs)[N] , float w, float h); //图像初始化函数
    void mappingCalculation();
    void paintROITest(QPainter &painter);
    void startTest();
    void nextROI();

    QTimer* timer; //图像重绘定时器
    std::vector<cv::Point> Calculate_light1; //光斑点集
    std::vector<cv::Point> Calculate_light2; //光斑点集
    std::vector<cv::Point> Calculate_light3; //光斑点集
    std::vector<cv::Point> Calculate_light4; //光斑点集
    std::vector<cv::Point> Calculate_pupil; //瞳孔点集
    bool start = true;
    pipline *pip;
    videoCapturePip * cameraPipe;
    MergedProcessingPip *mergedPip;


    std::vector<cv::Point2f> Gaze;
    cv::Point2f pt2{0.0f, 0.0f};
    QStringList cameraIndexList;  // 存储摄像头索引

private:
    RectROI * m_RectROI = nullptr;
    RectROI m_RectROIs[15 * 9];
    int m_TestIndex = 0;
    bool m_ROITestFinish = false;
    float stepX = 1920.0 / 15.0;
    float stepY = 1080.0 / 9.0;
    std::vector<int> m_RoiIndex;
    std::vector<cv::Point2f> fixationSet;

public slots:
    void updateWidget();     //更新widget绘制图像槽函数

    void getGazePoint(std::vector<cv::Point2f> gaze);
};

#endif // FIXATIONTEST_H
