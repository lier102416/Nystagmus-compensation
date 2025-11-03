#include "fixationtest.h"
#include "ui_fixationtest.h"


fixationTest::fixationTest(QWidget *parent)
    :QWidget(parent),
    ui(new Ui::fixationTest)
{
    ui->setupUi(this);
    connect(this, &fixationTest::sendGazePoint, ui->widget_2, &FixationTest::getGazePoint);
    connect(ui->widget_2->mergedPip, &MergedProcessingPip::processingComplete, this, &fixationTest::processVideoFrame);
    scanCreamDevice();
}

fixationTest::~fixationTest()
{
    delete ui;
}

void fixationTest::scanCreamDevice()
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
void fixationTest::processVideoFrame(int frameId,bool success)
{
    QEtimer.start();
    FrameData frameData;
    cv::Mat rgbImage;
    int currentId = SharedPipelineData::getCurrentFrameId();

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

        // 绘制瞳孔
        visualizePupilDetection(rgbImage, frameData.pupilCircle);


        // 绘制光斑
        for (const auto& point : frameData.lightPoints) {
            cv::circle(rgbImage, point.center, 3, cv::Scalar(0, 0, 255), -1);
        }

        // 如果数据完整
        if (success) {
            std::vector<cv::Point2f> originalGazePoint = calculateGazePoint(
                frameData.lightPoints[0].center,  // 右上
                frameData.lightPoints[1].center,  // 左上
                frameData.lightPoints[2].center,  // 左下
                frameData.lightPoints[3].center,  // 右下
                frameData.pupilCircle.center
                );
            if(originalGazePoint.empty()){
                qWarning()<<"计算注视点失败";
                frameData.resultFlag = 0;
                return;
            }

            cv::Point2f unifiedGazePoint = calculateGazePointWithCombinedModel(
                frameData.lightPoints[0].center, frameData.lightPoints[1].center,
                frameData.lightPoints[2].center, frameData.lightPoints[3].center,
                frameData.pupilCircle.center
                );

            // // 发送绘制信号
             sendGazePoint(originalGazePoint);

            // 屏幕上的注视点，作为对比
            cv::Point2f gazePoint(ui->widget_2->pt2.x, ui->widget_2->pt2.y);
            const double DistanceThreshold = 5.0;
            qDebug()<<"屏幕注视点"<<ui->widget_2->pt2.x<<ui->widget_2->pt2.y;
            qDebug()<<"计算注视点1"<<originalGazePoint[0].x<<originalGazePoint[0].y;
            qDebug()<<"计算注视点2"<<originalGazePoint[1].x<<originalGazePoint[1].y;
            qDebug()<<"计算注视点3"<<originalGazePoint[2].x<<originalGazePoint[2].y;
            qDebug()<<"计算注视点4"<<originalGazePoint[3].x<<originalGazePoint[3].y;

            if(!originalGazePoint.empty() && originalGazePoint.size() >= 4){
                double dist0 = sqrt(pow(gazePoint.x - originalGazePoint[0].x, 2) +
                                    pow(gazePoint.y - originalGazePoint[0].y, 2));
                double dist1 = sqrt(pow(gazePoint.x - originalGazePoint[1].x, 2) +
                                    pow(gazePoint.y - originalGazePoint[1].y, 2));
                double dist2 = sqrt(pow(gazePoint.x - originalGazePoint[2].x, 2) +
                                    pow(gazePoint.y - originalGazePoint[2].y, 2));
                double dist3 = sqrt(pow(gazePoint.x - originalGazePoint[3].x, 2) +
                                    pow(gazePoint.y - originalGazePoint[3].y, 2));

                double minDistance = std::min({dist0, dist1, dist2, dist3});

                if(minDistance <= DistanceThreshold){
                    static int cnt = 0;  // 注意：这里使用静态变量保持计数
                    cnt++;
                    ui->textEdit->append(QString("注视点偏离合适：: distance=%1").arg(minDistance));
                    if(cnt == 10){
                        cnt = 0;
                        ui->widget_2->nextROI();
                    }
                }
                else {
                    ui->textEdit->append(QString("注视点偏离过大：: distance=%1").arg(minDistance));
                }
            }


            ui->widget_2->Calculate_light1.push_back(frameData.lightPoints[0].center); // 上面
            ui->widget_2->Calculate_light2.push_back(frameData.lightPoints[1].center); // 右上
            ui->widget_2->Calculate_light3.push_back(frameData.lightPoints[2].center); // 左下
            ui->widget_2->Calculate_light4.push_back(frameData.lightPoints[3].center); // 右下
            ui->widget_2->Calculate_pupil.push_back(frameData.pupilCircle.center);

            //saveSourceImageToLocal(rgbImage, frameId, "fixationtest");

        }
        else{
             frameData.resultFlag= 0;
            qDebug()<<"数据不完整，放弃存入";
        }

        QImage Qimg(rgbImage.data, rgbImage.cols, rgbImage.rows,
                    rgbImage.step, QImage::Format_RGB888);
        ui->displayLabel->setPixmap(QPixmap::fromImage(Qimg).scaled(
            ui->displayLabel->size(),Qt::KeepAspectRatioByExpanding,
            Qt::SmoothTransformation));

        qint64 ns = QEtimer.nsecsElapsed();       // 返回纳秒
        double ms = ns / 1e6;                    // 转换为毫秒，带小数

        // qDebug()<<"处理时间："<<frameData.frameId<<frameData.capTime<<frameData.rolTime<<frameData.spotTime<<frameData.pupilTime<<ms;

    }
}


