#include "mergedprocessingpip.h"

MergedProcessingPip::MergedProcessingPip() :
    QObject(),
    AbstractPipe("MergedProcessingPipe", PIPE_PROCESS_E)
{
    // 初始化所有处理组件
    rolExtraction = new RolExtraction();
    spotExtraction = new SpotExtraction();
    pupilExtraction = new PupilEtraction();
    spotProcessor = new SmartSpotProcessor();

    // 初始化映射系数
    initializeDefaultMappingCoefficients();

    qDebug() << "MergedProcessingPip: 构造完成";
}

// === 🔧 析构函数 ===
MergedProcessingPip::~MergedProcessingPip() {
    delete rolExtraction;
    delete spotExtraction;
    delete pupilExtraction;
    delete spotProcessor;

    qDebug() << "MergedProcessingPip: 析构完成";
}

// ===  主管道函数 ===
void MergedProcessingPip::pipe(QSemaphore& inSem, QSemaphore& outSem) {
    FrameImage* pInFrame = (FrameImage*)m_pInImage;
    FrameImage* pOutFrame = (FrameImage*)m_pOutImage;
    int lastProcessedFrameId = -1;

    // 创建失败图像保存目录
    QString saveDir = "failed_frames";
    QDir dir;
    if (!dir.exists(saveDir)) {
        if (!dir.mkpath(saveDir)) {
            qWarning() << "无法创建失败帧保存目录:" << saveDir;
        } else {
            qDebug() << "创建失败帧保存目录:" << saveDir;
        }
    }

    while (!exit()) {
        inSem.acquire();
        if (pInFrame && !pInFrame->image.empty()) {
            int frameId = pInFrame->frameId;
            // 防止处理重复帧
            if (frameId == lastProcessedFrameId) {
                qWarning() << "MergedProcessingPip: 检测到重复帧" << frameId;
                outSem.release();
                continue;
            }
            lastProcessedFrameId = frameId;

            QElapsedTimer totalTimer;
            totalTimer.start();

            cv::Mat src = pInFrame->image.clone();
            SharedPipelineData::createFrameData(frameId, src);

            // 执行完整的处理流程
            bool success = processFrameComplete(frameId);

            if (!success) {
                qDebug() << "帧：" << frameId << "失败";

                // 保存失败的原始图像到本地
                try {
                    // 生成文件名：failed_frame_[frameId]_[timestamp].jpg
                    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
                    QString filename = QString("failed_frame_%1_%2.jpg").arg(frameId).arg(timestamp);
                    QString fullPath = QDir(saveDir).absoluteFilePath(filename);

                    // 转换QString到std::string
                    std::string stdPath = fullPath.toStdString();

                    // 保存图像
                    if (cv::imwrite(stdPath, src)) {
                        qDebug() << "失败帧已保存:" << fullPath;
                        qDebug() << "图像尺寸:" << src.cols << "x" << src.rows
                                 << "通道数:" << src.channels() << "类型:" << src.type();
                    } else {
                        qWarning() << "保存失败帧失败:" << fullPath;
                    }

                } catch (const cv::Exception& e) {
                    qWarning() << "保存失败帧时发生OpenCV异常:" << e.what();
                } catch (const std::exception& e) {
                    qWarning() << "保存失败帧时发生异常:" << e.what();
                } catch (...) {
                    qWarning() << "保存失败帧时发生未知异常";
                }
            }

            double totalTime = totalTimer.nsecsElapsed() / 1e6;
            // SharedPipelineData::setTime(frameId, 2, totalTime);

            // 发送信号
            emit processingComplete(frameId, success);
            emit sendOverSign(pInFrame->frameId);
        }
        outSem.release();
    }
}

