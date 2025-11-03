#include "videoplayer.h"
#include "qlabel.h"
#include "ui_videoplayer.h"
#include <QDebug>
#include <QMessageBox>


videoPlayer::videoPlayer(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::videoPlayer)
{
    ui->setupUi(this);
    setupUI();
    scanCreamDevice();
    m_camera = new CameraCapture(this);
    connect(m_camera, &CameraCapture::frameReady, this, &videoPlayer::onFrameReady);
    connect(m_camera, &CameraCapture::errorOccurred, this, &videoPlayer::onCameraError);

}

videoPlayer::~videoPlayer()
{
    delete ui;
}

void videoPlayer::onFrameReady(const cv::Mat& frame)
{
    if(frame.empty()) {
        return;
    }

    // 将cv::Mat转换为QImage
    QImage qimg = matToQImage(frame);

    // 缩放并显示图像
    QPixmap pixmap = QPixmap::fromImage(qimg.scaled(1700, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    displayLabel->setPixmap(pixmap);

    // 更新帧计数
    m_frameCount++;
    static QTime lastTime = QTime::currentTime();
    QTime currentTime = QTime::currentTime();
    if(lastTime.msecsTo(currentTime) >= 1000){
        qDebug()<<"实际帧率"<<m_frameCount<<"fps";
        m_frameCount = 0;
        lastTime = currentTime;
    }
}

// 辅助函数：将cv::Mat转换为QImage
QImage videoPlayer::matToQImage(const cv::Mat& mat)
{
    switch (mat.type()) {
    // 8位，BGRA格式
    case CV_8UC4: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image.rgbSwapped();
    }
    // 8位，BGR格式
    case CV_8UC3: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    // 8位，灰度格式
    case CV_8UC1: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return image;
    }
    default:
        qWarning("Mat type not supported for conversion to QImage");
        return QImage();
    }
}
void videoPlayer::onStartClicked()
{
    if(!m_camera->isOpened()){
        // 从分辨率ComboBox获取宽高
        QString selectedSize = sizeBox->currentText();
        QStringList sizeParts = selectedSize.split('x');
        if (sizeParts.size() == 2) {
            int width = sizeParts[0].toInt();
            int height = sizeParts[1].toInt();
            m_camera->setResoluution(width, height);
        } else {
            // 默认值
            m_camera->setResoluution(1280, 720);
        }

        // 从帧率ComboBox获取帧率
        QString selectedFrameRate = FrameBox->currentText();
        selectedFrameRate = selectedFrameRate.replace(" fps", ""); // 去掉" fps"后缀
        int frameRate = selectedFrameRate.toInt();
        if (frameRate > 0) {
            m_camera->setFramerate(frameRate);
        } else {
            m_camera->setFramerate(30); // 默认值
        }
        //获取选中的摄像头
        QString selectedCamera = cameraBox->currentText();
        if(m_camera->openCamera(selectedCamera)){
            m_camera->startCapture();
        }
        // 从编码格式ComboBox获取格式
        QString selectedFormat = codingBox->currentText();
        m_camera->setPixelFormat(selectedFormat);

        qDebug() << "启动摄像头参数:";
        qDebug() << "摄像头：" << selectedCamera;
        qDebug() << "分辨率:" << selectedSize;
        qDebug() << "帧率:" << frameRate << "fps";
        qDebug() << "编码格式:" << selectedFormat;


    }
}

void videoPlayer::onStart2Clicked()
{
    if(!m_camera->isOpened()){
        // 从分辨率ComboBox获取宽高
        QString selectedSize = sizeBox->currentText();
        QStringList sizeParts = selectedSize.split('x');
        if (sizeParts.size() == 2) {
            int width = sizeParts[0].toInt();
            int height = sizeParts[1].toInt();
            m_camera->setResoluution(width, height);
        } else {
            // 默认值
            m_camera->setResoluution(1280, 720);
        }

        // 从帧率ComboBox获取帧率
        QString selectedFrameRate = FrameBox->currentText();
        selectedFrameRate = selectedFrameRate.replace(" fps", ""); // 去掉" fps"后缀
        int frameRate = selectedFrameRate.toInt();
        if (frameRate > 0) {
            m_camera->setFramerate(frameRate);
        } else {
            m_camera->setFramerate(30); // 默认值
        }
        //获取选中的摄像头
        QString selectedCamera = cameraBox->currentText();
        if(m_camera->openCamera(selectedCamera)){
            m_camera->startCapture();
        }
        // 从编码格式ComboBox获取格式
        QString selectedFormat = codingBox->currentText();
        m_camera->setPixelFormat(selectedFormat);

        qDebug() << "启动摄像头参数:";
        qDebug() << "摄像头：" << selectedCamera;
        qDebug() << "分辨率:" << selectedSize;
        qDebug() << "帧率:" << frameRate << "fps";
        qDebug() << "编码格式:" << selectedFormat;

    }
}
void videoPlayer::onStopCaputre()
{
    m_camera->closeCamera();
}