void fixationTest:: acceptanceCoefficient(const std::vector<MappingCoefficients> & coefficients, const MappingCoefficients & coefficient){
    if(coefficients.size() == 0){
        // 使用预定义的默认映射系数
        initializeDefaultMappingCoefficients();

        // 如果传入的组合系数也为空，使用默认组合系数
        if(coefficient.xCoeff.empty() && coefficient.yCoeff.empty()) {
            combinedMappingCoefficients = m_mappingCoefficients[0]; // 使用第一组作为默认
        } else {
            combinedMappingCoefficients = coefficient;
        }

        qDebug() << "使用默认映射系数配置";
    } else {
        m_mappingCoefficients = coefficients;
        combinedMappingCoefficients = coefficient;
        qDebug() << "使用传入的映射系数配置";
    }

    printceCoefficient(m_mappingCoefficients, combinedMappingCoefficients);
}

// 辅助函数：初始化默认映射系数
void fixationTest::initializeDefaultMappingCoefficients() {
    m_mappingCoefficients.clear();
    m_mappingCoefficients.resize(4);

    // 可以从配置文件或预设值中加载
    static const std::vector<std::vector<float>> defaultXCoeffs = {
        {236.574875f, 12.459167f, -1.110212f, -0.052689f, 0.000403f, -0.029463f, 0.001294f, -0.000007f},
        {697.615479f, 10.136406f, -0.659631f, -0.001990f, 0.000454f, 0.041473f, 0.000447f, -0.000007f},
        {726.269653f, 8.985279f, -0.656963f, -0.015915f, 0.000704f, 0.033213f, 0.000384f, -0.000007f},
        {295.393463f, 13.015799f, -1.058814f, -0.088046f, 0.000639f, -0.022954f, 0.001079f, -0.000007f}
    };
    static const std::vector<std::vector<float>> defaultYCoeffs = {
        {1171.261108f, -0.606877f, -11.946161f, -0.006476f, -0.019261f, 0.002177f, -0.000119f},
        {1123.675415f, -1.167611f, -11.971226f, -0.006496f, -0.020796f, -0.013616f, -0.000249f},
        {1799.309204f, -0.852376f, -15.101971f, -0.012155f, 0.009181f, -0.007970f, -0.000023f},
        {1885.803833f, 0.514598f, -16.293446f, -0.020861f, 0.017816f, -0.012899f, 0.000146f}
    };

    for(int i = 0; i < 4; i++) {
        m_mappingCoefficients[i].xCoeff = defaultXCoeffs[i];
        m_mappingCoefficients[i].yCoeff = defaultYCoeffs[i];
    }
}
void fixationTest::printceCoefficient(const std::vector<MappingCoefficients> &coeffs, const MappingCoefficients &coeff)
{
    qDebug() << "  X系数 (共" << coeff.xCoeff.size() << "个):";
    for (size_t j = 0; j < coeff.xCoeff.size(); ++j) {
        qDebug() << "    a" << j << ":" << coeff.xCoeff[j];
    }
    qDebug() << "  Y系数 (共" << coeff.yCoeff.size() << "个):";
    for (size_t j = 0; j < coeff.yCoeff.size(); ++j) {
        qDebug() << "    b" << j << ":" << coeff.yCoeff[j];
    }

    qDebug() << "映射系数数量:" << coeffs.size();

    for (size_t i = 0; i < coeffs.size(); ++i) {
        qDebug() << "映射系数组 #" << i + 1;

        qDebug() << "  X系数 (共" << coeffs[i].xCoeff.size() << "个):";
        for (size_t j = 0; j < coeffs[i].xCoeff.size(); ++j) {
            qDebug() << "    a" << j << ":" << coeffs[i].xCoeff[j];
        }

        qDebug() << "  Y系数 (共" << coeffs[i].yCoeff.size() << "个):";
        for (size_t j = 0; j < coeffs[i].yCoeff.size(); ++j) {
            qDebug() << "    b" << j << ":" << coeffs[i].yCoeff[j];
        }
    }
}