// === 🔧 完整的帧处理函数 ===
bool MergedProcessingPip::processFrameComplete(int frameId) {
    QElapsedTimer stepTimer;

    try {
        // === 🔧 初始化当前帧数据 ===
        currentFrame.clear();
        currentFrame.frameId = frameId;

        // 从SharedPipelineData获取原始图像
        FrameData frameData;
        if (!SharedPipelineData::getFrameData(frameId, frameData)) {
            qDebug() << "无法获取帧数据，frameId:" << frameId;
            return false;
        }

        currentFrame.originalImage = frameData.originalImage.clone();
        if (currentFrame.originalImage.empty()) {
            qDebug() << "原始图像为空，frameId:" << frameId;
            return false;
        }

        // === 步骤1: ROI提取 ===
        stepTimer.start();
        if (!performROIExtraction()) {
            qWarning() << "ROI提取失败，frameId:" << frameId;
            return false;
        }
        double roiTime = stepTimer.nsecsElapsed() / 1e6 ;

        // === 步骤2: 光斑检测 ===
        stepTimer.restart();

        if (!performSpotDetection()) {
            qWarning() << "光斑检测失败，frameId:" << frameId;
            return false;
        }
        double spotTime = stepTimer.nsecsElapsed() / 1e6;

        // === 步骤3: 瞳孔检测 ===
        stepTimer.restart();
        if (!performPupilDetection()) {
            qWarning() << "瞳孔检测失败，frameId:" << frameId;
            return false;
        }
        double pupilTime = stepTimer.nsecsElapsed() / 1e6;

        // === 步骤4: 注视点计算 ===
        stepTimer.restart();
        if (!calculateGazePoint()) {
            qWarning() << "注视点计算失败，frameId:" << frameId;
            return false;
        }
        double gazeTime = stepTimer.nsecsElapsed() / 1e6;

        // === 🔧 保存结果到SharedPipelineData ===
        saveResultsToSharedData();

        // 记录时间
        SharedPipelineData::setTime(frameId, 2, roiTime);
        SharedPipelineData::setTime(frameId, 3, spotTime);
        SharedPipelineData::setTime(frameId, 4, pupilTime);

        return true;

    } catch (const std::exception& e) {
        qCritical() << "合并处理异常，frameId:" << frameId << "错误:" << e.what();
        SharedPipelineData::setCalculationError(frameId, true,
                                                "合并处理异常: " + std::string(e.what()));
        return false;
    } catch (...) {
        qCritical() << "合并处理未知异常，frameId:" << frameId;
        SharedPipelineData::setCalculationError(frameId, true, "合并处理未知异常");
        return false;
    }
}

// === 🔧 ROI提取 ===
bool MergedProcessingPip::performROIExtraction() {
    try {
        // 优化4: 减少不必要的计算和内存分配

        // 1. 最暗区域检测（已优化）
        currentFrame.darkestCenter = rolExtraction->getDarkestArea(currentFrame.originalImage);

        // 2. ROI区域创建（避免重复计算）
        currentFrame.roiRect = rolExtraction->createIrisRol(currentFrame.originalImage, currentFrame.darkestCenter);
        currentFrame.roiPoint = cv::Point(currentFrame.roiRect.x, currentFrame.roiRect.y);

        // 3. 暗点坐标调整 - 简化计算
        currentFrame.adjustedDarkPoint.x = currentFrame.darkestCenter.x - (currentFrame.roiRect.x - 30);
        currentFrame.adjustedDarkPoint.y = currentFrame.darkestCenter.y - (currentFrame.roiRect.y - 30);

        // 4. ROI图像提取
        rolExtraction->rolProcessImage(currentFrame.originalImage, currentFrame.roiRect, currentFrame.roiImage);

        // 优化5: 减少调试输出频率
        if (debugFlag && currentFrame.frameId % 10 == 0) {  // 每10帧输出一次
            qDebug() << QString("Frame %1 ROI: 原始暗点(%2,%3) -> 调整后(%4,%5)")
                            .arg(currentFrame.frameId)
                            .arg(currentFrame.darkestCenter.x).arg(currentFrame.darkestCenter.y)
                            .arg(currentFrame.adjustedDarkPoint.x).arg(currentFrame.adjustedDarkPoint.y);
        }

        return !currentFrame.roiImage.empty();

    } catch (const std::exception& e) {
        qCritical() << "ROI提取异常，frameId:" << currentFrame.frameId << "错误:" << e.what();
        return false;
    }
}

