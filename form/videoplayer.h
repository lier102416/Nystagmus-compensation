#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <QWidget>
#include <QPushButton>
#include <cameracapture.h>
#include <QLabel>
#include <QTime>
#include <QComboBox>
#include <QCameraDevice>
#include <QMediaDevices>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
}
namespace Ui {
class videoPlayer;
}



class videoPlayer : public QWidget
{
    Q_OBJECT

public:
    explicit videoPlayer(QWidget *parent = nullptr);
    ~videoPlayer();
    void scanCreamDevice();
    QImage matToQImage(const cv::Mat& mat);

public slots:
    void onFrameReady(const cv::Mat& frame);
    void onStartClicked();
    void onStart2Clicked();
    void onStopCaputre();
    void onCameraError(const QString &error);

private:
    Ui::videoPlayer *ui;
    void setupUI();
    CameraCapture * m_camera;
    QLabel        * displayLabel;
    QComboBox     * cameraBox;
    QComboBox     * sizeBox;
    QComboBox     * FrameBox;
    QComboBox     * codingBox;

    int m_frameCount = 0;
    QList<QCameraDevice> cameras;// 摄像头列表

};

#endif // VIDEOPLAYER_H