// 修改函数定义，让参数顺序与lights数组顺序一致
std::vector<cv::Point2f> fixationTest::calculateGazePoint(
    const cv::Point &rightTop,    // 右上光斑 lights[0]
    const cv::Point &leftTop,     // 左上光斑 lights[1]
    const cv::Point &leftBottom,  // 左下光斑 lights[2]
    const cv::Point &rightBottom, // 右下光斑 lights[3]
    const cv::Point &pupil)       // 瞳孔中心
{
    // 定义屏幕边界
    const float MAX_X = 1920.0f;
    const float MAX_Y = 1080.0f;
    const float MIN_X = 0.0f;
    const float MIN_Y = 0.0f;

    // 确保有四组映射系数
    if (m_mappingCoefficients.size() < 4) {
        return std::vector<cv::Point2f>();
    }

    // 存储四个光斑计算出的注视点
    std::vector<cv::Point2f> gazePoints;
    gazePoints.reserve(4);

    const cv::Point lights[4] = {rightTop, leftTop, leftBottom, rightBottom};

    // 分别计算四个光斑的注视点
    for (int group = 0; group < 4; ++group) {
        // 检查当前组的系数是否足够
        if (group >= m_mappingCoefficients.size()) {
            qWarning() << "组索引超出范围:" << group;
            return std::vector<cv::Point2f>();
        }
        if (m_mappingCoefficients[group].xCoeff.size() < 8) {
            qWarning() << "组" << group << "的X系数不足，需要8个，当前:" << m_mappingCoefficients[group].xCoeff.size();
            return std::vector<cv::Point2f>();
        }
        if (m_mappingCoefficients[group].yCoeff.size() < 7) {
            qWarning() << "组" << group << "的Y系数不足，需要7个，当前:" << m_mappingCoefficients[group].yCoeff.size();
            return std::vector<cv::Point2f>();
        }

        // 获取当前光斑
        const cv::Point& light = lights[group];

        // 计算相对偏移量
        float dx = light.x - pupil.x;
        float dy = light.y - pupil.y;

        // 计算注视点坐标
        float gazeX = m_mappingCoefficients[group].xCoeff[0];
        gazeX += m_mappingCoefficients[group].xCoeff[1] * dx;
        gazeX += m_mappingCoefficients[group].xCoeff[2] * dy;
        gazeX += m_mappingCoefficients[group].xCoeff[3] * dx * dx;
        gazeX += m_mappingCoefficients[group].xCoeff[4] * dx * dx * dx;
        gazeX += m_mappingCoefficients[group].xCoeff[5] * dx * dy;
        gazeX += m_mappingCoefficients[group].xCoeff[6] * dx * dx * dy;
        gazeX += m_mappingCoefficients[group].xCoeff[7] * dx * dx * dx * dy;

        float gazeY = m_mappingCoefficients[group].yCoeff[0];
        gazeY += m_mappingCoefficients[group].yCoeff[1] * dx;
        gazeY += m_mappingCoefficients[group].yCoeff[2] * dy;
        gazeY += m_mappingCoefficients[group].yCoeff[3] * dx * dx;
        gazeY += m_mappingCoefficients[group].yCoeff[4] * dy * dy;
        gazeY += m_mappingCoefficients[group].yCoeff[5] * dx * dy;
        gazeY += m_mappingCoefficients[group].yCoeff[6] * dx * dx * dy;

        // 应用边界限制
        gazeX = std::max(MIN_X, std::min(MAX_X, gazeX));
        gazeY = std::max(MIN_Y, std::min(MAX_Y, gazeY));

        // 可选：输出边界限制前后的值进行调试
        // qDebug() << "组" << group << "原始注视点:" << gazeX << "," << gazeY;

        gazePoints.push_back(cv::Point2f(gazeX, gazeY));
    }



    return gazePoints;
}


