#include "pupildetect.h"
#include "ui_pupildetect.h"
#include <QFileDialog>


PupilDetect::PupilDetect(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PupilDetect)
{
    ui->setupUi(this);
    pip = new pipline;
    cameraPipe = new videoCapturePip;
    rolExtraction = new rolExtractionPip;
    pupillExtraction = new pupilExtractionPip;
    spotExtraction = new SpotExtractionPip;
    mergedPip      = new MergedProcessingPip;
    connect(ui->DarknessPushButton, &QPushButton::clicked, this,&PupilDetect:: on_Darkness_clicked);
    /*扫描摄像头*/
    scanCreamDevice();
}

PupilDetect::~PupilDetect()
{
    delete pip;
    delete cameraPipe;
    delete rolExtraction;
    delete pupillExtraction;
    delete spotExtraction;
    delete ui;
}

void PupilDetect::scanCreamDevice()
{
    //获取可用摄像头列表
    cameras = QMediaDevices::videoInputs();
    ui->comboBox->clear();
    //添加摄像头到下拉列表
    for(const QCameraDevice &camera : cameras){
        qDebug()<<"adding camera:" <<camera.description();
        ui->comboBox->addItem(camera.description(), QVariant::fromValue(camera));
    }
    ui->comboBox->addItem("选择文件", QString("file"));
    source = ui->comboBox->currentIndex();
}

void PupilDetect::channelEnable()
{
    disconnect(cameraPipe, &videoCapturePip::sendOverSign, this, &PupilDetect::processVideoFrame);
    disconnect(rolExtraction, &rolExtractionPip::sendOverSign, this, &PupilDetect::processVideoFrame);
    disconnect(pupillExtraction, &pupilExtractionPip::sendOverSign, this, &PupilDetect::processVideoFrame);
    disconnect(spotExtraction, &SpotExtractionPip::sendOverSign, this, &PupilDetect::processVideoFrame);
    disconnect(mergedPip, &MergedProcessingPip::sendOverSign, this, &PupilDetect::processVideoFrame);

    if(mergFlag){
        connect(mergedPip, &MergedProcessingPip::sendOverSign, this, &PupilDetect::processVideoFrame);

    }
    else if(PupilFlag){
        // SpotFlag优先级最高，如果为true，则连接spotExtraction
        connect(pupillExtraction, &pupilExtractionPip::sendOverSign, this, &PupilDetect::processVideoFrame);

        qDebug() << "已连接 spotExtraction 信号";
    }
    else if(SpotFlag){
        // 其次是PupilFlag
        connect(spotExtraction, &SpotExtractionPip::sendOverSign, this, &PupilDetect::processVideoFrame);

        qDebug() << "已连接 pupillExtraction 信号";
    }
    else if(DarkFlag){
        // 然后是RolFlag
        connect(rolExtraction, &rolExtractionPip::sendOverSign, this, &PupilDetect::processVideoFrame);
        qDebug() << "已连接 rolExtraction 信号";
    }
    else if(cameraFlag){
        connect(cameraPipe, &videoCapturePip::sendOverSign, this, &PupilDetect::processVideoFrame);
        qDebug() << "已连接 cameraPipe 信号";
    }

    qDebug()<<"channelEnable"<<cameraFlag<<PupilFlag<<SpotFlag;
}