void videoPlayer::onCameraError(const QString &error)
{
    qDebug()<<"摄像头错误："<<error;
    QMessageBox::warning(this, "摄像头错误", error);

}

void videoPlayer::setupUI()
{

    displayLabel = new QLabel(this);
    displayLabel->setMinimumSize( 1700, 900);
    displayLabel->setStyleSheet("border: 1px solid gray;");
    //摄像头
    cameraBox = new QComboBox(this);
    cameraBox->addItem(QString());
    cameraBox->setObjectName("comboBox");
    cameraBox->setGeometry(QRect(10, 900, 200, 60));
    cameraBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
    cameraBox->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                               "background:#3c3c3c;\n"
                                               "color: white;\n"
                                               "border-radius:20px;"));
    //分辨率
    sizeBox = new QComboBox(this);
    sizeBox->addItem(QString());
    sizeBox->setObjectName("comboBox");
    sizeBox->setGeometry(QRect(220, 900, 200, 60));
    sizeBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
    sizeBox->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                             "background:#3c3c3c;\n"
                                             "color: white;\n"
                                             "border-radius:20px;"));
    // 添加常用分辨率
    sizeBox->addItem("320x240");
    sizeBox->addItem("640x480");
    sizeBox->addItem("800x600");
    sizeBox->addItem("1024x768");
    sizeBox->addItem("1280x720");   // HD 720p
    sizeBox->addItem("1920x1080");  // Full HD 1080p
    sizeBox->addItem("2560x1440");  // 2K QHD
    sizeBox->addItem("3840x2160");  // 4K UHD
    sizeBox->setCurrentText("1280x720");

    //帧率
    FrameBox = new QComboBox(this);
    FrameBox->addItem(QString());
    FrameBox->setObjectName("comboBox");
    FrameBox->setGeometry(QRect(430, 900, 200, 60));
    FrameBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
    FrameBox->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                              "background:#3c3c3c;\n"
                                              "color: white;\n"
                                              "border-radius:20px;"));
    // 添加常用帧率
    FrameBox->addItem("15 fps");
    FrameBox->addItem("20 fps");
    FrameBox->addItem("24 fps");
    FrameBox->addItem("25 fps");
    FrameBox->addItem("30 fps");
    FrameBox->addItem("60 fps");

    FrameBox->setCurrentText("30 fps");

    codingBox = new QComboBox(this);
    codingBox->addItem(QString());
    codingBox->setObjectName("comboBox");
    codingBox->setGeometry(QRect(640, 900, 200, 60));
    codingBox->setLayoutDirection(Qt::LayoutDirection::LeftToRight);
    codingBox->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                               "background:#3c3c3c;\n"
                                               "color: white;\n"
                                               "border-radius:20px;"));
    // 添加常用编码格式
    codingBox->addItem("mjpeg");
    codingBox->addItem("yuyv422");
    codingBox->addItem("nv12");
    codingBox->addItem("yuv420p");
    codingBox->addItem("rgb24");
    codingBox->setCurrentText("mjpeg");

    QPushButton* m_startButton = new QPushButton("开始",this);
    m_startButton->setObjectName("StartButton");
    m_startButton->setGeometry(QRect(10, 965, 200, 60));  // 调整Y坐标，避免超出窗口
    m_startButton->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                                   "background:#3c3c3c;\n"
                                                   "color: white;\n"
                                                   "border-radius:20px;"));

    QPushButton* m_stopButton = new QPushButton("停止",this);
    m_stopButton->setObjectName("StopButton");
    m_stopButton->setGeometry(QRect(220, 965, 200, 60));  // 调整Y坐标
    m_stopButton->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                                  "background:#3c3c3c;\n"
                                                  "color: white;\n"
                                                  "border-radius:20px;"));

    QPushButton* m_start2Button = new QPushButton("全部开始",this);
    m_stopButton->setObjectName("StopButton");
    m_stopButton->setGeometry(QRect(330, 965, 200, 60));  // 调整Y坐标
    m_stopButton->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                                  "background:#3c3c3c;\n"
                                                  "color: white;\n"
                                                  "border-radius:20px;"));

    connect(m_startButton, &QPushButton::clicked, this, &videoPlayer::onStartClicked);
    connect(m_stopButton , &QPushButton::clicked, this, &videoPlayer::onStopCaputre);
    connect(m_start2Button , &QPushButton::clicked, this, &videoPlayer::onStart2Clicked);

}

void videoPlayer::scanCreamDevice()
{
    //获取可用摄像头列表
    cameras = QMediaDevices::videoInputs();
    cameraBox->clear();
    //添加摄像头到下拉列表
    for(const QCameraDevice &camera : cameras){
        qDebug()<<"adding camera:" <<camera.description();
        cameraBox->addItem(camera.description(), QVariant::fromValue(camera));
    }
    cameraBox->addItem("选择文件", QString("file"));

}