cv::Point2f fixationTest::calculateGazePointWithCombinedModel(const cv::Point& pupil,
                                                              const cv::Point& light1,
                                                              const cv::Point& light2,
                                                              const cv::Point& light3,
                                                              const cv::Point& light4)
{
    // 计算相对偏移量
    float dx1 = light1.x - pupil.x;
    float dy1 = light1.y - pupil.y;
    float dx2 = light2.x - pupil.x;
    float dy2 = light2.y - pupil.y;
    float dx3 = light3.x - pupil.x;
    float dy3 = light3.y - pupil.y;
    float dx4 = light4.x - pupil.x;
    float dy4 = light4.y - pupil.y;

    // 计算X方向特征
    std::vector<float> xFeatures(16);
    int idx = 0;

    // 常数项
    xFeatures[idx++] = 1.0f;

    // 一阶项
    xFeatures[idx++] = dx1;
    xFeatures[idx++] = dy1;
    xFeatures[idx++] = dx2;
    xFeatures[idx++] = dy2;
    xFeatures[idx++] = dx3;
    xFeatures[idx++] = dy3;
    xFeatures[idx++] = dx4;
    xFeatures[idx++] = dy4;

    // 二阶交叉项
    xFeatures[idx++] = dx1 * dy1;
    xFeatures[idx++] = dx2 * dy2;
    xFeatures[idx++] = dx3 * dy3;
    xFeatures[idx++] = dx4 * dy4;

    // 光斑间的关系项
    xFeatures[idx++] = (dx1 - dx2) * (dx1 - dx2) + (dy1 - dy2) * (dy1 - dy2);
    xFeatures[idx++] = (dx3 - dx4) * (dx3 - dx4) + (dy3 - dy4) * (dy3 - dy4);
    xFeatures[idx++] = (dx1 + dx2 + dx3 + dx4) / 4.0f;

    // 计算Y方向特征
    std::vector<float> yFeatures(16);
    idx = 0;

    // 常数项
    yFeatures[idx++] = 1.0f;

    // 一阶项
    yFeatures[idx++] = dx1;
    yFeatures[idx++] = dy1;
    yFeatures[idx++] = dx2;
    yFeatures[idx++] = dy2;
    yFeatures[idx++] = dx3;
    yFeatures[idx++] = dy3;
    yFeatures[idx++] = dx4;
    yFeatures[idx++] = dy4;

    // 二阶项
    yFeatures[idx++] = dy1 * dy1;
    yFeatures[idx++] = dy2 * dy2;
    yFeatures[idx++] = dy3 * dy3;
    yFeatures[idx++] = dy4 * dy4;

    // 光斑间的关系项
    yFeatures[idx++] = (dx1 - dx3) * (dx1 - dx3) + (dy1 - dy3) * (dy1 - dy3);
    yFeatures[idx++] = (dx2 - dx4) * (dx2 - dx4) + (dy2 - dy4) * (dy2 - dy4);
    yFeatures[idx++] = (dy1 + dy2 + dy3 + dy4) / 4.0f;

    // 利用模型系数计算注视点
    float gazeX = 0.0f;
    float gazeY = 0.0f;

    for(int i = 0; i < combinedMappingCoefficients.xCoeff.size(); ++i) {
        gazeX += combinedMappingCoefficients.xCoeff[i] * xFeatures[i];
    }

    for(int i = 0; i < combinedMappingCoefficients.yCoeff.size(); ++i) {
        gazeY += combinedMappingCoefficients.yCoeff[i] * yFeatures[i];
    }

    return cv::Point2f(gazeX, gazeY);
}