//开始按钮
void PupilDetect::on_start_clicked()
{
    cameraFlag = !cameraFlag;
    if(cameraFlag){
        int index = ui->comboBox->currentIndex();
        if (index == -1) {
            qWarning() << "未选择摄像头";
            return;
        }

        QVariant selectedItemData = ui->comboBox->itemData(index);
        qDebug() << "selectedItemData type:" << selectedItemData.typeName();
        qDebug() << "selectedItemData:" << selectedItemData.toString();

        if (selectedItemData.toString() == "file") {
            QString filePath = QFileDialog::getOpenFileName(this, "选择视频文件", "", "Videos (*.mp4 *.avi *.mjpeg)");
            if (filePath.isEmpty()) {
                qWarning() << "文件为空";
                return;
            }
            cameraPipe->setSource(1, filePath);
        } else {
            // 选择的是摄像头
            QCameraDevice selectedCamera = selectedItemData.value<QCameraDevice>();
            qDebug() << "selectedCamera:" << selectedCamera.description();
            if (selectedCamera.isNull()) {
                qWarning() << "选择摄像头无效";
                return;
            }
            QString cameraIndex = ui->comboBox->currentText();
            cameraPipe->setSource(0, cameraIndex);
        }
        //确保管道完全停止
        pip->deletePipeLine();
        pip->creat_capturepip(cameraPipe, false);
        pip->createPipeLine();
        this->ui->start->setText("关闭摄像师");
    }
    else
    {
        //暂停并删除
        pip->deletePipeLine();

        if(cameraPipe){
            cameraPipe->setExit(true);
            cameraPipe->resetSource();
        }
        this->ui->start->setText("开启摄像");
        qDebug()<<"清除完毕";
        pip->safeDeletePipeline();
        this->ui->displayLabel->clear();
        return;
    }
     channelEnable();
}

void PupilDetect::on_Darkness_clicked()
{
    DarkFlag = !DarkFlag;
    pip->deleteALLPip();
    if(DarkFlag){

        this->ui->DarknessPushButton->setText("关闭最暗点");
        pip->add_process_modles(rolExtraction);
    }
    else{
        this->ui->DarknessPushButton->setText("最暗点");
        pip->remove_process_modles(rolExtraction);
    }
    pip->createPipeLine();
    channelEnable();
}

void PupilDetect::on_PupilRecognition_clicked()
{
    PupilFlag = !PupilFlag;
    pip->deleteALLPip();
    if(PupilFlag){

        this->ui->PupilRecognition->setText("关闭识别");
        pip->add_process_modles(pupillExtraction);
    }
    else{
        this->ui->PupilRecognition->setText("瞳孔识别");
        pip->remove_process_modles(pupillExtraction);
    }
    pip->createPipeLine();
    channelEnable();
}

void PupilDetect::on_LightRecognition_clicked()
{
    SpotFlag = !SpotFlag;
    pip->deleteALLPip();
    if(SpotFlag){
        this->ui->LightRecognition->setText("关闭识别");

        pip->add_process_modles(spotExtraction);
    }
    else{
        this->ui->LightRecognition->setText("光斑识别");

        pip->remove_process_modles(spotExtraction);
    }
    pip->createPipeLine();
    channelEnable();
}


void PupilDetect::processVideoFrame(int frameId)
{
    FrameData frameData;
    cv::Mat rgbImage;
    if(SharedPipelineData::getFrameData(frameId, frameData)) {
        cv::Mat srcImage = frameData.originalImage;

        if(srcImage.empty()){
            qDebug()<<"getOriginaldImage 为空";
            return;
        }

        if(srcImage.channels() < 3)
            cv::cvtColor(srcImage, rgbImage, cv::COLOR_GRAY2BGR);
        else
            cv::cvtColor(srcImage, rgbImage, cv::COLOR_BGR2RGB);

        // 处理 DarkPoint（需要确认 FrameData 中是否有这些数据）
        cv::Point DarkPoint = frameData.darkPoint;    // 假设 FrameData 中有这个字段
        cv::Point RolPoint = frameData.roiPoint;      // 假设 FrameData 中有这个字段

        if(DarkFlag){
            DarkPoint.x += RolPoint.x;
            DarkPoint.y += RolPoint.y;
        }

        if(PupilFlag||mergFlag) // 绘制瞳孔
            visualizePupilDetection(rgbImage, frameData.pupilCircle);

        if(SpotFlag||mergFlag) // 绘制光斑
            for (const auto& point : frameData.lightPoints) {
                cv::circle(rgbImage, point.center, point.radius,
                           cv::Scalar(0, 0, 255), 2);
            }

        QImage Qimg(rgbImage.data, rgbImage.cols, rgbImage.rows,
                    rgbImage.step, QImage::Format_RGB888);
        ui->displayLabel->setPixmap(QPixmap::fromImage(Qimg).scaled(
        ui->displayLabel->size(), Qt::KeepAspectRatio));
    }
}