// === 🔧 光斑检测 ===
bool MergedProcessingPip::performSpotDetection() {
    try {
        // 1. 图像预处理
        cv::Mat blur, outPutLightImage;
        cv::normalize(currentFrame.roiImage, currentFrame.roiImage, 0, 255, cv::NORM_MINMAX);
        cv::GaussianBlur(currentFrame.roiImage, blur, cv::Size(5, 5), 0);
        cv::threshold(blur, outPutLightImage, 220, 255, cv::THRESH_BINARY);

        // 2. 光斑检测（使用调整后的暗点）
        currentFrame.lightSpots = spotExtraction->lightExpection(outPutLightImage, currentFrame.adjustedDarkPoint);

        // 3. 光斑智能处理
        cv::Mat processedBlur = blur.clone();
        spotProcessor->processLightSpots(processedBlur, currentFrame.lightSpots,
                                         cv::Point2f(currentFrame.adjustedDarkPoint.x, currentFrame.adjustedDarkPoint.y), 30);
        cv::Mat outPutPupilImage;
        //lijing
        // cv::threshold(processedBlur, outPutPupilImage, 100, 255, cv::THRESH_BINARY);
        //阳
        cv::threshold(processedBlur, outPutPupilImage, 85, 255, cv::THRESH_BINARY);

        currentFrame.processedImage = outPutPupilImage.clone();
        // 4. 坐标调整（转换回全图坐标）
        for (auto& spot : currentFrame.lightSpots) {
            spot.center.x += (currentFrame.roiPoint.x );  // 减去边距
            spot.center.y += (currentFrame.roiPoint.y );
        }


        // 5. 光斑排列
        bool arrangeSuccess = spotExtraction->arrangeSpots(currentFrame.lightSpots, currentFrame.arrangedSpots);

        if (!arrangeSuccess) {
            qDebug() << "光斑排列失败，frameId:" << currentFrame.frameId;
            return false;
        }

        // 调试输出
        if (debugFlag && currentFrame.arrangedSpots.size() >= 4) {
            qDebug() << QString("Frame %1 光斑坐标: [%2,%3] [%4,%5] [%6,%7] [%8,%9]")
                            .arg(currentFrame.frameId)
                            .arg(currentFrame.arrangedSpots[0].center.x).arg(currentFrame.arrangedSpots[0].center.y)
                            .arg(currentFrame.arrangedSpots[1].center.x).arg(currentFrame.arrangedSpots[1].center.y)
                            .arg(currentFrame.arrangedSpots[2].center.x).arg(currentFrame.arrangedSpots[2].center.y)
                            .arg(currentFrame.arrangedSpots[3].center.x).arg(currentFrame.arrangedSpots[3].center.y);
        }

        return currentFrame.arrangedSpots.size() >= 4;

    } catch (const std::exception& e) {
        qCritical() << "光斑检测异常，frameId:" << currentFrame.frameId << "错误:" << e.what();
        return false;
    }
}

// === 🔧 瞳孔检测 ===
bool MergedProcessingPip::performPupilDetection() {
    // try {
        // 瞳孔检测

        bool pupilSuccess = pupilExtraction->pupilDetection(currentFrame.processedImage, currentFrame.pupilCircle, currentFrame.frameId);

        if (pupilSuccess) {
            // 坐标调整（转换回全图坐标）
            currentFrame.pupilCircle.center.x += currentFrame.roiPoint.x;
            currentFrame.pupilCircle.center.y += currentFrame.roiPoint.y;


                qDebug() << QString("Frame %1 瞳孔中心: (%2,%3), 尺寸: %4x%5 角度：%6")
                                .arg(currentFrame.frameId)
                                .arg(currentFrame.pupilCircle.center.x).arg(currentFrame.pupilCircle.center.y)
                                .arg(currentFrame.pupilCircle.size.width).arg(currentFrame.pupilCircle.size.height)
                                .arg(currentFrame.pupilCircle.angle);            
            return true;
        }
        else{
            qDebug() << "失败";

        }

        return false;

    // }
    // catch (const std::exception& e) {
    //     qCritical() << "瞳孔检测异常，frameId:" << currentFrame.frameId << "错误:" << e.what();
    //     return false;
    // }
}

