#ifndef PUPILDETECT_H
#define PUPILDETECT_H

#include <QWidget>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QComboBox>
#include <QTimer>
#include <QMediaCaptureSession>
#include <QVideoSink>
#include <QMediaPlayer>
#include <opencv2/opencv.hpp>  // 包含 OpenCV 的头文件
#include <QDebug>
#include "extractthread.h"
#include "videoprocess.h"
#include "videocapturepip.h"
#include "rolextractionpip.h"
#include "pupilextractionpip.h"
#include "spotextractionpip.h"
#include "mergedprocessingpip.h"

namespace Ui {
class PupilDetect;
}

class PupilDetect : public QWidget
{
    Q_OBJECT

public:
    explicit PupilDetect(QWidget *parent = nullptr);
    ~PupilDetect();
    void scanCreamDevice();
    void channelEnable();
private slots:
    void on_start_clicked();
    void processVideoFrame(int frameId);


    void on_PupilRecognition_clicked();
    void on_LightRecognition_clicked();

    void on_Darkness_clicked();

    void on_start_2_clicked();

    void on_stop_clicked();

    void on_start3_clicked();

    void on_DarknessPushButton_clicked();

private:
    Ui::PupilDetect *ui;
    QList<QCameraDevice> cameras;// 摄像头列表
    cv::VideoCapture *videoCapture;
    QVariant  source;

    pipline *pip;
    videoCapturePip * cameraPipe;
    MergedProcessingPip * mergedPip;
    rolExtractionPip * rolExtraction;
    pupilExtractionPip * pupillExtraction;
    SpotExtractionPip * spotExtraction;


    bool cameraFlag = false;
    bool DarkFlag   = false;
    bool PupilFlag  = false;
    bool SpotFlag   = false;
    bool RoiFlag    = false;
    bool mergFlag   = false;
};

#endif // PUPILDETECT_H