void fixationTest::on_pushButton_start_clicked()
{
    cameraFlag = !cameraFlag;
    this->ui->widget_2->startTest();
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
            ui->widget_2->cameraPipe->setSource(1, filePath);
        } else {
            // 选择的是摄像头
            QCameraDevice selectedCamera = selectedItemData.value<QCameraDevice>();
            qDebug() << "selectedCamera:" << selectedCamera.description();
            if (selectedCamera.isNull()) {
                qWarning() << "选择摄像头无效";
                return;
            }
            ui->widget_2->cameraPipe->setSource(0, selectedCamera.description());
        }

        ui->widget_2->pip->creat_capturepip(ui->widget_2->cameraPipe, false);
        ui->widget_2->pip->add_process_modles(ui->widget_2->mergedPip);

        ui->widget_2->pip->createPipeLine();
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


void fixationTest::on_pushButton_over_clicked()
{
    ui->widget_2->cameraPipe->closeCamera();
    ui->widget_2->pip->safeDeletePipeline();
    ui->displayLabel->clear();
}

void fixationTest::on_pushButton_last_clicked()
{
    ui->widget_2->nextROI();
    qDebug()<<"next";
}



FixationTest::~FixationTest()
{
    delete pip;
    delete cameraPipe;
    delete mergedPip;
}

FixationTest::FixationTest(QWidget *parent)
    :QWidget(parent)
{

    // timer = new QTimer(this);       //通过定时器定时对图像进行更新
    // // 设置定时器单次触发
    // timer->setSingleShot(true);
    // connect(timer, &QTimer::timeout, this, &FixationTest::updateWidget);

    initRectROI(m_RectROIs, stepX, stepY); //对64个区域进行初始化
    m_RoiIndex.resize(15*9,0);//改变容器大小改变为64，值为0
    for (int i = 0; i < 15*9; ++i) {
        m_RoiIndex[i] = i;
    }

    cameraPipe = new videoCapturePip;
    mergedPip  = new MergedProcessingPip;

    //使用 Fisher-Yates 洗牌算法将数组元素顺序随机打乱
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(m_RoiIndex.begin(), m_RoiIndex.end(), gen);

}


void FixationTest::startTest(){
    m_TestIndex = 0;
    m_RectROI = &m_RectROIs[m_RoiIndex[m_TestIndex]];
    start = false;
}


void FixationTest::paintEvent(QPaintEvent *event)
{

    Q_UNUSED(event);

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

    // 创建一个新的QPainter对象，用于在窗口上绘制最终图像
    QPainter finalPainter(this);
    // 在窗口上绘制之前绘制在QImage上的图像
    finalPainter.drawImage(0, 0, image);
}


template <int N>
void FixationTest::initRectROI(RectROI (&RectROIs)[N] , float w, float h) {
    for (int i = 0; i < 9; ++i) {
        for(int j = 0; j < 15; ++j)
        {
            RectROIs[i * 8 + j].row = i; //表示该区域在低级
            RectROIs[i * 8 + j].col = j;
            RectROIs[i * 8 + j].width = w;
            RectROIs[i * 8 + j].height = h;
        }
    }
}

void FixationTest::getGazePoint(std::vector<cv::Point2f> gaze){
    Gaze = gaze;
    repaint();
}

void FixationTest::paintROITest(QPainter &painter)
{
    qDebug()<<"updateWidget";

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
        //绿点为注视点
        if(!Gaze.empty() && Gaze.size() >= 4){
            painter.setPen( QPen(Qt::green, 10, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPoint(Gaze[0].x,Gaze[0].y);
            painter.setPen( QPen(Qt::gray, 10, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPoint(Gaze[1].x,Gaze[1].y);
            painter.setPen( QPen(Qt::yellow, 10, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPoint(Gaze[2].x,Gaze[2].y);
            painter.setPen( QPen(Qt::red, 10, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPoint(Gaze[3].x,Gaze[3].y);
        }
        else{
            qDebug()<<"Gaze的大小有问题"<<Gaze.size();
        }
        pt2.x = x0+stepX/2;
        pt2.y = y0+stepY/2;

        fixationSet.push_back(pt2);
    }

}

void FixationTest::nextROI(){
    m_RectROI = &m_RectROIs[m_RoiIndex[++m_TestIndex]];
    repaint();
}
void FixationTest::updateWidget()
{

    qDebug()<<"updateWidget";
    update();

}