// === 🔧 注视点计算 ===
bool MergedProcessingPip::calculateGazePoint() {
    // 检查数据有效性
    if (currentFrame.arrangedSpots.size() < 4) {
        qDebug() << "光斑数量不足，frameId:" << currentFrame.frameId;
        return false;
    }

    try {
        // 使用类变量中的数据进行计算
        currentFrame.gazePoint = calculateGazeFromFourPoints(
            currentFrame.arrangedSpots[0].center,
            currentFrame.arrangedSpots[1].center,
            currentFrame.arrangedSpots[2].center,
            currentFrame.arrangedSpots[3].center,
            currentFrame.pupilCircle.center
            );
        // 验证计算结果
        if (std::isnan(currentFrame.gazePoint.x) || std::isnan(currentFrame.gazePoint.y) ||
            std::isinf(currentFrame.gazePoint.x) || std::isinf(currentFrame.gazePoint.y)) {
            qWarning() << "注视点计算结果无效，frameId:" << currentFrame.frameId;
            return false;
        }

        currentFrame.gazeValid = true;

        if (debugFlag) {
            qDebug() << QString("Frame %1 注视点: (%2,%3)")
                            .arg(currentFrame.frameId)
                            .arg(currentFrame.gazePoint.x, 0, 'f', 2)
                            .arg(currentFrame.gazePoint.y, 0, 'f', 2);
        }

        return true;

    } catch (const std::exception& e) {
        qCritical() << "注视点计算异常，frameId:" << currentFrame.frameId << "错误:" << e.what();
        return false;
    }
}

cv::Point2f MergedProcessingPip::calculateGazeFromFourPoints(
    const cv::Point &light1Rol,  // 左上光斑
    const cv::Point &light2Rol,  // 右上光斑
    const cv::Point &light3Rol,  // 左下光斑
    const cv::Point &light4Rol,  // 右下光斑
    const cv::Point &pupil)      // 瞳孔中心
{

    // 确保有四组映射系数
    if (m_mappingCoefficients.size() < 4) {
        qWarning() << "映射系数不足，无法计算注视点";
        return cv::Point2f(0, 0);
    }

    // 存储四个光斑计算出的注视点
    cv::Point2f gazePoints[4];

    // 创建光斑数组，便于循环处理
    const cv::Point lights[4] = {light1Rol, light2Rol, light3Rol, light4Rol};

    // 分别计算四个光斑的注视点
    for (int group = 0; group < 4; ++group) {
        // 获取当前光斑
        const cv::Point& light = lights[group];

        // 计算相对偏移量
        float dx = light.x - pupil.x;
        float dy = light.y - pupil.y;

        // 使用映射函数计算注视点x坐标（8个系数）
        float gazeX = m_mappingCoefficients[group].xCoeff[0];  // a0
        gazeX += m_mappingCoefficients[group].xCoeff[1] * dx;  // a1*dx
        gazeX += m_mappingCoefficients[group].xCoeff[2] * dy;  // a2*dy
        gazeX += m_mappingCoefficients[group].xCoeff[3] * dx * dx;  // a3*dx²
        gazeX += m_mappingCoefficients[group].xCoeff[4] * dx * dx * dx;  // a4*dx³
        gazeX += m_mappingCoefficients[group].xCoeff[5] * dx * dy;  // a5*dxdy
        gazeX += m_mappingCoefficients[group].xCoeff[6] * dx * dx * dy;  // a6*dx²dy
        gazeX += m_mappingCoefficients[group].xCoeff[7] * dx * dx * dx * dy;  // a7*dx³dy

        // 使用映射函数计算注视点y坐标（7个系数）
        float gazeY = m_mappingCoefficients[group].yCoeff[0];  // b0
        gazeY += m_mappingCoefficients[group].yCoeff[1] * dx;  // b1*dx
        gazeY += m_mappingCoefficients[group].yCoeff[2] * dy;  // b2*dy
        gazeY += m_mappingCoefficients[group].yCoeff[3] * dx * dx;  // b3*dx²
        gazeY += m_mappingCoefficients[group].yCoeff[4] * dy * dy;  // b4*dy²
        gazeY += m_mappingCoefficients[group].yCoeff[5] * dx * dy;  // b5*dxdy
        gazeY += m_mappingCoefficients[group].yCoeff[6] * dx * dx * dy;  // b6*dx²dy

        // 存储计算结果
        gazePoints[group] = cv::Point2f(gazeX, gazeY);
    }

    // 计算四个注视点的平均值
    cv::Point2f avgGazePoint(
        (gazePoints[0].x + gazePoints[1].x + gazePoints[2].x + gazePoints[3].x) / 4.0f,
        (gazePoints[0].y + gazePoints[1].y + gazePoints[2].y + gazePoints[3].y) / 4.0f
        );

    // 可选：输出调试信息
    // qDebug() << QString("注视点_x%1 注视点_y%2").arg(avgGazePoint.x).arg(avgGazePoint.y);

    return avgGazePoint;
}

