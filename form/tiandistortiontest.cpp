#include "tiandistortiontest.h"
#include "ui_tiandistortiontest.h"
#include <QThread>
#include <QPainterPath>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include <QPushButton>
#include <QMetaType>
#include <QFileDialog>
Q_DECLARE_METATYPE(cv::Point)
Q_DECLARE_METATYPE(const cv::Point&)

tiandistortiontest::tiandistortiontest(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::tiandistortiontest)
{
    ui->setupUi(this);
    connect(ui->widget, &TianDistortionTest::countReached29, this, &tiandistortiontest::stopWidget);
    connect(ui->widget->mergedPip, &MergedProcessingPip::processingComplete, this, &tiandistortiontest::processVideoFrame);

    qRegisterMetaType<cv::Point>("cv::Point");
    qRegisterMetaType<const cv::Point&>("const cv::Point&");
    scanCreamDevice();
}




tiandistortiontest::~tiandistortiontest()
{
    delete ui;
}

void tiandistortiontest::scanCreamDevice()
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

}


void tiandistortiontest::on_pushButton_start_clicked()
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
            ui->widget->cameraPipe->setSource(1, filePath);
        } else {
            // 选择的是摄像头
            QCameraDevice selectedCamera = selectedItemData.value<QCameraDevice>();
            qDebug() << "selectedCamera:" << selectedCamera.description();
            if (selectedCamera.isNull()) {
                qWarning() << "选择摄像头无效";
                return;
            }
            int cameraIndex = index;
            ui->widget->cameraPipe->setSource(0, selectedCamera.description());
        }
        ui->widget->cameraPipe->setPaused(true);

        ui->widget->pip->creat_capturepip(ui->widget->cameraPipe, true);
        ui->widget->pip->add_process_modles(ui->widget->mergedPip);

        ui->widget->ImageSave.setImageBufferEnable(true);
        ui->widget->pip->createPipeLine();

        this->ui->pushButton_start->setText("关闭摄像师");
        return;
    }
    else
    {
        this->ui->displayLabel->clear();
        this->ui->pushButton_start->setText("开启摄像");
        return;
    }
}


void tiandistortiontest::on_pushButton_lase_clicked()
{
    ui->widget->onButton1Clicked();

}

void tiandistortiontest::stopWidget()
{
    ui->widget->start = 1;
    disconnect(ui->widget, nullptr, this, nullptr);
    // 停止定时器和线程（如果在运行）
    if (ui->widget->timer) {
        ui->widget->timer->stop();
    }
    m_mappingCoefficients = ui->widget->mappingCoefficients;
    m_combinedMappingCoefficients =  ui->widget->combinedMappingCoefficients;
    qDebug() << "映射系数已复制，共" << m_mappingCoefficients.size() << "组系数";
}