// void PupilDetect::processVideoFrame(int frameId)
// {
    // cv::Mat rgbImage;
    // cv::Mat srcImage    = SharedPipelineData::getOriginaldImage();
    // Circle pupil        = SharedPipelineData::getPupilCircle();
    // cv::Point whitePoint = SharedPipelineData::getWhitePoint();
    // cv::Point DarkPoint = SharedPipelineData::getDarkPoint();
    // cv::Point RolPoint  = SharedPipelineData::getRoiPoint();
    // std::vector<Circle> lights = SharedPipelineData::getLightPoints();
    // if(srcImage.empty()){
    //     qDebug()<<"getOriginaldImage 为空";
    // }
    // if(srcImage.channels() <3)
    //     cv::cvtColor(srcImage, rgbImage, cv::COLOR_GRAY2BGR);
    // else
    //     cv::cvtColor(srcImage,rgbImage,cv::COLOR_BGR2RGB);

    // if(DarkFlag){
    //     DarkPoint.x    += RolPoint.x;
    //     DarkPoint.y    += RolPoint.y;

    // }
    // if(PupilFlag) //绘制瞳孔
    //     cv::circle(rgbImage, pupil.center, pupil.radius, cv::Scalar(0, 255, 0), 2);
    // if(SpotFlag) //获知瞳孔
    // for (const auto& point : lights) {
    //     cv::circle(rgbImage, point.center, point.radius, cv::Scalar(0, 0, 255), 2);
    // }
    // cv::circle(rgbImage, whitePoint, 3, cv::Scalar(255, 0,0), -1);

    // QImage Qimg(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
    // ui->displayLabel->setPixmap(QPixmap::fromImage(Qimg).scaled(ui->displayLabel->size(), Qt::KeepAspectRatio));
// }







void PupilDetect::on_start_2_clicked()
{
    mergFlag = !mergFlag;
    if(mergFlag){
        int index = ui->comboBox->currentIndex();
        if (index == -1) {
            qWarning() << "未选择摄像头";
            return;
        }

        QVariant selectedItemData = ui->comboBox->itemData(index);
        qDebug() << "selectedItemData type:" << selectedItemData.typeName();
        qDebug() << "selectedItemData:" << selectedItemData.toString();

        if (selectedItemData.toString() == "file") {
            QString filePath = QFileDialog::getOpenFileName(this, "选择视频文件", "", "Videos (*.mp4 *.avi *.mjpeg)");
            if (filePath.isEmpty()) {
                qWarning() << "文件为空";
                return;
            }
            cameraPipe->setSource(1, filePath);
        } else {
            // 选择的是摄像头
            QCameraDevice selectedCamera = selectedItemData.value<QCameraDevice>();
            qDebug() << "selectedCamera:" << selectedCamera.description();
            if (selectedCamera.isNull()) {
                qWarning() << "选择摄像头无效";
                return;
            }
            QString cameraIndex = ui->comboBox->currentText();
            cameraPipe->setSource(0, cameraIndex);
        }
        //确保管道完全停止
        pip->deletePipeLine();
        pip->add_process_modles(mergedPip);
        pip->creat_capturepip(cameraPipe, false);

        pip->createPipeLine();
        this->ui->start_2->setText("关闭摄像师");
    }
    else
    {
        //暂停并删除
        pip->deletePipeLine();

        if(cameraPipe){
            cameraPipe->setExit(true);
            cameraPipe->resetSource();
        }
        this->ui->start->setText("开启摄像");
        qDebug()<<"清除完毕";
        pip->safeDeletePipeline();
        this->ui->displayLabel->clear();
        return;
    }
    channelEnable();
}


void PupilDetect::on_stop_clicked()
{
    pip->pausePipeLine();
}


void PupilDetect::on_start3_clicked()
{
    pip->resumePipeLine();

}


void PupilDetect::on_DarknessPushButton_clicked()
{

}