void MergedProcessingPip::logProcessingResult(int frameId, bool success, double totalTime) {
    if (success) {
        qDebug() << QString("合并检测成功 - 帧%1, 总耗时:%2ms").arg(frameId).arg(totalTime, 0, 'f', 0);
    } else {
        qDebug() << QString("合并检测失败 - 帧%1, 总耗时:%2ms").arg(frameId).arg(totalTime, 0, 'f', 0);
    }
}

void MergedProcessingPip::initializeDefaultMappingCoefficients()
{
    m_mappingCoefficients.clear();
    m_mappingCoefficients.resize(4);

    // 默认映射系数（从 eyeTrack 移动过来）
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

    // 设置默认的组合系数
    combinedMappingCoefficients = m_mappingCoefficients[0];

    qDebug() << "MergedProcessingPip: 默认映射系数已初始化";
}

void MergedProcessingPip::setMappingCoefficients(const std::vector<MappingCoefficients>& coefficients)
{
    if (coefficients.empty()) {
        qWarning() << "MergedProcessingPip: 尝试设置空的映射系数，使用默认值";
        initializeDefaultMappingCoefficients();
    } else {
        m_mappingCoefficients = coefficients;
        qDebug() << "MergedProcessingPip: 映射系数已更新，共" << coefficients.size() << "组";
    }
}

void MergedProcessingPip::setCombinedMappingCoefficients(const MappingCoefficients& coefficient)
{
    combinedMappingCoefficients = coefficient;
    qDebug() << "MergedProcessingPip: 组合映射系数已更新";
}

void MergedProcessingPip::saveResultsToSharedData() {
    // 保存结果到SharedPipelineData，用于显示等外部需求
    SharedPipelineData::setRoiPoint(currentFrame.frameId, currentFrame.roiPoint);
    SharedPipelineData::setDarkPoint(currentFrame.frameId, currentFrame.adjustedDarkPoint);
    SharedPipelineData::setLightPoints(currentFrame.frameId, currentFrame.arrangedSpots);
    SharedPipelineData::setPupilCircle(currentFrame.frameId, currentFrame.pupilCircle);

    if (currentFrame.gazeValid) {
        SharedPipelineData::setGazePoint(currentFrame.frameId, currentFrame.gazePoint);
        SharedPipelineData::setGazeValid(currentFrame.frameId, true);
    }

    SharedPipelineData::setCalculationError(currentFrame.frameId, false);
}