void tiandistortiontest::processVideoFrame(int frameId, bool success) {
    FrameData frameData;
    cv::Mat rgbImage;
    qDebug() << "processVideoFrame" << frameId;

    if (!SharedPipelineData::getFrameData(frameId, frameData)) {
        qWarning() << "无法获取帧数据:" << frameId;
        return;
    }

    cv::Mat resultImage = frameData.originalImage.clone();

    if (resultImage.empty()) {
        qWarning() << "图像为空:" << frameId;
        return;
    }

    // 安全的图像转换
    if (resultImage.channels() < 3) {
        cv::cvtColor(resultImage, rgbImage, cv::COLOR_GRAY2BGR);
    } else {
        cv::cvtColor(resultImage, rgbImage, cv::COLOR_BGR2RGB);
    }

    // 绘制瞳孔
    visualizePupilDetection(rgbImage, frameData.pupilCircle);

    // 安全检查光斑数量
    if (frameData.lightPoints.size() < 4) {
        qWarning() << "光斑数量不足，frameId:" << frameId
                   << "数量:" << frameData.lightPoints.size();
        frameData.resultFlag = false;
    } else {
        // 绘制光斑
        for (const auto& spot : frameData.lightPoints) {
            cv::circle(rgbImage, spot.center, spot.radius, cv::Scalar(0, 0, 255), 2);
        }
    }

    // 线程安全的数据更新
    if (success) {
        QMutexLocker locker(&m_dataMutex);  // 自动加锁/解锁

        try {
            ui->widget->Calculate_light1.push_back(frameData.lightPoints[0].center);
            ui->widget->Calculate_light2.push_back(frameData.lightPoints[1].center);
            ui->widget->Calculate_light3.push_back(frameData.lightPoints[2].center);
            ui->widget->Calculate_light4.push_back(frameData.lightPoints[3].center);
            ui->widget->Calculate_pupil.push_back(frameData.pupilCircle.center);

            ui->textEdit->append(QString("光斑1：x = %1 , y = %2 光斑2：x = %3 , y = %4 光斑3：x = %5 , y = %6 光斑4：x = %7 , y = %8 瞳孔：x = %9 , y = %10")
                                     .arg(frameData.lightPoints[0].center.x).arg(frameData.lightPoints[0].center.y)
                                     .arg(frameData.lightPoints[1].center.x).arg(frameData.lightPoints[1].center.y)
                                     .arg(frameData.lightPoints[2].center.x).arg(frameData.lightPoints[2].center.y)
                                     .arg(frameData.lightPoints[3].center.x).arg(frameData.lightPoints[3].center.y)
                                     .arg(frameData.pupilCircle.center.x).arg(frameData.pupilCircle.center.y));
        } catch (const std::exception& e) {
            qCritical() << "数据存储异常:" << e.what();
            frameData.resultFlag = false;
        }
    } else {
        qDebug() << "数据不完整，放弃存入. frameId:" << frameId;
        ui->widget->ImageSave.addOriginalImageToBuffer(resultImage, frameId);
        ui->widget->ImageSave.addDisplayImageToBuffer(rgbImage, frameId);

    }

    // 安全的图像显示
    try {
        QImage Qimg(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
        ui->displayLabel->setPixmap(QPixmap::fromImage(Qimg).scaled(
            ui->displayLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } catch (const std::exception& e) {
        qCritical() << "图像显示异常:" << e.what();
    }
}

template <int N>
void TianDistortionTest::initRectROI(RectROI (&RectROIs)[N] , float w, float h) {
    for (int i = 0; i < 9; ++i) {
        for(int j = 0; j < 15; ++j)
        {
            RectROIs[i * 8 + j].row = i; //表示该区域在低级
            RectROIs[i * 8 + j].col = j;
            RectROIs[i * 8 + j].width = w;
            RectROIs[i * 8 + j].height = h;
            RectROIs[i * 8 + j].isDistortional = false;
        }
    }
}



TianDistortionTest::TianDistortionTest(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);  // ← 添加这行

    initRectROI(m_RectROIs, stepX, stepY); //对64个区域进行初始化
    timer = new QTimer(this);       //通过定时器定时对图像进行更新
    // 设置定时器单次触发
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, &TianDistortionTest::updateWidget);

    m_RoiIndex.resize(15*9,0);//改变容器大小改变为64，值为0
    for (int i = 0; i < 15*9; ++i) {
        m_RoiIndex[i] = i;
    }

    pip = new pipline;
    cameraPipe = new videoCapturePip;
    mergedPip  = new MergedProcessingPip;
}

TianDistortionTest::~TianDistortionTest()
{
    delete pip;
    delete cameraPipe;

}


void TianDistortionTest::onButton1Clicked() {
    // 处理按钮点击事件，切换变量值
    setshow = true;
    count ++;
    qDebug()<<"count:"<<count;
    nextLineOrROI();
}


void TianDistortionTest::mappingCalculation()
{
    // 参数配置
    const int GROUP_COUNT = 4;      // 四个光斑：lightRol_1、lightRol_2、lightRol_3和lightRol_4
    const int COEFF_X_COUNT = 8;    // x系数数量 a0-a7
    const int COEFF_Y_COUNT = 7;    // y系数数量 b0-b6


    // 获取可用数据点数量
    int count = std::min({lightRol_1.size(), lightRol_2.size(), lightRol_3.size(), lightRol_4.size(),
                          pupilRol.size(), fixationSet.size()});

    // 在计算前添加调试代码，检查各光斑数据
    for(int i = 0; i < count; ++i) {
        qDebug() << "点" << i << ": 瞳孔=(" << pupilRol[i].x << "," << pupilRol[i].y << ")"
                 << "光斑1=(" << lightRol_1[i].x << "," << lightRol_1[i].y << ")"
                 << "光斑2=(" << lightRol_2[i].x << "," << lightRol_2[i].y << ")"
                 << "光斑3=(" << lightRol_3[i].x << "," << lightRol_3[i].y << ")"
                 << "光斑4=(" << lightRol_4[i].x << "," << lightRol_4[i].y << ")"
                 << "注视点=(" << fixationSet[i].x << "," << fixationSet[i].y << ")";
    }
    // 数据校验
    if(count < 18) {
        qWarning() << "数据不足，需要至少18组数据点，当前只有" << count << "组";
        return;
    }

    // 使用前18组数据（或者你可以选择其他的18组）
    const int DATA_POINTS = std::min(18, count);

    // 初始化存储结构
    mappingCoefficients.clear();
    mappingCoefficients.resize(GROUP_COUNT);

    // 为四组光斑分别计算映射函数
    for(int group = 0; group < GROUP_COUNT; ++group)
    {
        // 准备矩阵（x和y使用不同特征维度）
        cv::Mat Ax(DATA_POINTS, COEFF_X_COUNT, CV_32F);
        cv::Mat Ay(DATA_POINTS, COEFF_Y_COUNT, CV_32F);
        cv::Mat bx(DATA_POINTS, 1, CV_32F);
        cv::Mat by(DATA_POINTS, 1, CV_32F);

        // 填充数据矩阵
        for(int i = 0; i < DATA_POINTS; ++i)
        {
            // 获取当前数据点
            const cv::Point& pupil = pupilRol[i];

            // 根据组号选择对应的光斑点
            const cv::Point& light = (group == 0) ? lightRol_1[i] :
                                         (group == 1) ? lightRol_2[i] :
                                         (group == 2) ? lightRol_3[i] : lightRol_4[i];

            const cv::Point2f& fixation = fixationSet[i];

            // 计算相对偏移量
            float dx = light.x - pupil.x;
            float dy = light.y - pupil.y;

            // 构建x多项式特征（8项）
            float* row_x = Ax.ptr<float>(i);
            row_x[0] = 1.0f;              // a0
            row_x[1] = dx;                // a1*dx
            row_x[2] = dy;                // a2*dy
            row_x[3] = dx * dx;           // a3*dx²
            row_x[4] = dx * dx * dx;      // a4*dx³
            row_x[5] = dx * dy;           // a5*dxdy
            row_x[6] = dx * dx * dy;      // a6*dx²dy
            row_x[7] = dx * dx * dx * dy; // a7*dx³dy

            // 构建y多项式特征（7项）
            float* row_y = Ay.ptr<float>(i);
            row_y[0] = 1.0f;         // b0
            row_y[1] = dx;           // b1*dx
            row_y[2] = dy;           // b2*dy
            row_y[3] = dx * dx;      // b3*dx²
            row_y[4] = dy * dy;      // b4*dy²
            row_y[5] = dx * dy;      // b5*dxdy
            row_y[6] = dx * dx * dy; // b6*dx²dy

            // 目标值
            bx.at<float>(i) = fixation.x;
            by.at<float>(i) = fixation.y;
        }

        // 解线性方程组（分别求解x/y方向）
        cv::Mat xCoeff, yCoeff;
        cv::solve(Ax, bx, xCoeff, cv::DECOMP_SVD);
        cv::solve(Ay, by, yCoeff, cv::DECOMP_SVD);

        // 存储系数
        mappingCoefficients[group].xCoeff.resize(COEFF_X_COUNT);
        mappingCoefficients[group].yCoeff.resize(COEFF_Y_COUNT);

        for(int j = 0; j < COEFF_X_COUNT; ++j) {
            mappingCoefficients[group].xCoeff[j] = xCoeff.at<float>(j);
        }

        for(int j = 0; j < COEFF_Y_COUNT; ++j) {
            mappingCoefficients[group].yCoeff[j] = yCoeff.at<float>(j);
        }

        // 调试输出
        qDebug() << "=== 光斑" << (group + 1) << "映射系数 ===";

        QString xCoeffs = "X系数: ";
        for(int j = 0; j < COEFF_X_COUNT; ++j) {
            xCoeffs += QString::number(mappingCoefficients[group].xCoeff[j], 'f', 6) + " ";
        }
        qDebug() << xCoeffs;

        QString yCoeffs = "Y系数: ";
        for(int j = 0; j < COEFF_Y_COUNT; ++j) {
            yCoeffs += QString::number(mappingCoefficients[group].yCoeff[j], 'f', 6) + " ";
        }
        qDebug() << yCoeffs;
    }
}

void TianDistortionTest::enhancedMappingCalculation()
{
    // 参数配置
    const int DATA_POINTS = std::min(18, count); // 使用至少18个校准点

    // 特征设计：结合所有四个光斑的信息
    const int COMBINED_X_FEATURES = 16; // X方向特征数
    const int COMBINED_Y_FEATURES = 16; // Y方向特征数

    // 初始化矩阵
    cv::Mat Ax(DATA_POINTS, COMBINED_X_FEATURES, CV_32F);
    cv::Mat Ay(DATA_POINTS, COMBINED_Y_FEATURES, CV_32F);
    cv::Mat bx(DATA_POINTS, 1, CV_32F);
    cv::Mat by(DATA_POINTS, 1, CV_32F);

    // 填充数据矩阵
    for(int i = 0; i < DATA_POINTS; ++i)
    {
        // 获取当前数据点
        const cv::Point& pupil = pupilRol[i];
        const cv::Point& light1 = lightRol_1[i];
        const cv::Point& light2 = lightRol_2[i];
        const cv::Point& light3 = lightRol_3[i];
        const cv::Point& light4 = lightRol_4[i];
        const cv::Point2f& fixation = fixationSet[i];

        // 计算相对偏移量
        float dx1 = light1.x - pupil.x;
        float dy1 = light1.y - pupil.y;
        float dx2 = light2.x - pupil.x;
        float dy2 = light2.y - pupil.y;
        float dx3 = light3.x - pupil.x;
        float dy3 = light3.y - pupil.y;
        float dx4 = light4.x - pupil.x;
        float dy4 = light4.y - pupil.y;

        // 填充X方向特征矩阵
        float* row_x = Ax.ptr<float>(i);
        int idx = 0;

        // 常数项
        row_x[idx++] = 1.0f;

        // 一阶项（各光斑的dx和dy）
        row_x[idx++] = dx1;
        row_x[idx++] = dy1;
        row_x[idx++] = dx2;
        row_x[idx++] = dy2;
        row_x[idx++] = dx3;
        row_x[idx++] = dy3;
        row_x[idx++] = dx4;
        row_x[idx++] = dy4;

        // 二阶交叉项（dx*dy组合）
        row_x[idx++] = dx1 * dy1;
        row_x[idx++] = dx2 * dy2;
        row_x[idx++] = dx3 * dy3;
        row_x[idx++] = dx4 * dy4;

        // 光斑间的关系项
        row_x[idx++] = (dx1 - dx2) * (dx1 - dx2) + (dy1 - dy2) * (dy1 - dy2); // 光斑1和2的距离平方
        row_x[idx++] = (dx3 - dx4) * (dx3 - dx4) + (dy3 - dy4) * (dy3 - dy4); // 光斑3和4的距离平方
        row_x[idx++] = (dx1 + dx2 + dx3 + dx4) / 4.0f; // 四个光斑dx的平均值

        // 填充Y方向特征矩阵（与X类似但可以设计不同的特征）
        float* row_y = Ay.ptr<float>(i);
        idx = 0;

        // 常数项
        row_y[idx++] = 1.0f;

        // 一阶项
        row_y[idx++] = dx1;
        row_y[idx++] = dy1;
        row_y[idx++] = dx2;
        row_y[idx++] = dy2;
        row_y[idx++] = dx3;
        row_y[idx++] = dy3;
        row_y[idx++] = dx4;
        row_y[idx++] = dy4;

        // 二阶项
        row_y[idx++] = dy1 * dy1;
        row_y[idx++] = dy2 * dy2;
        row_y[idx++] = dy3 * dy3;
        row_y[idx++] = dy4 * dy4;

        // 光斑间的关系项
        row_y[idx++] = (dx1 - dx3) * (dx1 - dx3) + (dy1 - dy3) * (dy1 - dy3); // 光斑1和3的距离平方
        row_y[idx++] = (dx2 - dx4) * (dx2 - dx4) + (dy2 - dy4) * (dy2 - dy4); // 光斑2和4的距离平方
        row_y[idx++] = (dy1 + dy2 + dy3 + dy4) / 4.0f; // 四个光斑dy的平均值

        // 目标值
        bx.at<float>(i) = fixation.x;
        by.at<float>(i) = fixation.y;
    }

    // 解线性方程组
    cv::Mat xCoeff, yCoeff;
    cv::solve(Ax, bx, xCoeff, cv::DECOMP_SVD);
    cv::solve(Ay, by, yCoeff, cv::DECOMP_SVD);

    // 存储统一模型的系数
    combinedMappingCoefficients.xCoeff.resize(COMBINED_X_FEATURES);
    combinedMappingCoefficients.yCoeff.resize(COMBINED_Y_FEATURES);

    for(int j = 0; j < COMBINED_X_FEATURES; ++j) {
        combinedMappingCoefficients.xCoeff[j] = xCoeff.at<float>(j);
    }

    for(int j = 0; j < COMBINED_Y_FEATURES; ++j) {
        combinedMappingCoefficients.yCoeff[j] = yCoeff.at<float>(j);
    }

    // 调试输出
    qDebug() << "=== 统一映射模型系数 ===";

    QString xCoeffs = "X系数: ";
    for(int j = 0; j < COMBINED_X_FEATURES; ++j) {
        xCoeffs += QString::number(combinedMappingCoefficients.xCoeff[j], 'f', 6) + " ";
    }
    qDebug() << xCoeffs;

    QString yCoeffs = "Y系数: ";
    for(int j = 0; j < COMBINED_Y_FEATURES; ++j) {
        yCoeffs += QString::number(combinedMappingCoefficients.yCoeff[j], 'f', 6) + " ";
    }
    qDebug() << yCoeffs;
}
static int paintROITestFlag = 0;
void TianDistortionTest::paintROITest(QPainter &painter)
{
    paintROITestFlag++;
    qDebug()<<"paintROITest"<<paintROITestFlag <<detectionFlag<<setshow;

    if(!start)
    {
        // 循环遍历8x8的矩阵，绘制矩形、编号和数字
        for(int row = 0; row < 9; row++)
        {
            for(int col = 0; col < 15; col++)
            {
                float x0 = col * stepX;
                float y0 = row * stepY;
                float w = stepX;
                float h = stepY;
                if(col == 0)
                {
                    x0 += 2.0;
                }
                if(row == 0)
                {
                    y0 += 2.0;
                }
                if(col == 7)
                {
                    w -= 2.0;
                }
                if(row == 7)
                {
                    h -= 2.0;
                }

                // 设置画笔、绘制矩形、设置字体和颜色、绘制数字
                painter.setPen( QPen(Qt::black, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawRect(x0 ,y0 ,w ,h );
                QFont font;
                font.setPixelSize(30); // 设置字体大小
                painter.setFont(font);
                painter.setPen(QPen(Qt::black));
            }
        }

        if(setshow && detectionFlag == false)
        {

            int col = m_RectROI->col;//行
            int row = m_RectROI->row;
            float x0 = col * stepX;
            float y0 = row * stepY;
            float w = stepX;
            float h = stepY;
            // 设置画刷为红色，填充圆形
            painter.setBrush(Qt::red);  // 设置画刷为红色
            painter.setPen(Qt::NoPen);  // 可选：设置无边框
            painter.drawEllipse(x0, y0, w, h);

            // 设置画笔，绘制一个红色的点，位置在圆心
            painter.setPen( QPen(Qt::blue, 10, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPoint(x0+stepX/2,y0+stepY/2);

            fixationSet.push_back(cv::Point2f(x0+stepX/2,y0+stepY/2));
            qDebug()<<"fixationSet"<<x0+stepX/2<<y0+stepY/2;
            pip->resumePipeLine();

            qDebug()<<"恢复";
            setshow = false;
            detectionFlag = true;
        }

        else if(setshow == false && detectionFlag == true)
        {
            qDebug()<<"暂停";
            AverageValueCalculation();
            pip->pausePipeLine();
            detectionFlag = false;
        }

    }

}

void TianDistortionTest::paintEvent(QPaintEvent *event)
{

    Q_UNUSED(event);

    if(start == true)
        return;
    // 创建一个QImage对象，大小为当前窗口的大小，使用ARGB32格式
    QImage image(size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);  // 填充图像的背景为透明
    // 创建QPainter，用于在QImage上进行绘制
    QPainter painter(&image);

    // 设置渲染提示，启用抗锯齿
    painter.setRenderHint(QPainter::Antialiasing);
    // 设置画笔为无边框
    painter.setPen(Qt::NoPen);
    // 设置画刷为白色，绘制一个白色的矩形，覆盖整个图像区域
    painter.setBrush(Qt::white);
    painter.drawRect(rect());
    //调用paintROITest函数绘制
    if(!m_ROITestFinish)
    {
        paintROITest(painter);
    }

    // 结束在QImage上的绘制
    painter.end();
    qDebug()<<"paintevent";
    // 创建一个新的QPainter对象，用于在窗口上绘制最终图像
    QPainter finalPainter(this);
    // 在窗口上绘制之前绘制在QImage上的图像
    finalPainter.drawImage(0, 0, image);
}

void TianDistortionTest::updateWidget()
{
    update();
}

void TianDistortionTest::startTest()
{
    m_TestIndex = 0;
    m_LineIndex = 0;

}


void TianDistortionTest::setShow(bool setshow)
{
    this->setshow = setshow;
    qDebug()<<"数值:"<<setshow;

}

void TianDistortionTest::AverageValueCalculation()
{
    //确保五个向量长度一致
    qDebug() << Calculate_pupil.size() << Calculate_light1.size() << Calculate_light2.size() << Calculate_light3.size() << Calculate_light4.size();
    int cnt = Calculate_pupil.size();
    std::cout << "cnt" << cnt << std::endl;
    if (Calculate_light1.size() != cnt || Calculate_light2.size() != cnt ||
        Calculate_light3.size() != cnt || Calculate_light4.size() != cnt) {
        // qDebug() << "警告：光斑点集和瞳孔点集数量不一致";
        // // 使用最小的数量作为计数
        cnt = std::min(cnt, (int)Calculate_light1.size());
        cnt = std::min(cnt, (int)Calculate_light2.size());
        cnt = std::min(cnt, (int)Calculate_light3.size());
        cnt = std::min(cnt, (int)Calculate_light4.size());
    }
    int currentCount = count;  // 或者您定义的其他计数变量

    // 如果数据点太少，直接使用所有点计算平均值
    if (cnt < 10) {
        qDebug() << "数据点过少" << cnt;
        CalculateSimpleAverage(cnt, currentCount);
        return;
    }
    int startIdx = 0;
    startIdx = static_cast<int>(cnt * 0.3);

    // 首先过滤掉零值点
    std::vector<MeasurementPoint> stablePoints;
    for (int i = startIdx; i < cnt; ++i) {
        // 添加非零点
        MeasurementPoint mp;
        mp.light1 = Calculate_light1[i];
        mp.light2 = Calculate_light2[i];
        mp.light3 = Calculate_light3[i];
        mp.light4 = Calculate_light4[i];
        mp.pupil = Calculate_pupil[i];
        stablePoints.push_back(mp);
    }

    // 如果过滤后点数太少，直接计算平均值
    if (stablePoints.size() < 10) {
        qDebug() << "过滤零值后数据点过少: " << stablePoints.size();
        CalculateSimpleAverage(cnt, currentCount);
        return;
    }

    // 计算各点的均值，用于后续方差计算
    cv::Point meanLight1 = CalculateMean(stablePoints, PointType::LIGHT1);
    cv::Point meanLight2 = CalculateMean(stablePoints, PointType::LIGHT2);
    cv::Point meanLight3 = CalculateMean(stablePoints, PointType::LIGHT3);
    cv::Point meanLight4 = CalculateMean(stablePoints, PointType::LIGHT4);
    cv::Point meanPupil = CalculateMean(stablePoints, PointType::PUPIL);

    // 计算各点的方差
    double varLight1 = CalculateVariance(stablePoints, meanLight1, PointType::LIGHT1);
    double varLight2 = CalculateVariance(stablePoints, meanLight2, PointType::LIGHT2);
    double varLight3 = CalculateVariance(stablePoints, meanLight3, PointType::LIGHT3);
    double varLight4 = CalculateVariance(stablePoints, meanLight4, PointType::LIGHT4);
    double varPupil = CalculateVariance(stablePoints, meanPupil, PointType::PUPIL);

    // 设置方差阈值，一般取2-3个标准差作为阈值
    const double thresholdMultiplier = 2.0; // 可以根据需要调整
    double thresholdLight1 = thresholdMultiplier * sqrt(varLight1);
    double thresholdLight2 = thresholdMultiplier * sqrt(varLight2);
    double thresholdLight3 = thresholdMultiplier * sqrt(varLight3);
    double thresholdLight4 = thresholdMultiplier * sqrt(varLight4);
    double thresholdPupil = thresholdMultiplier * sqrt(varPupil);

    // 根据方差筛选有效点
    std::vector<MeasurementPoint> validPoints;
    for (const auto& point : stablePoints) {
        // 计算各点距离均值的距离
        double distLight1 = CalculateDistance(point.light1, meanLight1);
        double distLight2 = CalculateDistance(point.light2, meanLight2);
        double distLight3 = CalculateDistance(point.light3, meanLight3);
        double distLight4 = CalculateDistance(point.light4, meanLight4);
        double distPupil = CalculateDistance(point.pupil, meanPupil);

        // 如果所有点的距离均在阈值内，则认为是有效点
        if (distLight1 <= thresholdLight1 &&
            distLight2 <= thresholdLight2 &&
            distLight3 <= thresholdLight3 &&
            distLight4 <= thresholdLight4 &&
            distPupil <= thresholdPupil) {
            validPoints.push_back(point);
        }
    }

    qDebug() << "筛选后有效点数: " << validPoints.size();

    // 如果有效点太少，则降低筛选标准或使用原始非零点
    if (validPoints.size() < 5) {
        qDebug() << "有效点数太少，使用所有非零点";
        CalculateAverageFromPoints(stablePoints, currentCount);
    } else {
        // 使用有效点计算最终平均值
        CalculateAverageFromPoints(validPoints, currentCount);
    }

    // 清空计算点集，准备下一轮收集
    Calculate_light1.clear();
    Calculate_light2.clear();
    Calculate_light3.clear();
    Calculate_light4.clear();
    Calculate_pupil.clear();
}

// 计算简单平均值的辅助函数
void TianDistortionTest::CalculateSimpleAverage(int cnt, int currentCount)
{
    cv::Point avgLightRol1(0, 0);
    cv::Point avgLightRol2(0, 0);
    cv::Point avgLightRol3(0, 0);
    cv::Point avgLightRol4(0, 0);
    cv::Point avgPupilRol(0, 0);
    std::vector<MeasurementPoint> validPoints;

    // 累加所有点的坐标
    for (int i = 0; i < cnt; ++i) {
        avgLightRol1.x += Calculate_light1[i].x;
        avgLightRol1.y += Calculate_light1[i].y;
        avgLightRol2.x += Calculate_light2[i].x;
        avgLightRol2.y += Calculate_light2[i].y;
        avgLightRol3.x += Calculate_light3[i].x;
        avgLightRol3.y += Calculate_light3[i].y;
        avgLightRol4.x += Calculate_light4[i].x;
        avgLightRol4.y += Calculate_light4[i].y;
        avgPupilRol.x += Calculate_pupil[i].x;
        avgPupilRol.y += Calculate_pupil[i].y;

        // 添加一个完整的测量点
        MeasurementPoint mp;
        mp.light1 = Calculate_light1[i];
        mp.light2 = Calculate_light2[i];
        mp.light3 = Calculate_light3[i];
        mp.light4 = Calculate_light4[i];
        mp.pupil = Calculate_pupil[i];
        validPoints.push_back(mp);
    }

    // 计算平均值
    avgLightRol1.x /= cnt;
    avgLightRol1.y /= cnt;
    avgLightRol2.x /= cnt;
    avgLightRol2.y /= cnt;
    avgLightRol3.x /= cnt;
    avgLightRol3.y /= cnt;
    avgLightRol4.x /= cnt;
    avgLightRol4.y /= cnt;
    avgPupilRol.x /= cnt;
    avgPupilRol.y /= cnt;

    // 确保collectingData有足够的空间
    if (collectingData.size() <= currentCount) {
        collectingData.resize(currentCount + 1);
    }

    // 保存当前批次的有效数据
    collectingData[currentCount] = validPoints;

    // 将计算结果存储在类成员变量中以供后续使用
    lightRol_1.push_back(avgLightRol1);
    lightRol_2.push_back(avgLightRol2);
    lightRol_3.push_back(avgLightRol3);
    lightRol_4.push_back(avgLightRol4);
    pupilRol.push_back(avgPupilRol);
}

// 从指定点集计算平均值的辅助函数
void TianDistortionTest::CalculateAverageFromPoints(const std::vector<MeasurementPoint>& points, int currentCount)
{
    cv::Point avgLightRol1(0, 0);
    cv::Point avgLightRol2(0, 0);
    cv::Point avgLightRol3(0, 0);
    cv::Point avgLightRol4(0, 0);
    cv::Point avgPupilRol(0, 0);

    int size = points.size();
    // 累加所有点的坐标
    for (const auto& point : points) {
        avgLightRol1.x += point.light1.x;
        avgLightRol1.y += point.light1.y;
        avgLightRol2.x += point.light2.x;
        avgLightRol2.y += point.light2.y;
        avgLightRol3.x += point.light3.x;
        avgLightRol3.y += point.light3.y;
        avgLightRol4.x += point.light4.x;
        avgLightRol4.y += point.light4.y;
        avgPupilRol.x += point.pupil.x;
        avgPupilRol.y += point.pupil.y;
    }

    // 计算平均值
    avgLightRol1.x /= size;
    avgLightRol1.y /= size;
    avgLightRol2.x /= size;
    avgLightRol2.y /= size;
    avgLightRol3.x /= size;
    avgLightRol3.y /= size;
    avgLightRol4.x /= size;
    avgLightRol4.y /= size;
    avgPupilRol.x /= size;
    avgPupilRol.y /= size;

    // 确保collectingData有足够的空间
    if (collectingData.size() <= currentCount) {
        collectingData.resize(currentCount + 1);
    }

    // 保存当前批次的有效数据
    collectingData[currentCount] = points;

    // 将计算结果存储在类成员变量中以供后续使用
    lightRol_1.push_back(avgLightRol1);
    lightRol_2.push_back(avgLightRol2);
    lightRol_3.push_back(avgLightRol3);
    lightRol_4.push_back(avgLightRol4);
    pupilRol.push_back(avgPupilRol);
}

// 计算点的均值
cv::Point TianDistortionTest::CalculateMean(const std::vector<MeasurementPoint>& points, PointType type)
{
    cv::Point mean(0, 0);
    int size = points.size();

    for (const auto& point : points) {
        cv::Point current;
        switch (type) {
        case PointType::LIGHT1: current = point.light1; break;
        case PointType::LIGHT2: current = point.light2; break;
        case PointType::LIGHT3: current = point.light3; break;
        case PointType::LIGHT4: current = point.light4; break;
        case PointType::PUPIL: current = point.pupil; break;
        }
        mean.x += current.x;
        mean.y += current.y;
    }

    mean.x /= size;
    mean.y /= size;
    return mean;
}

// 计算点的方差
double TianDistortionTest::CalculateVariance(const std::vector<MeasurementPoint>& points,
                                             const cv::Point& mean, PointType type)
{
    double variance = 0.0;
    int size = points.size();

    for (const auto& point : points) {
        cv::Point current;
        switch (type) {
        case PointType::LIGHT1: current = point.light1; break;
        case PointType::LIGHT2: current = point.light2; break;
        case PointType::LIGHT3: current = point.light3; break;
        case PointType::LIGHT4: current = point.light4; break;
        case PointType::PUPIL: current = point.pupil; break;
        }

        double dx = current.x - mean.x;
        double dy = current.y - mean.y;
        variance += (dx * dx + dy * dy);
    }

    return variance / size;
}

// 计算两点之间的欧氏距离
double TianDistortionTest::CalculateDistance(const cv::Point& p1, const cv::Point& p2)
{
    double dx = p1.x - p2.x;
    double dy = p1.y - p2.y;
    return sqrt(dx * dx + dy * dy);
}

void TianDistortionTest::SaveCollectingData()
{
    QString fileName = QDir::currentPath() + "/collected_check_data.csv";
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug()<<"无法写入";
        return;
    }
    QTextStream out(&file);
    // 写入CSV文件标题
    out << "count,pointIndex,LightSpot1x,LightSpot1y,LightSpot2x,LightSpot2y,LightSpot3x,LightSpot3y,LightSpot4x,LightSpot4y,pupilx,pupily\n";
    // 遍历所有count的数据
    for (size_t countIndex = 0; countIndex < collectingData.size(); ++countIndex) {
        const auto& measurementPoints = collectingData[countIndex];
        const auto& mpTeat = fixationSet[countIndex-1];
        // 遍历每个count下的所有测量点
        for (size_t pointIndex = 0; pointIndex < measurementPoints.size(); ++pointIndex) {
            const auto& mp = measurementPoints[pointIndex];
            // 写入一行数据: count, point_index, 光斑1x, 光斑1y, 光斑2x, 光斑2y, 光斑3x, 光斑3y, 光斑4x, 光斑4y, 瞳孔x, 瞳孔y
            out << countIndex << ","
                << pointIndex << ","
                << mp.light1.x << ","
                << mp.light1.y << ","
                << mp.light2.x << ","
                << mp.light2.y << ","
                << mp.light3.x << ","
                << mp.light3.y << ","
                << mp.light4.x << ","
                << mp.light4.y << ","
                << mp.pupil.x << ","
                << mp.pupil.y << "\n";
        }
        out << "average, ,LightSpot1x,LightSpot1y,LightSpot2x,LightSpot2y,LightSpot3x,LightSpot3y,LightSpot4x,LightSpot4y,pupilx,pupily,testx,testy\n";
        out << " " <<","
            << countIndex<<","
            << lightRol_1[countIndex].x <<","
            << lightRol_1[countIndex].y <<","
            << lightRol_2[countIndex].x <<","
            << lightRol_2[countIndex].y <<","
            << lightRol_3[countIndex].x <<","
            << lightRol_3[countIndex].y <<","
            << lightRol_4[countIndex].x <<","
            << lightRol_4[countIndex].y <<","
            << pupilRol[countIndex].x <<","
            << pupilRol[countIndex].y <<","
            << fixationSet[countIndex].x <<","
            << fixationSet[countIndex].y <<"\n";
    }
    file.close();
    qDebug()<<"保存成功";
}

void TianDistortionTest::nextLineOrROI() {
    m_TestIndex = 0;
    // 获取当前矩形区域
    m_RectROI = &m_RectROIs[m_RoiIndex[m_TestIndex]];
    m_LineIndex = 0;
    start = false;

    // 处理count为19的特殊情况
    if (count == 19) {
        SaveCollectingData();
        mappingCalculation();
        enhancedMappingCalculation();
        cameraPipe->closeCamera();
        ImageSave.saveOriginalBufferImage(this);
        ImageSave.saveDisplayBufferImage(this);

        pip->safeDeletePipeline();

        emit countReached29();
        return;
    }

    // 如果count不在有效范围内，直接返回
    if (count < 1 || count > 18) {
        return;
    }

    // count 1-18 对应9个5x3子网格，每个子网格选择2个位置
    int gridIndex = (count - 1) / 2;  // 0-8，表示第几个5x3子网格
    int posInGrid = (count - 1) % 2;  // 0或1，表示子网格内的第几个位置

    // 计算子网格在3x3布局中的位置
    int gridRow = gridIndex / 3;      // 子网格行 (0, 1, 2)
    int gridCol = gridIndex % 3;      // 子网格列 (0, 1, 2)

    // 每个5x3子网格的基础偏移
    int baseCol = gridCol * 5;        // 列偏移: 0, 5, 10
    int baseRow = gridRow * 3;        // 行偏移: 0, 3, 6

    // 在每个5x3子网格中选择固定位置
    // 为了更好体现边界效果，将原来的(2,2)和(4,2)向左上移动一行一列

    int relativeCol = (posInGrid == 0) ? 1 : 3;  // 从2,4改为1,3
    int relativeRow = 1;  // 从2改为1

    // 计算最终位置
    m_RectROI->col = baseCol + relativeCol;
    m_RectROI->row = baseRow + relativeRow;

    // 添加调试信息，确认选择的格子位置
    qDebug() << "Count:" << count
             << "Grid:" << gridIndex
             << "PosInGrid:" << posInGrid
             << "BaseCol:" << baseCol << "BaseRow:" << baseRow
             << "RelativeCol:" << relativeCol << "RelativeRow:" << relativeRow
             << "最终位置: col=" << m_RectROI->col
             << ", row=" << m_RectROI->row;

    repaint();
    if(count == 1)
        timer->start(2500);
    else
        timer->start(1500);
}
