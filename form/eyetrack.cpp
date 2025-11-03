#include "eyetrack.h"
#include "ui_eyetrack.h"


eyeTrack::PerformanceStats eyeTrack::performanceStats;


eyeTrack::eyeTrack(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::eyeTrack)
{
    ui->setupUi(this);
    initializeComponents();
    scanCreamDevice();
    connect(ui->StarPushButton, &QPushButton::clicked, this, &eyeTrack::on_StarPushButton_clicked);
    connect(ui->OutPushButton, &QPushButton::clicked, this, &eyeTrack::on_OutPushButton_clicked);
    connect(ui->OutSavePushButton, &QPushButton::clicked, this, &eyeTrack::on_OutSavePushButton_clicked);
    connect(m_stopButton, &QPushButton::clicked, this, &eyeTrack::on_StopPushButton_clicked);
    connect(mergedPip, &MergedProcessingPip::processingComplete, this, &eyeTrack::processMergedResult);
    connect(this, &eyeTrack::chartSignals, this, [this]() {
        if (!m_trueGazePoints.empty() && !m_actualPredictions.empty()) {
            auto lastGaze = m_trueGazePoints.rbegin()->second;

            int lastFrameId = m_trueGazePoints.rbegin()->first;
            if (m_actualPredictions.find(lastFrameId) != m_actualPredictions.end()) {
                auto lastPredicted = m_actualPredictions[lastFrameId];
                chartUpdates(lastGaze, lastPredicted, lastFrameId);
            } else {
                // 如果没有预测值，使用真实值
                chartUpdates(lastGaze, lastGaze, lastFrameId);
            }
        }
    });
}

eyeTrack::~eyeTrack()
{
    if (currentState == RUNNING) {
        qDebug() << "析构时强制停止系统";
        currentState = STOPPING;

        if (timer) {
            timer->stop();
        }

        if (mergedPip) {
            disconnect(mergedPip, nullptr, this, nullptr);
        }

        if (pip) {
            pip->safeDeletePipeline();
        }
    }

    delete cameraPipe;
    delete mergedPip;
    delete pip;
    delete ui;

    qDebug() << "eyeTrack析构完成";
}

void eyeTrack::initializeComponents() {
    currentState = STOPPED;
    cameraFlag = false;


    performanceLabel = new QLabel("眼震检测系统已就绪", this);
    performanceLabel->setGeometry(10, 10, 500, 30);
    performanceLabel->setStyleSheet("color: green; font-weight: bold; background-color: rgba(0,0,0,0.1); padding: 5px;");

    // === 图表设置 ===
    GazePlot = new QCustomPlot(this);
    GazePlot->setGeometry(600, 1080, 600, 300);
    GazePlot->xAxis->setLabel("ORIGINAL PLOT X");
    GazePlot->yAxis->setLabel("ORIGINAL PLOT Y");
    GazePlot->xAxis->setRange(0, 2000);
    GazePlot->yAxis->setRange(0, 1500);

    GazePointGraph = GazePlot->addGraph();
    GazePointGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::blue), QBrush(Qt::blue), 5));
    GazePointGraph->setLineStyle(QCPGraph::lsNone);
    GazePlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

    PredictPlot = new QCustomPlot(this);
    PredictPlot->setGeometry(1200, 1080, 600, 300);
    PredictPlot->xAxis->setLabel("Predict PLOT X");
    PredictPlot->yAxis->setLabel("Predict PLOT Y");
    PredictPlot->xAxis->setRange(0, 2000);
    PredictPlot->yAxis->setRange(0, 1500);
    PredictPointGraph = PredictPlot->addGraph();
    PredictPointGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, QPen(Qt::red), QBrush(Qt::red), 5));
    PredictPointGraph->setLineStyle(QCPGraph::lsNone);
    PredictPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);



    m_stopButton = new QPushButton("停止",this);
    m_stopButton->setObjectName("stopButton");
    m_stopButton->setGeometry(QRect(1950, 1190, 171, 51));  // 调整Y坐标，避免超出窗口
    m_stopButton->setStyleSheet(QString::fromUtf8("background:transparent; \n"
                                                  "background:#3c3c3c;\n"
                                                  "color: white;\n"
                                                  "border-radius:20px;"));


    timer = new QTimer(this);
    cameraPipe = new videoCapturePip();
    mergedPip = new MergedProcessingPip();

    pip = new pipline();
    qDebug() << "组件初始化完成，初始状态:" << currentState;

    QString imagePath = "F://opencv_picture//moni.png";
    fieldImage = cv::imread(imagePath.toStdString());

    if (fieldImage.empty()) {
        qWarning() << "无法读取背景图像:" << imagePath;
    } else {
        qDebug() << "背景图像读取成功:" << imagePath;
        qDebug() << "原始图像尺寸:" << fieldImage.cols << "x" << fieldImage.rows
                 << "通道数:" << fieldImage.channels();
    }
    if (fieldImage.cols > 1920 || fieldImage.rows > 1080) {
        cv::resize(fieldImage, fieldImage, cv::Size(1920, 1080));
    }


    correctionParams.gainFactor = 1.0;
    correctionParams.maxOffset = 50.0;
    correctionParams.deadZone = 0.5;
    correctionParams.enableCorrection = true;
    smoothingFactor = 0.3;
    nystagmusSimulationActive = false;  // 确保初始状态为false

    currentOffset = cv::Point2f(0, 0);
    smoothOffset = cv::Point2f(0, 0);

    currentState = STOPPED;
    cameraFlag = false;
    predictionSystem.reset();  // 确保清洁状态

    baseImage = fieldImage.clone();
    image = fieldImage.clone();

    imageSize = cv::Size(IMAGE_WIDTH, IMAGE_HEIGHT);
    imageCenterReference = cv::Point2f(IMAGE_WIDTH / 2.0f, IMAGE_HEIGHT / 2.0f);  // (960, 540)
    currentCorrectionMode = MODE_NORMAL_CORRECTION;

    peakInfo = PeakDetectionInfo();
    qDebug() << "峰值检测系统已初始化";

    qDebug() << QString("固定参考点系统初始化完成 - 图像中心: (%.1f, %.1f)")
                    .arg(imageCenterReference.x)
                    .arg(imageCenterReference.y);
    qDebug() << QString("图像尺寸: %1 x %2").arg(IMAGE_WIDTH).arg(IMAGE_HEIGHT);

    qDebug() << "组件初始化完成，初始状态:" << currentState;
    qDebug() << "矫正系统参数已初始化：";
    qDebug() << "  - 增益系数:" << correctionParams.gainFactor;
    qDebug() << "  - 最大偏移:" << correctionParams.maxOffset;
    qDebug() << "  - 死区:" << correctionParams.deadZone;
    qDebug() << "  - 平滑系数:" << smoothingFactor;
    qDebug() << "  - 基准图像尺寸:" << baseImage.cols << "x" << baseImage.rows;
}




void eyeTrack::processMergedResult(int frameId, bool success) {
    QElapsedTimer timer;
    timer.start();

    static std::map<int, std::vector<cv::Point2f>> multiFramePredictions;

    static std::map<int, cv::Point2f> alphaBetaNextFramePredictions;
    static std::map<int, cv::Point2f> arxNextFramePredictions;

    static std::map<int, float> alphaBetaPreviousPredictionsX;
    static std::map<int, float> arxPreviousPredictionsX;

    static std::map<int, float> l2l3PreviousPredictionsX;
    static std::map<int, float> l1l2PreviousPredictionsX;
    static std::map<int, float> l1OnlyPreviousPredictionsX;


    static int lastProcessedFrameId = -1;
    static cv::Point2f lastValidGazePoint(960.0f, 540.0f);
    static cv::Point2f lastKnownGoodGazePoint(960.0f, 540.0f);
    static bool hasValidHistory = false;
    static int totalProcessedFrames = 0;

    // 眼震检测相关静态变量
    static int nystagmusPeakCount = 0;
    static cv::Point2f lastGazeDirection(0, 0);
    static int directionReversalCount = 0;
    static std::deque<float> velocityHistory;
    static const int VELOCITY_HISTORY_SIZE = 10;


    // 预测来源追踪
    static std::map<int, int> predictionSourceFrame;
    static std::map<int, cv::Point2f> frameGazePoints;

    totalProcessedFrames++;

    // === 第1步：防重复处理 ===
    if (frameId == lastProcessedFrameId) {
        qWarning() << "收到重复的帧ID:" << frameId;
        return;
    }

    // === 第2步：获取并评估之前的预测 ===
    cv::Point2f bestPredictionForThisFrame(960.0f, 540.0f);
    bool hasPrediction = false;
    int predictionSource = -1;

    // 查找最佳预测（来自1-3帧前）
    for (int lookback = 1; lookback <= 3; lookback++) {
        int sourceFrame = frameId - lookback;
        if (multiFramePredictions.find(sourceFrame) != multiFramePredictions.end()) {
            auto& predictions = multiFramePredictions[sourceFrame];
            if (predictions.size() >= lookback) {
                bestPredictionForThisFrame = predictions[lookback - 1];
                hasPrediction = true;
                predictionSource = sourceFrame;
                predictionSourceFrame[frameId] = sourceFrame;

                if (frameId % 50 == 0) {  // 减少日志输出频率
                    qDebug() << QString("帧%1: 使用来自帧%2的%3步预测")
                                    .arg(frameId).arg(sourceFrame).arg(lookback);
                }
                break;
            }
        }
    }
    //验证当前帧数据 ===
    FrameData frameData;
    bool hasValidData = false;
    cv::Point2f currentGazePoint(960.0f, 540.0f);
    ValidationResult validation;
    validation.success = success;

    if (!success) {
        validation.failReason = "MergedProcessingPip处理失败";
    } else if (!SharedPipelineData::getFrameData(frameId, frameData)) {
        validation.failReason = "无法获取帧数据";
    } else {
        validation.hasFrameData = true;
        validation.imageValid = !frameData.originalImage.empty();
        validation.gazeValid = frameData.gazeValid;
        validation.lightPointsValid = frameData.lightPoints.size() >= 4;
        validation.pupilValid = frameData.pupilCircle.center.x > 0 && frameData.pupilCircle.center.y > 0;

        if (!validation.imageValid) {
            validation.failReason = "原始图像为空";
        } else if (!validation.gazeValid) {
            validation.failReason = "注视点无效";
        } else if (!validation.lightPointsValid) {
            validation.failReason = "光斑数量不足：" + std::to_string(frameData.lightPoints.size());
        } else if (!validation.pupilValid) {
            validation.failReason = "瞳孔中心无效";
        } else {
            currentGazePoint = frameData.gazePoint;

            // 数据合理性检查
            if (std::isnan(currentGazePoint.x) || std::isnan(currentGazePoint.y) ||
                std::isinf(currentGazePoint.x) || std::isinf(currentGazePoint.y)) {
                validation.failReason = "注视点包含NaN或Inf值";
            } else if (std::abs(currentGazePoint.x) > 3000.0f || std::abs(currentGazePoint.y) > 3000.0f) {
                validation.failReason = "注视点超出合理范围";
            } else {
                hasValidData = true;
                lastKnownGoodGazePoint = currentGazePoint;
                hasValidHistory = true;
            }
        }
    }


    if (hasValidData) {

        frameGazePoints[frameId] = currentGazePoint;
        m_actualPredictions[frameId] = bestPredictionForThisFrame;
        m_trueGazePoints[frameId] = currentGazePoint;
        if(peakInfo.compensationActive &&
            peakInfo.compensationFrameCount == 2 &&
            frameId == peakInfo.compensationStartFrame + 1){
            float predictions = bestPredictionForThisFrame.x- currentGazePoint.x;
            if(predictions < 0)
            {
                peakInfo.skipNextCompensation = true;
            }
        }
    }

    // 处理无效数据的情况
    if (!hasValidData) {
        if (frameId % 30 == 0) {  // 减少无效数据的日志输出
            qWarning() << "帧" << frameId << "数据无效：" << QString::fromStdString(validation.failReason);
        }

        // 尝试使用历史预测
        if (hasValidHistory) {
            std::vector<cv::Point2f> fallbackPredictions(3, lastKnownGoodGazePoint);
            multiFramePredictions[frameId] = fallbackPredictions;
            m_nextFramePredictions[frameId] = fallbackPredictions[0];
        }

        lastProcessedFrameId = frameId;
        return;
    }

    //生成新的预测
    QElapsedTimer predictTimer;
    predictTimer.start();

    double processingTimeMs = 0;
    std::string diagnosticInfo;

    // 原有的BalancedLowLatencyPredictor预测
    cv::Point2f currentPrediction = predictionSystem.processFrame(
        currentGazePoint, frameId, processingTimeMs, diagnosticInfo);

    // double alphaBetaProcessingTime = 0;
    // std::string alphaBetaDiagnostic;
    // cv::Point2f alphaBetaNextPrediction = alphaBetaPredictor.processFrame(
    //     currentGazePoint, frameId, alphaBetaProcessingTime, alphaBetaDiagnostic);

    // // ARXPredictor预测（返回对下一帧的预测）
    // double arxProcessingTime = 0;
    // std::string arxDiagnostic;
    // cv::Point2f arxNextPrediction = arxPredictor.processFrame(
    //     currentGazePoint, frameId, arxProcessingTime, arxDiagnostic);


    // double kalmanProcessingTime = 0;
    // std::string kalmanDiagnostic;
    // cv::Point2f kalmanNextPrediction = kalmanPredictor.processFrame(
    //     currentGazePoint, frameId, kalmanProcessingTime, kalmanDiagnostic);

    // double l2l3ProcessingTime = 0;
    // std::string l2l3Diagnostic;
    // cv::Point2f l2l3NextPrediction = l2l3Predictor.processFrame(
    //     currentGazePoint, frameId, l2l3ProcessingTime, l2l3Diagnostic);

    // // L1+L2预测器（无趋势增强）
    // double l1l2ProcessingTime = 0;
    // std::string l1l2Diagnostic;
    // cv::Point2f l1l2NextPrediction = l1l2Predictor.processFrame(
    //     currentGazePoint, frameId, l1l2ProcessingTime, l1l2Diagnostic);

    // // 仅L1预测器（自适应前瞻）
    // double l1OnlyProcessingTime = 0;
    // std::string l1OnlyDiagnostic;
    // cv::Point2f l1OnlyNextPrediction = l1OnlyPredictor.processFrame(
    //     currentGazePoint, frameId, l1OnlyProcessingTime, l1OnlyDiagnostic);



    // // 添加静态变量存储卡尔曼的预测（在其他静态变量附近）
    // static std::map<int, float> kalmanPreviousPredictionsX;

    // // 处理卡尔曼预测的时序（在处理其他预测器后面）
    // if (kalmanPreviousPredictionsX.find(frameId) != kalmanPreviousPredictionsX.end()) {
    //     m_kalmanPredictionsX[frameId] = kalmanPreviousPredictionsX[frameId];
    // } else {
    //     // 使用已保存的上一帧数据，而不是从未来预测map中取
    //     if (frameId > 0 && m_kalmanPredictionsX.find(frameId-1) != m_kalmanPredictionsX.end()) {
    //         m_kalmanPredictionsX[frameId] = m_kalmanPredictionsX[frameId-1];
    //     } else {
    //         m_kalmanPredictionsX[frameId] = 960.0f; // 默认值（屏幕中心X坐标）
    //     }
    // }

    // // 保存当前帧的真实值
    // m_actualGazeX[frameId] = currentGazePoint.x;

    // // AlphaBeta预测
    // if (alphaBetaPreviousPredictionsX.find(frameId) != alphaBetaPreviousPredictionsX.end()) {
    //     m_alphaBetaPredictionsX[frameId] = alphaBetaPreviousPredictionsX[frameId];
    // } else {
    //     if (frameId > 0 && m_alphaBetaPredictionsX.find(frameId-1) != m_alphaBetaPredictionsX.end()) {
    //         m_alphaBetaPredictionsX[frameId] = m_alphaBetaPredictionsX[frameId-1];
    //     } else {
    //         m_alphaBetaPredictionsX[frameId] = 960.0f;
    //     }
    // }

    // // ARX预测
    // if (arxPreviousPredictionsX.find(frameId) != arxPreviousPredictionsX.end()) {
    //     m_arxPredictionsX[frameId] = arxPreviousPredictionsX[frameId];
    // } else {
    //     if (frameId > 0 && m_arxPredictionsX.find(frameId-1) != m_arxPredictionsX.end()) {
    //         m_arxPredictionsX[frameId] = m_arxPredictionsX[frameId-1];
    //     } else {
    //         m_arxPredictionsX[frameId] = 960.0f;
    //     }
    // }

    // // L2+L3预测
    // if (l2l3PreviousPredictionsX.find(frameId) != l2l3PreviousPredictionsX.end()) {
    //     m_l2l3PredictionsX[frameId] = l2l3PreviousPredictionsX[frameId];
    // } else {
    //     if (frameId > 0 && m_l2l3PredictionsX.find(frameId-1) != m_l2l3PredictionsX.end()) {
    //         m_l2l3PredictionsX[frameId] = m_l2l3PredictionsX[frameId-1];
    //     } else {
    //         m_l2l3PredictionsX[frameId] = 960.0f;
    //     }
    // }

    // // L1+L2预测
    // if (l1l2PreviousPredictionsX.find(frameId) != l1l2PreviousPredictionsX.end()) {
    //     m_l1l2PredictionsX[frameId] = l1l2PreviousPredictionsX[frameId];
    // } else {
    //     if (frameId > 0 && m_l1l2PredictionsX.find(frameId-1) != m_l1l2PredictionsX.end()) {
    //         m_l1l2PredictionsX[frameId] = m_l1l2PredictionsX[frameId-1];
    //     } else {
    //         m_l1l2PredictionsX[frameId] = 960.0f;
    //     }
    // }

    // // 仅L1预测
    // if (l1OnlyPreviousPredictionsX.find(frameId) != l1OnlyPreviousPredictionsX.end()) {
    //     m_l1OnlyPredictionsX[frameId] = l1OnlyPreviousPredictionsX[frameId];
    // } else {
    //     if (frameId > 0 && m_l1OnlyPredictionsX.find(frameId-1) != m_l1OnlyPredictionsX.end()) {
    //         m_l1OnlyPredictionsX[frameId] = m_l1OnlyPredictionsX[frameId-1];
    //     } else {
    //         m_l1OnlyPredictionsX[frameId] = 960.0f;
    //     }
    // }


    // // 保存对下一帧的预测
    // kalmanPreviousPredictionsX[frameId + 1] = kalmanNextPrediction.x;

    // alphaBetaPreviousPredictionsX[frameId + 1] = alphaBetaNextPrediction.x;
    // arxPreviousPredictionsX[frameId + 1] = arxNextPrediction.x;

    // l2l3PreviousPredictionsX[frameId + 1] = l2l3NextPrediction.x;
    // l1l2PreviousPredictionsX[frameId + 1] = l1l2NextPrediction.x;
    // l1OnlyPreviousPredictionsX[frameId + 1] = l1OnlyNextPrediction.x;

    // 清理旧的预测数据（避免内存泄漏）
    // if (alphaBetaPreviousPredictionsX.size() > 500) {
    //     for (auto it = alphaBetaPreviousPredictionsX.begin(); it != alphaBetaPreviousPredictionsX.end(); ) {
    //         if (it->first < frameId - 400) {
    //             it = alphaBetaPreviousPredictionsX.erase(it);
    //         } else {
    //             ++it;
    //         }
    //     }
    // }
    // if (arxPreviousPredictionsX.size() > 500) {
    //     for (auto it = arxPreviousPredictionsX.begin(); it != arxPreviousPredictionsX.end(); ) {
    //         if (it->first < frameId - 400) {
    //             it = arxPreviousPredictionsX.erase(it);
    //         } else {
    //             ++it;
    //         }
    //     }
    // }


    // 清理旧数据
    // if (kalmanPreviousPredictionsX.size() > 500) {
    //     for (auto it = kalmanPreviousPredictionsX.begin(); it != kalmanPreviousPredictionsX.end(); ) {
    //         if (it->first < frameId - 400) {
    //             it = kalmanPreviousPredictionsX.erase(it);
    //         } else {
    //             ++it;
    //         }
    //     }
    // }

    // if (frameId % 100 == 0) {
    //     qDebug() << QString("=== 预测器X轴比较 [帧%1] ===").arg(frameId);
    //     qDebug() << QString("真实值X: %.2f").arg(currentGazePoint.x);

    //     // AlphaBeta预测
    //     if (m_alphaBetaPredictionsX.find(frameId) != m_alphaBetaPredictionsX.end() &&
    //         m_alphaBetaPredictionsX[frameId] != currentGazePoint.x) {
    //         qDebug() << QString("AlphaBeta预测X: %.2f (误差: %.2f)")
    //                         .arg(m_alphaBetaPredictionsX[frameId])
    //                         .arg(std::abs(m_alphaBetaPredictionsX[frameId] - currentGazePoint.x));
    //     }

    //     // ARX预测
    //     if (m_arxPredictionsX.find(frameId) != m_arxPredictionsX.end() &&
    //         m_arxPredictionsX[frameId] != currentGazePoint.x) {
    //         qDebug() << QString("ARX预测X: %.2f (误差: %.2f)")
    //                         .arg(m_arxPredictionsX[frameId])
    //                         .arg(std::abs(m_arxPredictionsX[frameId] - currentGazePoint.x));
    //     }

    //     // Kalman预测
    //     if (m_kalmanPredictionsX.find(frameId) != m_kalmanPredictionsX.end() &&
    //         m_kalmanPredictionsX[frameId] != currentGazePoint.x) {
    //         qDebug() << QString("Kalman预测X: %.2f (误差: %.2f)")
    //                         .arg(m_kalmanPredictionsX[frameId])
    //                         .arg(std::abs(m_kalmanPredictionsX[frameId] - currentGazePoint.x));
    //     }

    //     // Balanced预测
    //     qDebug() << QString("Balanced预测X: %.2f (误差: %.2f)")
    //                     .arg(currentPrediction.x)
    //                     .arg(std::abs(currentPrediction.x - currentGazePoint.x));

    //     // 显示对下一帧的预测
    //     qDebug() << QString("下一帧预测 - AlphaBeta: %.2f, ARX: %.2f, Kalman: %.2f")
    //                     .arg(alphaBetaNextPrediction.x)
    //                     .arg(arxNextPrediction.x)
    //                     .arg(kalmanNextPrediction.x);
    // }


    // 生成多步预测 ===
    std::vector<cv::Point2f> futurePredictions;

    try {
        // 使用 BalancedLowLatencyPredictor 的多步预测能力
        futurePredictions = predictionSystem.getMultiStepPredictions(frameId);

        // 确保预测值有效
        for (size_t i = 0; i < futurePredictions.size(); i++) {
            if (std::isnan(futurePredictions[i].x) || std::isnan(futurePredictions[i].y) ||
                futurePredictions[i].x < 0 || futurePredictions[i].x > 1920 ||
                futurePredictions[i].y < 0 || futurePredictions[i].y > 1080) {

                qWarning() << QString("帧%1: 第%2步预测无效，使用当前预测值")
                                  .arg(frameId).arg(i + 1);
                futurePredictions[i] = currentPrediction;
            }
        }

    } catch (const std::exception& e) {
        qWarning() << "多步预测生成失败:" << e.what();
        // 回退方案：基于当前预测和速度生成
        futurePredictions.clear();

        cv::Point2f velocity(0, 0);
        if (hasValidHistory && !m_trueGazePoints.empty()) {
            auto lastIt = m_trueGazePoints.find(frameId - 1);
            if (lastIt != m_trueGazePoints.end()) {
                velocity = currentGazePoint - lastIt->second;
            }
        }

        // 生成简单的线性预测
        for (int step = 1; step <= 3; step++) {
            cv::Point2f futurePoint = currentPrediction + velocity * (step * 0.5f);

            // 边界检查
            futurePoint.x = std::max(0.0f, std::min(1920.0f, futurePoint.x));
            futurePoint.y = std::max(0.0f, std::min(1080.0f, futurePoint.y));

            futurePredictions.push_back(futurePoint);
        }
    }

    // 确保有3个预测值
    while (futurePredictions.size() < 3) {
        futurePredictions.push_back(currentPrediction);
    }

    //  眼震特征分析 ===
    cv::Point2f currentDirection = currentGazePoint - lastValidGazePoint;
    float currentVelocity = cv::norm(currentDirection);

    // 更新速度历史
    velocityHistory.push_back(currentVelocity);
    if (velocityHistory.size() > VELOCITY_HISTORY_SIZE) {
        velocityHistory.pop_front();
    }

    // === 峰值检测逻辑 ===
    bool peakDetected = false;

    if (hasValidData) {
        peakDetected = detectSimplePeak(currentGazePoint, frameId);

        if (peakDetected) {
            // 设置基准误差用于补偿
            cv::Point2f detectionFrameError;
            if (m_actualPredictions.find(frameId) != m_actualPredictions.end()) {
                detectionFrameError = m_actualPredictions[frameId] - currentGazePoint;
            } else {
                detectionFrameError = futurePredictions[0] - currentGazePoint;
            }
            peakInfo.baseCompensationError = detectionFrameError;
        }
    }


    lastGazeDirection = currentDirection;
    cv::Point2f detectionFrameError;

    if (peakDetected) {
        peakInfo.lastPeakFrame = frameId - 1;
        peakInfo.compensationStartFrame = frameId;
        peakInfo.compensationActive = true;


        // 优先使用来自之前帧的预测（这是真正的预测值）
        if (m_actualPredictions.find(frameId) != m_actualPredictions.end()) {
            cv::Point2f actualPrediction = m_actualPredictions[frameId];
            detectionFrameError = actualPrediction - currentGazePoint;

            qDebug() << QString("使用实际预测作为基准: 预测=(%1,%2), 真实=(%3,%4)")
                            .arg(actualPrediction.x).arg(actualPrediction.y)
                            .arg(currentGazePoint.x).arg(currentGazePoint.y);
        } else {
            // 回退：使用当前生成的预测
            detectionFrameError = futurePredictions[0] - currentGazePoint;

            qDebug() << QString(" 使用当前预测作为基准: 预测=(%1,%2), 真实=(%3,%4)")
                            .arg(futurePredictions[0].x).arg(futurePredictions[0].y)
                            .arg(currentGazePoint.x).arg(currentGazePoint.y);
        }

        peakInfo.baseCompensationError = detectionFrameError;

        // 保存其他峰值信息
        peakInfo.lastPeakVelocity = (velocityHistory.size() >= 2) ?
                                        velocityHistory[velocityHistory.size() - 2] : currentVelocity;
        peakInfo.lastPeakDirection = lastGazeDirection;

        qDebug() << QString("峰值检测[帧%1]: 峰值帧=%2, 基准误差=(%3,%4), 误差幅度=%5px, 补偿%6帧")
                        .arg(frameId)
                        .arg(frameId - 1)
                        .arg(detectionFrameError.x).arg(detectionFrameError.y)
                        .arg(cv::norm(detectionFrameError))
                        .arg(peakInfo.compensationFrameCount);
    }

    // 修改：动态补偿帧数应用
    if (peakInfo.compensationActive) {
        int framesSincePeak = frameId - peakInfo.compensationStartFrame;
        int maxCompensationFrames = peakInfo.compensationFrameCount - 1;  // 转换为0-based索引

        if (framesSincePeak >= 0 && framesSincePeak <= maxCompensationFrames) {
            float compensationFactor = 0;
            bool shouldApplyCompensation = true;

            if (peakInfo.compensationFrameCount == 2) {
                // 2帧补偿策略 (X > 600)
                if(detectionFrameError.x < 100)
                    detectionFrameError.x = 100;
                if (framesSincePeak == 0) {
                    compensationFactor = 0.7f;      // 第1帧：60%减小
                } else if (framesSincePeak == 1) {
                    if(peakInfo.skipNextCompensation){
                        shouldApplyCompensation = false;
                        peakInfo.skipNextCompensation = false;
                        qDebug()<<"第一帧补偿过了，停止补偿 帧："<<frameId;
                    }
                    else
                        compensationFactor = 0.4f;      // 第2帧：40%减小

                }
            } else {
                // 1帧补偿策略 (X <= 600)
                if (framesSincePeak == 0) {
                    compensationFactor = 0.55f;      // 第1帧：70%减小
                }
            }

            // 使用检测帧的误差作为基准，而不是当前帧误差
            cv::Point2f baseError = peakInfo.baseCompensationError;  // 检测帧的误差
            cv::Point2f reduction = baseError * compensationFactor;  // 按比例减小

            // 应用减小补偿
            cv::Point2f originalPrediction = futurePredictions[0];
            futurePredictions[0] -= reduction;  // 减去误差的一部分

            // 边界检查
            futurePredictions[0].x = std::max(0.0f, std::min(1920.0f, futurePredictions[0].x));
            futurePredictions[0].y = std::max(0.0f, std::min(1080.0f, futurePredictions[0].y));

            qDebug() << QString("动态补偿[帧%1]: 第%2/%3帧, 系数=%4f, 基准误差=(%5,%6), 减小%7fpx")
                            .arg(frameId)
                            .arg(framesSincePeak + 1)
                            .arg(peakInfo.compensationFrameCount)
                            .arg(compensationFactor)
                            .arg(baseError.x).arg(baseError.y)
                            .arg(cv::norm(reduction));

            qDebug() << QString("   预测变化: (%1f,%2f) → (%3f,%4f)")
                            .arg(originalPrediction.x).arg(originalPrediction.y)
                            .arg(futurePredictions[0].x).arg(futurePredictions[0].y);

            QString compensationMsg = QString("🔧 动态补偿[帧%1]: 第%2/%3帧, 系数=%4f, 减小%5fpx")
                                          .arg(frameId)
                                          .arg(framesSincePeak + 1)
                                          .arg(peakInfo.compensationFrameCount)
                                          .arg(compensationFactor)
                                          .arg(cv::norm(reduction));

        } else if (framesSincePeak > maxCompensationFrames) {
            // 补偿结束
            peakInfo.compensationActive = false;
            qDebug() << QString("动态补偿结束[帧%1]: 完成%2帧补偿")
                            .arg(frameId).arg(peakInfo.compensationFrameCount);
        }
    }

    // 存储预测结果
    multiFramePredictions[frameId] = futurePredictions;
    m_nextFramePredictions[frameId] = futurePredictions[0];

    // 存储其他数据
    //注视点相关信息
    std::vector<cv::Point2f> currentLightPoints(4);
    for(int i = 0; i < 4 && i < frameData.lightPoints.size(); i++){
        currentLightPoints[i] = frameData.lightPoints[i].center;
    }

    lightTotal[frameId] = currentLightPoints;
    pupilTotal[frameId] = frameData.pupilCircle.center;
    angelTotal[frameId] = frameData.pupilCircle.angle;
    areaTotal[frameId] = frameData.pupilCircle.area;
    eccentricityTotal[frameId] = frameData.pupilCircle.eccentricity;
    circularityTotal[frameId] = frameData.pupilCircle.circularity;

    // 处理时间
    double preTime = predictTimer.nsecsElapsed() / 1e6;
    videoCaptureTime[frameId] = frameData.capTime;
    pupilTime[frameId] = frameData.pupilTime;
    roiTime[frameId] = frameData.rolTime;
    spotTime[frameId] = frameData.spotTime;
    predictTime[frameId] = preTime;

    lastValidGazePoint = currentGazePoint;

    // === 第8步：清理旧数据 ===
    if (multiFramePredictions.size() > 300) {
        auto oldest = multiFramePredictions.begin();
        multiFramePredictions.erase(oldest);
    }

    if (frameGazePoints.size() > 500) {
        for (auto it = frameGazePoints.begin(); it != frameGazePoints.end(); ) {
            if (it->first < frameId - 400) {
                it = frameGazePoints.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (predictionSourceFrame.size() > 500) {
        for (auto it = predictionSourceFrame.begin(); it != predictionSourceFrame.end(); ) {
            if (it->first < frameId - 400) {
                it = predictionSourceFrame.erase(it);
            } else {
                ++it;
            }
        }
    }

    lastProcessedFrameId = frameId;

    // === 第9步：图像显示和UI更新 ===
    if (!nystagmusSimulationActive) {
        baseImage = fieldImage.clone();
        image = fieldImage.clone();
    }

    QElapsedTimer DrawTimer;
    DrawTimer.start();

    // 绘制和显示
    drawParallelMarkersAndDisplay(frameData.originalImage, &frameData, frameId);
    double Time = DrawTimer.nsecsElapsed() / 1e6;
    DrawTime[frameId] = Time;


    // 发送图表更新信号
    if (frameData.gazeValid) {
        emit chartSignals();
    }

    // === 第10步：性能监控和报告 ===
    // 更新性能统计
    if (hasPrediction) {
        double error = cv::norm(currentGazePoint - bestPredictionForThisFrame);
        if (error < 1000) {  // 过滤异常值
            performanceStats.totalFrames++;
            performanceStats.horizontalErrorSum += std::abs(currentGazePoint.x - bestPredictionForThisFrame.x);
            performanceStats.verticalErrorSum += std::abs(currentGazePoint.y - bestPredictionForThisFrame.y);

            if (error < 5.0) {
                performanceStats.highPrecisionFrames++;
            }

            performanceStats.recentErrors.push_back(error);
            if (performanceStats.recentErrors.size() > performanceStats.ERROR_WINDOW) {
                performanceStats.recentErrors.pop_front();
            }
        }
    }

    // 定期详细报告（每100帧）
    if (frameId % 100 == 0 && performanceStats.totalFrames > 0) {
        double avgHorizontalError = performanceStats.horizontalErrorSum / performanceStats.totalFrames;
        double avgVerticalError = performanceStats.verticalErrorSum / performanceStats.totalFrames;
        double precision = (double)performanceStats.highPrecisionFrames / performanceStats.totalFrames * 100;

        QString predictionStatus = QString("简化眼震预测[%1帧]: 水平误差=%2px, 垂直误差=%3px, 高精度率=%4%")
                                       .arg(performanceStats.totalFrames)
                                       .arg(avgHorizontalError, 0, 'f', 2)
                                       .arg(avgVerticalError, 0, 'f', 2)
                                       .arg(precision, 0, 'f', 1);

        // 预测器质量评分
        double quality = predictionSystem.getPredictionQuality();
        QString qualityStatus = QString("预测质量评分: %1/1.0 | 处理时间: %2ms")
                                    .arg(quality, 0, 'f', 2)
                                    .arg(processingTimeMs, 0, 'f', 2);

        // 眼震特征统计
        if (directionReversalCount > 0) {
            double timeInSeconds = frameId / 60.0;
            double nystagmusFreq = directionReversalCount / (2.0 * timeInSeconds);
            QString nystagmusStatus = QString("眼震特征: 方向反转%1次, 频率约%2Hz")
                                          .arg(directionReversalCount)
                                          .arg(nystagmusFreq, 0, 'f', 2);
        }

        // 预测器诊断信息
        QString diagnostics = QString::fromStdString(predictionSystem.getDiagnosticInfo());
        if (frameId % 200 == 0) {  // 每200帧输出一次详细诊断
            qDebug() << "简化预测系统诊断:\n" << diagnostics;
        }
    }

    // 实时性能监控（每30帧）
    if (frameId % 30 == 0) {
        double recentAvgError = 0;
        if (!performanceStats.recentErrors.empty()) {
            recentAvgError = std::accumulate(performanceStats.recentErrors.begin(),
                                             performanceStats.recentErrors.end(), 0.0) /
                             performanceStats.recentErrors.size();
        }

        // 眼震状态指示
        QString nystagmusIndicator = "";
        if (!velocityHistory.empty()) {
            float avgVelocity = std::accumulate(velocityHistory.begin(), velocityHistory.end(), 0.0f)
            / velocityHistory.size();
            if (avgVelocity > 100) {
                nystagmusIndicator = " | 眼震:活跃";
            } else if (avgVelocity > 50) {
                nystagmusIndicator = " | 眼震:中等";
            } else {
                nystagmusIndicator = " | 眼震:平静";
            }
        }

        // 预测器状态信息
        double predictionQuality = predictionSystem.getPredictionQuality();
        QString qualityIndicator = "";
        if (predictionQuality > 0.9) {
            qualityIndicator = " | 质量:优秀";
        } else if (predictionQuality > 0.8) {
            qualityIndicator = " | 质量:良好";
        } else if (predictionQuality > 0.7) {
            qualityIndicator = " | 质量:可接受";
        } else {
            qualityIndicator = " | 质量:需改进";
        }

        // 更新性能标签
        QString perfText = QString("简化预测系统 | 帧:%1 | 实时误差:%2px%3%4")
                               .arg(frameId)
                               .arg(recentAvgError, 0, 'f', 1)
                               .arg(nystagmusIndicator)
                               .arg(qualityIndicator);

        if (performanceLabel) {
            performanceLabel->setText(perfText);

            // 根据误差和质量设置颜色
            if (recentAvgError < 10 && predictionQuality > 0.9) {
                performanceLabel->setStyleSheet("color: green; font-weight: bold; background-color: rgba(0,255,0,0.1); padding: 5px;");
            } else if (recentAvgError < 20 && predictionQuality > 0.8) {
                performanceLabel->setStyleSheet("color: orange; font-weight: bold; background-color: rgba(255,165,0,0.1); padding: 5px;");
            } else {
                performanceLabel->setStyleSheet("color: red; font-weight: bold; background-color: rgba(255,0,0,0.1); padding: 5px;");
            }
        }
    }

    // 详细调试输出（每200帧）
    if (frameId % 200 == 0) {
        cv::Point2f predictionError = currentPrediction - currentGazePoint;
        qDebug() << "\n=== 简化预测系统状态报告 ===";
        qDebug() << QString("帧 %1: 处理时间 %2ms").arg(frameId).arg(timer.elapsed());
        qDebug() << QString("当前注视点: (%.2f, %.2f)").arg(currentGazePoint.x).arg(currentGazePoint.y);
        qDebug() << QString("预测结果: (%.2f, %.2f)").arg(currentPrediction.x).arg(currentPrediction.y);
        qDebug() << QString("预测误差: (%.2f, %.2f) | 幅度: %.2f px")
                        .arg(predictionError.x).arg(predictionError.y).arg(cv::norm(predictionError));
        qDebug() << QString("多步预测: %1步").arg(futurePredictions.size());
        qDebug() << QString("眼震统计: %1次反转, 平均速度%.1f px/帧")
                        .arg(directionReversalCount)
                        .arg(velocityHistory.empty() ? 0 :
                                 std::accumulate(velocityHistory.begin(), velocityHistory.end(), 0.0f) / velocityHistory.size());
        qDebug() << QString("预测器质量: %.1f%%, 诊断: %2")
                        .arg(predictionSystem.getPredictionQuality() * 100)
                        .arg(QString::fromStdString(diagnosticInfo).left(100));  // 只显示前100个字符
        qDebug() << "=====================================\n";
    }


}

void eyeTrack::drawParallelMarkersAndDisplay(cv::Mat& rgbImage, FrameData *frameData, int frameId)
{
    cv::Mat originalCopy = rgbImage.clone();

    // 绘制光斑（与原版保持一致）
    for(size_t i = 0; i < frameData->lightPoints.size(); ++i) {
        const auto& spot = frameData->lightPoints[i];
        // 绘制光斑圆点
        cv::circle(rgbImage, spot.center, 3, cv::Scalar(0, 0, 255), -1);
        // 添加光斑编号
        std::string spotText = std::to_string(i + 1);
        cv::Point textPos = spot.center;
        textPos.x += 5;
        textPos.y -= 5;
        cv::putText(rgbImage, spotText, textPos, cv::FONT_HERSHEY_SIMPLEX,
                    0.5, cv::Scalar(0, 0, 255), 1);
    }

    visualizePupilDetection(rgbImage, frameData->pupilCircle);

    // 在瞳孔中心添加文字标识
    std::string pupilText = "P";
    cv::Point textPos = frameData->pupilCircle.center;
    textPos.y -= frameData->pupilCircle.center.y + 10;
    cv::putText(rgbImage, pupilText, textPos, cv::FONT_HERSHEY_SIMPLEX,
                0.6, cv::Scalar(0, 255, 0), 2);

    // 添加总体信息显示
    std::string infoText = "Parallel Frame: " + std::to_string(frameId) +
                           " | Spots: " + std::to_string(frameData->lightPoints.size());
    cv::putText(rgbImage, infoText, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX,
                0.6, cv::Scalar(255, 255, 255), 2);


    // 转换并显示
    QImage qimg = QImage(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
    QPixmap pixmap = QPixmap::fromImage(qimg);

    // 🔧 获取label尺寸，减去一些边距
    QSize labelSize = ui->displayLabel->size();
    QSize adjustedSize(labelSize.width() - 20, labelSize.height() - 20);  // 减去20像素作为边距

    // 🔧 按调整后的尺寸缩放
    QPixmap centeredPixmap = pixmap.scaled(adjustedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    ui->displayLabel->setPixmap(centeredPixmap);

    // 保存图像
    if(dataFlag) {
        imageSave.addDisplayImageToBuffer(rgbImage,frameId);
        imageSave.addOriginalImageToBuffer(originalCopy,frameId);
    }
}

void eyeTrack::displayImage(const cv::Mat& image) {
    if (image.empty()) {
        qDebug() << "尝试显示空图像";
        return;
    }

    QImage qimg;

    // 根据图像通道数进行转换
    if (image.channels() == 1) {
        qimg = QImage(image.data, image.cols, image.rows, image.step, QImage::Format_Grayscale8);
    } else if (image.channels() == 3) {
        qimg = QImage(image.data, image.cols, image.rows, image.step, QImage::Format_RGB888);
    } else {
        qDebug() << "不支持的图像格式，通道数：" << image.channels();
        return;
    }

    if (qimg.isNull()) {
        qDebug() << "QImage 转换失败";
        return;
    }

    // === 关键修复：确保在主线程中更新UI ===
    QMetaObject::invokeMethod(this, [this, qimg]() {
        ui->displayLabel->setPixmap(QPixmap::fromImage(qimg).scaled(
            ui->displayLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        ui->displayLabel->update();  // 强制更新
    }, Qt::QueuedConnection);
}

void eyeTrack::updateGazePlots(const cv::Point2f& gazePoint, const cv::Point2f& prediction, int frameId) {
    // 更新真实注视点图表
    GazePointGraph->addData(gazePoint.x, gazePoint.y);
    GazePlot->rescaleAxes();
    GazePlot->replot();

    // 更新预测图表（显示预测轨迹，不是滤波轨迹）
    PredictPointGraph->addData(prediction.x, prediction.y);
    PredictPlot->rescaleAxes();
    PredictPlot->replot();

    // 定期清空数据
    static int clearCounter = 0;
    if (++clearCounter > 500) {
        GazePointGraph->data()->clear();
        PredictPointGraph->data()->clear();
        clearCounter = 0;
        qDebug() << "图表数据已清空";
    }
}
void eyeTrack::displayImageOnly(const cv::Mat& image) {
    displayImage(image);
}

void eyeTrack::acceptanceCoefficient(const std::vector<MappingCoefficients> &coefficients, const MappingCoefficients &coefficient)
{
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

void eyeTrack::SaveCollectingData() {
    qDebug() << "=== 开始保存数据 ===";
    qDebug() << "dataFlag状态:" << dataFlag;
    qDebug() << "m_trueGazePoints大小:" << m_trueGazePoints.size();
    qDebug() << "m_actualPredictions大小:" << m_actualPredictions.size();
    qDebug() << "m_nextFramePredictions大小:" << m_nextFramePredictions.size();

    // 输出一些示例数据
    if (!m_trueGazePoints.empty()) {
        auto first = m_trueGazePoints.begin();
        auto last = m_trueGazePoints.rbegin();
        qDebug() << "数据范围: 帧" << first->first << "到帧" << last->first;
        qDebug() << "第一帧数据:" << first->second.x << "," << first->second.y;
        qDebug() << "最后一帧数据:" << last->second.x << "," << last->second.y;
    }

    QString fileName = QDir::currentPath() + "/prediction_only_data.csv";
    qDebug() << "保存路径:" << fileName;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "无法写入预测数据文件:" << fileName;
        qDebug() << "文件错误:" << file.errorString();
        return;
    }
    QTextStream out(&file);

    // 修改CSV格式，添加时间列和注视点相关信息
    out << "frameId,"
        << "actualX,"
        << "predictedX,"
        << "alphaBetaPredX,"     // 新增
        << "arxPredX,"           // 新增
        << "kalmanPredX,"        // 新增
        << "l2l3PredX,"          // L2+L3
        << "l1l2PredX,"          // L1+L2
        << "l1OnlyPredX,"        // 仅L1

        // 注视点相关信息
        << "light1_x,light1_y,"  // 光斑1坐标
        << "light2_x,light2_y,"  // 光斑2坐标
        << "light3_x,light3_y,"  // 光斑3坐标
        << "light4_x,light4_y,"  // 光斑4坐标
        << "pupil_x,pupil_y,"    // 瞳孔中心坐标
        << "pupil_angle,"        // 瞳孔角度
        << "pupil_area,"        // 瞳孔面积
        << "pupil_eccentricity,"  // 瞳孔偏心率
        << "pupil_Circularity,"   // 瞳孔中线偏移

        << "videoCaptureTime,"
        << "pupilTime,"
        << "roiTime,"
        << "spotTime,"
        << "predictTime,"
        << "DrawTime,"
        << "totalProcessTime\n";  // 添加总处理时间

    int savedRecords = 0;

    // 遍历所有帧
    for (const auto& pair : m_trueGazePoints) {
        int frameId = pair.first;
        cv::Point2f actualGaze = pair.second;

        out << frameId << ",";
        out << actualGaze.x << ",";

        // 对下一帧的预测
        if (m_nextFramePredictions.find(frameId) != m_nextFramePredictions.end()) {
            cv::Point2f predForCurrentFrame = m_nextFramePredictions[frameId - 1];
            out << predForCurrentFrame.x << ",";
        } else {
            out << "NA,";
        }

        // AlphaBeta预测器X轴
        if (m_alphaBetaPredictionsX.find(frameId) != m_alphaBetaPredictionsX.end()) {
            out << m_alphaBetaPredictionsX[frameId] << ",";
        } else {
            out << "NA,";
        }

        // ARX预测器X轴
        if (m_arxPredictionsX.find(frameId) != m_arxPredictionsX.end()) {
            out << m_arxPredictionsX[frameId] << ",";
        } else {
            out << "NA,";
        }

        if (m_kalmanPredictionsX.find(frameId) != m_kalmanPredictionsX.end()) {
            out << m_kalmanPredictionsX[frameId] << ",";
        } else {
            out << "NA,";
        }

        if (m_l2l3PredictionsX.find(frameId) != m_l2l3PredictionsX.end()) {
            out << m_l2l3PredictionsX[frameId] << ",";
        } else {
            out << "NA,";
        }

        // L1+L2预测器X轴
        if (m_l1l2PredictionsX.find(frameId) != m_l1l2PredictionsX.end()) {
            out << m_l1l2PredictionsX[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 仅L1预测器X轴
        if (m_l1OnlyPredictionsX.find(frameId) != m_l1OnlyPredictionsX.end()) {
            out << m_l1OnlyPredictionsX[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 添加注视点相关信息
        // 光斑点数据 (4个光斑，每个光斑x,y坐标)
        if (lightTotal.find(frameId) != lightTotal.end()) {
            const auto& lightPoints = lightTotal[frameId];
            for (int i = 0; i < 4; i++) {
                if (i < lightPoints.size()) {
                    out << lightPoints[i].x << "," << lightPoints[i].y << ",";
                } else {
                    out << "NA,NA,";
                }
            }
        } else {
            out << "NA,NA,NA,NA,NA,NA,NA,NA,";  // 4个光斑，每个2个坐标
        }

        // 瞳孔中心数据
        if (pupilTotal.find(frameId) != pupilTotal.end()) {
            const auto& pupilCenter = pupilTotal[frameId];
            out << pupilCenter.x << "," << pupilCenter.y << ",";
        } else {
            out << "NA,NA,";
        }

        // 瞳孔角度数据
        if (angelTotal.find(frameId) != angelTotal.end()) {
            out << angelTotal[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 瞳孔角度面积
        if (areaTotal.find(frameId) != areaTotal.end()) {
            out << areaTotal[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 瞳孔偏心率
        if (eccentricityTotal.find(frameId) != eccentricityTotal.end()) {
            out << eccentricityTotal[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 瞳孔圆形度
        if (circularityTotal.find(frameId) != circularityTotal.end()) {
            out << circularityTotal[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 添加时间数据
        // 视频捕获时间
        if (videoCaptureTime.find(frameId) != videoCaptureTime.end()) {
            out << videoCaptureTime[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 瞳孔检测时间
        if (pupilTime.find(frameId) != pupilTime.end()) {
            out << pupilTime[frameId] << ",";
        } else {
            out << "NA,";
        }

        // ROI处理时间
        if (roiTime.find(frameId) != roiTime.end()) {
            out << roiTime[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 光斑检测时间
        if (spotTime.find(frameId) != spotTime.end()) {
            out << spotTime[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 预测时间
        if (predictTime.find(frameId) != predictTime.end()) {
            out << predictTime[frameId] << ",";
        } else {
            out << "NA,";
        }

        if (DrawTime.find(frameId) != DrawTime.end()) {
            out << DrawTime[frameId] << ",";
        } else {
            out << "NA,";
        }

        // 计算总处理时间（如果所有时间数据都存在）
        if (videoCaptureTime.find(frameId) != videoCaptureTime.end() &&
            pupilTime.find(frameId) != pupilTime.end() &&
            roiTime.find(frameId) != roiTime.end() &&
            spotTime.find(frameId) != spotTime.end() &&
            predictTime.find(frameId) != predictTime.end()) {

            double totalTime = videoCaptureTime[frameId] + pupilTime[frameId] +
                               roiTime[frameId] + spotTime[frameId] + predictTime[frameId] + DrawTime[frameId];
            out << totalTime;
        } else {
            out << "NA";
        }

        out << "\n";
        savedRecords++;

        // 每100条记录输出一次进度
        if (savedRecords % 100 == 0) {
            qDebug() << "已保存" << savedRecords << "条记录";
        }
    }

    file.close();
    qDebug() << "纯预测数据保存完成！";
    qDebug() << "总共保存了" << savedRecords << "条记录";
    qDebug() << "文件大小:" << QFileInfo(fileName).size() << "字节";

    // 输出预测性能总结
    if (performanceStats.totalFrames > 0) {
        double avgError = performanceStats.horizontalErrorSum / performanceStats.totalFrames;
        double precision = (double)performanceStats.highPrecisionFrames / performanceStats.totalFrames * 100;
        qDebug() << QString("预测性能总结: 平均误差=%1 px, 高精度率=%2%%")
                        .arg(avgError, 0, 'f', 2)
                        .arg(precision, 0, 'f', 1);
    }

    // 输出时间性能统计
    if (!videoCaptureTime.empty()) {
        double avgVideoCaptureTime = 0, avgPupilTime = 0, avgRoiTime = 0, avgSpotTime = 0, avgPredictTime = 0;
        int validTimeCount = 0;

        for (const auto& pair : videoCaptureTime) {
            int frameId = pair.first;
            if (pupilTime.find(frameId) != pupilTime.end() &&
                roiTime.find(frameId) != roiTime.end() &&
                spotTime.find(frameId) != spotTime.end() &&
                predictTime.find(frameId) != predictTime.end()) {

                avgVideoCaptureTime += pair.second;
                avgPupilTime += pupilTime[frameId];
                avgRoiTime += roiTime[frameId];
                avgSpotTime += spotTime[frameId];
                avgPredictTime += predictTime[frameId];
                validTimeCount++;
            }
        }

        if (validTimeCount > 0) {
            avgVideoCaptureTime /= validTimeCount;
            avgPupilTime /= validTimeCount;
            avgRoiTime /= validTimeCount;
            avgSpotTime /= validTimeCount;
            avgPredictTime /= validTimeCount;

            double avgTotalTime = avgVideoCaptureTime + avgPupilTime + avgRoiTime + avgSpotTime + avgPredictTime;

            qDebug() << "=== 时间性能统计 ===";
            qDebug() << QString("平均视频捕获时间: %1 ms").arg(avgVideoCaptureTime, 0, 'f', 2);
            qDebug() << QString("平均瞳孔检测时间: %1 ms").arg(avgPupilTime, 0, 'f', 2);
            qDebug() << QString("平均ROI处理时间: %1 ms").arg(avgRoiTime, 0, 'f', 2);
            qDebug() << QString("平均光斑检测时间: %1 ms").arg(avgSpotTime, 0, 'f', 2);
            qDebug() << QString("平均预测时间: %1 ms").arg(avgPredictTime, 0, 'f', 2);
            qDebug() << QString("平均总处理时间: %1 ms").arg(avgTotalTime, 0, 'f', 2);
            qDebug() << QString("平均FPS: %1").arg(1000.0 / avgTotalTime, 0, 'f', 1);
        }
    }
}
void eyeTrack::scanCreamDevice()
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

void eyeTrack::on_StarPushButton_clicked()
{
    static bool isButtonProcessing = false;
    if (isButtonProcessing) {
        qDebug() << "检测到重复点击，忽略此次调用";
        return;
    }

    isButtonProcessing = true;
    QTimer::singleShot(3000, []() {
        isButtonProcessing = false;
        qDebug() << "按钮处理标志已自动重置";
    });

    if (currentState == STARTING || currentState == STOPPING) {
        qDebug() << "系统正在" << (currentState == STARTING ? "启动" : "停止") << "中，请稍候...";
        isButtonProcessing = false;
        return;
    }

    qDebug() << "on_StarPushButton_clicked - 开始处理";

    if (currentState == STOPPED) {
        qDebug() << "=== 开始启动系统 ===";
        currentState = STARTING;
        cameraFlag = true;

        dataFlag = true;
        qDebug() << "数据收集已启用，dataFlag=" << dataFlag;

        try {
            int index = ui->comboBox->currentIndex();
            if (index == -1) {
                qWarning() << "未选择摄像头";
                currentState = STOPPED;
                cameraFlag = false;
                dataFlag = false;  // 重置标志
                isButtonProcessing = false;
                return;
            }

            QVariant selectedItemData = ui->comboBox->itemData(index);
            qDebug() << "selectedItemData type:" << selectedItemData.typeName();
            qDebug() << "selectedItemData:" << selectedItemData.toString();

            if (selectedItemData.toString() == "file") {
                qDebug() << "准备打开文件对话框";
                QString filePath = QFileDialog::getOpenFileName(this, "选择视频文件", "", "Videos (*.mp4 *.avi *.mjpeg)");
                qDebug() << "文件对话框已关闭，选择的文件：" << filePath;

                if (filePath.isEmpty()) {
                    qWarning() << "文件为空";
                    currentState = STOPPED;
                    cameraFlag = false;
                    dataFlag = false;  // 重置标志
                    isButtonProcessing = false;
                    return;
                }
                cameraPipe->setSource(1, filePath);
            } else {
                QCameraDevice selectedCamera = selectedItemData.value<QCameraDevice>();
                qDebug() << "selectedCamera:" << selectedCamera.description();
                if (selectedCamera.isNull()) {
                    qWarning() << "选择摄像头无效";
                    currentState = STOPPED;
                    cameraFlag = false;
                    dataFlag = false;  // 重置标志
                    isButtonProcessing = false;
                    return;
                }
                cameraPipe->setSource(0, selectedCamera.description());
            }

            pip->creat_capturepip(cameraPipe, false);
            pip->add_process_modles(mergedPip);
            pip->createPipeLine();

            imageSave.setImageBufferEnable(true);

            if (timer) {
                timer->start(30);
            }

            currentState = RUNNING;
            this->ui->StarPushButton->setText("关闭摄像头");
            performanceLabel->setText("系统运行中...");
            performanceLabel->setStyleSheet("color: green; font-weight: bold;");

            qDebug() << "=== 系统启动完成，dataFlag=" << dataFlag << " ===";

        } catch (const std::exception& e) {
            qCritical() << "启动过程中发生异常:" << e.what();
            currentState = STOPPED;
            cameraFlag = false;
            dataFlag = false;  // 重置标志
            this->ui->StarPushButton->setText("开启摄像头");
            performanceLabel->setText("启动失败");
            performanceLabel->setStyleSheet("color: red; font-weight: bold;");
            isButtonProcessing = false;
        }
    }

    isButtonProcessing = false;
    qDebug() << "on_StarPushButton_clicked - 处理完成";
}


bool eyeTrack::isSystemRunning() const {
    return currentState == RUNNING;
}

bool eyeTrack::isSystemReady() const {
    return currentState == STOPPED;
}

void eyeTrack::on_OutPushButton_clicked()
{
    SaveCollectingData();
    dataFlag = 0;
    ui->displayLabel->clear();
}

void eyeTrack::on_OutSavePushButton_clicked()
{
    SaveCollectingData();
    dataFlag = 0;
    ui->displayLabel->clear();
    pip->pausePipeLine();
    imageSave.saveDisplayBufferImage(this);
    imageSave.saveOriginalBufferImage(this);

    imageSave.setImageBufferEnable(false);
}


void eyeTrack::saveInvalidFrameImage(const cv::Mat& image, int frameId, QString fileName) {
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz");
    QString filename = QString("%1_%2_%3.jpg")
                           .arg(fileName)
                           .arg(frameId)
                           .arg(timestamp);
    QString filepath = QString("./error_images/%1").arg(filename);

    // 确保目录存在
    QDir dir("./error_images");
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存图像
    if (cv::imwrite(filepath.toStdString(), image)) {
        qDebug() << "无效帧图像已保存:" << filepath;
    } else {
        qDebug() << "保存无效帧图像失败:" << filepath;
    }
}

void eyeTrack::chartUpdates(const cv::Point2f &gazePoint, const cv::Point2f &predictedPoint, int frameId) {
    // 1. 更新图表显示（gazePoint是真实值，predictedPoint是预测值）
    updateGazePlots(gazePoint, predictedPoint, frameId);

    // 2. 验证注视点有效性
    if (!isGazePointValid(gazePoint)) {
        if (frameId % 30 == 0) {
            qWarning() << QString("帧%1: 注视点无效 (%.2f, %.2f)").arg(frameId).arg(gazePoint.x).arg(gazePoint.y);
        }
        return;
    }

    // 3. 使用预测值进行矫正
    cv::Point2f actualPredictionForCorrection;
    bool hasPrediction = false;

    // 检查是否有对当前帧的预测
    if (m_actualPredictions.find(frameId) != m_actualPredictions.end()) {
        actualPredictionForCorrection = m_actualPredictions[frameId];
        hasPrediction = true;

        if (frameId % 10 == 0) {
            qDebug() << QString("使用预测进行矫正 - 帧%1: 预测=(%2,%3), 真实=(%4,%5)")
                            .arg(frameId)
                            .arg(actualPredictionForCorrection.x, 0, 'f', 2)
                            .arg(actualPredictionForCorrection.y, 0, 'f', 2)
                            .arg(gazePoint.x, 0, 'f', 2)
                            .arg(gazePoint.y, 0, 'f', 2);
        }
    } else {
        // 没有预测时使用当前值
        actualPredictionForCorrection = gazePoint;
        hasPrediction = false;
    }
    QElapsedTimer DrawTimer;
    DrawTimer.start();

    // 4. 矫正处理（使用预测-真实差值进行矫正）
    if (nystagmusSimulationActive) {
        qDebug()<<"震颤";
        currentCorrectionMode = MODE_NYSTAGMUS_SIMULATION;
        processNystagmusSimulation(gazePoint, frameId);
    } else {
        qDebug()<<"矫正";
        currentCorrectionMode = MODE_NORMAL_CORRECTION;
        processNormalCorrection(gazePoint, actualPredictionForCorrection, frameId);
    }
    double Time = DrawTimer.nsecsElapsed() / 1e6;
    DrawTime[frameId] = Time;
}

bool eyeTrack::isGazePointValid(const cv::Point2f& gazePoint) {
    // 检查NaN和Inf
    if (std::isnan(gazePoint.x) || std::isnan(gazePoint.y) ||
        std::isinf(gazePoint.x) || std::isinf(gazePoint.y)) {
        return false;
    }

    // 检查是否在合理范围内（可以稍微超出图像边界）
    const float MARGIN = 500.0f;  // 允许超出图像边界的范围
    if (gazePoint.x < -MARGIN || gazePoint.x > IMAGE_WIDTH + MARGIN ||
        gazePoint.y < -MARGIN || gazePoint.y > IMAGE_HEIGHT + MARGIN) {
        return false;
    }

    return true;
}
void eyeTrack::processNystagmusSimulation(const cv::Point2f &currentGazePoint, int frameId) {
    if (!hasGazeReference) {
        centerReference = currentGazePoint;
        lastGazePoint = currentGazePoint;
        hasGazeReference = true;
        qDebug() << QString("设置参考中心: %1, %2").arg(centerReference.x, 0, 'f', 2).arg(centerReference.y, 0, 'f', 2);
        return;
    }

    cv::Point2f gazeOffset = currentGazePoint - centerReference;

    // 每帧都处理图像位移 - 去除间隔处理
    applyNystagmusDisplacement(gazeOffset);

    // 统计可以保留间隔（不影响图像处理）
    if (frameId % 5 == 0) {
        simStats.updateStats(gazeOffset);
    }

    if (frameId % 60 == 0) {
        outputRealTimeNystagmusStats(currentGazePoint, gazeOffset, frameId);
    }

    if (frameId % 30 == 0) {
        // 使用曼哈顿距离替代欧几里得距离
        double magnitude = std::abs(gazeOffset.x) + std::abs(gazeOffset.y);
        qDebug() << QString("帧%1: 当前=(%2,%3), 偏移=(%4,%5), 幅度=%6px")
                        .arg(frameId)
                        .arg(currentGazePoint.x, 0, 'f', 2)
                        .arg(currentGazePoint.y, 0, 'f', 2)
                        .arg(gazeOffset.x, 0, 'f', 2)
                        .arg(gazeOffset.y, 0, 'f', 2)
                        .arg(magnitude, 0, 'f', 2);
    }

    lastGazePoint = currentGazePoint;
}

void eyeTrack::outputRealTimeNystagmusStats(const cv::Point2f& currentGaze, const cv::Point2f& offset, int frameId) {
    QString status = QString("水平眼震模拟[%1帧]: 注视点X=%2, X偏移=%3px, 平均=%4px, 最大=%5px")
                         .arg(frameId)
                         .arg(currentGaze.x, 0, 'f', 1)
                         .arg(offset.x, 0, 'f', 1)
                         .arg(simStats.avgOffset, 0, 'f', 1)
                         .arg(simStats.maxOffset, 0, 'f', 1);

    performanceLabel->setText(status);
    qDebug() << status;
}

void eyeTrack::processNormalCorrection(const cv::Point2f &gazePoint, const cv::Point2f &predictedPoint, int frameId) {
    static int debugCounter = 0;
    debugCounter++;

    cv::Point2f displacement = calculateDisplacement(gazePoint, predictedPoint);

    // 使用曼哈顿距离简化计算
    double rawDisplacementMagnitude = std::abs(predictedPoint.x - gazePoint.x) + std::abs(predictedPoint.y - gazePoint.y);
    double finalDisplacementMagnitude = std::abs(displacement.x) + std::abs(displacement.y);

    applyTremorCorrection(displacement);

    // 每帧都更新图像显示 - 去除间隔处理
    updateCorrectedImageDisplay();

    // 减少统计计算频率（这个可以保留间隔，不影响图像处理）
    static double totalCorrectionError = 0;
    static int correctionCount = 0;
    if (frameId % 5 == 0) {
        totalCorrectionError += rawDisplacementMagnitude;
        correctionCount++;
    }

    if (correctionCount % 20 == 0 && debugCounter % 30 == 0) {
        double avgError = totalCorrectionError / correctionCount;
        qDebug() << QString("矫正统计%1: 平均误差=%2 px").arg(correctionCount).arg(avgError, 0, 'f', 2);
    }
}


void eyeTrack::applyNystagmusDisplacement(const cv::Point2f& gazeOffset) {
    if (originalFieldImage.empty()) {
        return;
    }

    // 使用1/4尺寸的原始图像
    cv::Mat smallOriginalImage;
    cv::Size processSize(originalFieldImage.cols / 4, originalFieldImage.rows / 4);
    cv::resize(originalFieldImage, smallOriginalImage, processSize, 0, 0, cv::INTER_LINEAR);

    cv::Mat displacedImage;
    cv::Point2f scaledOffset = gazeOffset * 0.25f;

    applyGazeBasedDisplacement(smallOriginalImage, displacedImage, scaledOffset);

    baseImage = displacedImage.clone();
    displayNystagmusImage(displacedImage, gazeOffset);
}


void eyeTrack::applyTremorCorrection(const Point2f &displacement)
{
    if (!correctionParams.enableCorrection) {
        return;
    }

    // 更新当前偏移量
    currentOffset = displacement;

    // 应用平滑滤波
    smoothOffset = smoothOffset * (1.0 - smoothingFactor) + currentOffset * smoothingFactor;

    // 记录矫正数据用于分析
    recordCorrectionData(displacement, smoothOffset);
}
cv::Point2f eyeTrack::calculateDisplacement(const cv::Point2f &gazePoint, const cv::Point2f &predictedPoint) {
    // 震颤矫正：计算预测误差并反向补偿
    // 如果预测点在真实点右边，说明眼睛将向右移动，我们需要将图像向左移动
    cv::Point2f predictionError = predictedPoint - gazePoint;
    cv::Point2f displacement = -predictionError;  // 反向移动以补偿

    static int debugCount = 0;
    debugCount++;


    // 只处理X轴
    displacement.y = 0;

    // 应用死区
    if (std::abs(displacement.x) < correctionParams.deadZone) {
        displacement.x = 0;
    }

    // 限制最大偏移
    if (std::abs(displacement.x) > correctionParams.maxOffset) {
        float sign = (displacement.x > 0) ? 1.0f : -1.0f;
        displacement.x = sign * static_cast<float>(correctionParams.maxOffset);
    }

    // 应用增益
    displacement.x *= static_cast<float>(correctionParams.gainFactor);

    return displacement;
}

void eyeTrack::visualizeDisplacement(cv::Mat& image, const cv::Point2f& displacement, const QString& mode) {
    // 在图像上绘制位移信息
    int centerX = image.cols / 2;
    int centerY = image.rows / 2;

    // 绘制中心点
    cv::circle(image, cv::Point(centerX, centerY), 5, cv::Scalar(0, 255, 0), -1);

    // 绘制位移向量
    if (cv::norm(displacement) > 1.0) {
        cv::Point endPoint(centerX + displacement.x * 5, centerY + displacement.y * 5);
        cv::arrowedLine(image, cv::Point(centerX, centerY), endPoint,
                        cv::Scalar(0, 0, 255), 2, 8, 0, 0.2);
    }

    // 添加文字说明
    std::string modeText = mode.toStdString() + " Displacement: " +
                           std::to_string((int)displacement.x) + "px";
    cv::putText(image, modeText, cv::Point(10, 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 0), 2);
}

void eyeTrack::updateCorrectedImageDisplay() {
    if (baseImage.empty()) {
        qDebug() << "基础图像为空";
        return;
    }

    // 使用1/4尺寸图像处理
    cv::Mat smallBaseImage;
    cv::Size processSize(baseImage.cols / 4, baseImage.rows / 4);
    cv::resize(baseImage, smallBaseImage, processSize, 0, 0, cv::INTER_LINEAR);

    cv::Mat correctedImage;
    cv::Point2f scaledOffset = smoothOffset * 0.25f;

    applySpatialCorrection(smallBaseImage, correctedImage, scaledOffset);

    if (correctedImage.empty()) {
        qWarning() << "矫正图像为空";
        return;
    }

    QImage qimg = matToQImage(correctedImage);
    if (qimg.isNull()) {
        qWarning() << "QImage转换失败";
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(qimg);
    if (!ui->VideoLabel) {
        qWarning() << "VideoLabel为空";
        return;
    }

    QSize labelSize = ui->VideoLabel->size();
    if (labelSize.width() > 0 && labelSize.height() > 0) {
        // 使用快速缩放
        pixmap = pixmap.scaled(labelSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        ui->VideoLabel->setPixmap(pixmap);
    }
}



void eyeTrack::applyGazeBasedDisplacement(const cv::Mat& inputImage, cv::Mat& outputImage, const cv::Point2f& gazeOffset) {
    if (inputImage.empty()) {
        qDebug() << "输入图像为空";
        return;
    }

    // 只处理X轴偏移，提高性能
    int offsetX = static_cast<int>(std::round(-gazeOffset.x));

    // 限制偏移范围
    offsetX = std::max(-100, std::min(100, offsetX));

    outputImage = cv::Mat::zeros(inputImage.size(), inputImage.type());

    if (offsetX == 0) {
        inputImage.copyTo(outputImage);
        addNystagmusSimulationOverlay(outputImage, gazeOffset);
        return;
    }

    cv::Rect srcRect, dstRect;

    if (offsetX > 0) {
        srcRect = cv::Rect(0, 0, inputImage.cols - offsetX, inputImage.rows);
        dstRect = cv::Rect(offsetX, 0, srcRect.width, srcRect.height);
    } else {
        int absOffsetX = -offsetX;
        srcRect = cv::Rect(absOffsetX, 0, inputImage.cols - absOffsetX, inputImage.rows);
        dstRect = cv::Rect(0, 0, srcRect.width, srcRect.height);
    }

    // 快速边界检查
    srcRect &= cv::Rect(0, 0, inputImage.cols, inputImage.rows);
    dstRect &= cv::Rect(0, 0, outputImage.cols, outputImage.rows);

    if (srcRect.width > 0 && srcRect.height > 0) {
        inputImage(srcRect).copyTo(outputImage(dstRect));
    }

    addNystagmusSimulationOverlay(outputImage, gazeOffset);
}

void eyeTrack::addNystagmusSimulationOverlay(cv::Mat& image, const cv::Point2f& gazeOffset) {
    // 添加眼震偏移信息文字
    std::string offsetText = "Horizontal Nystagmus Offset: " +
                             std::to_string((int)gazeOffset.x) + "px";

    cv::putText(image, offsetText, cv::Point(10, image.rows - 80),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(255, 255, 0), 2);

    std::string modeText = "Mode: Horizontal Nystagmus Simulation (X-axis Only)";
    cv::putText(image, modeText, cv::Point(10, image.rows - 50),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);

    // 显示参考点信息
    std::string refText = "Reference: Image Center (" +
                          std::to_string((int)imageCenterReference.x) + ", " +
                          std::to_string((int)imageCenterReference.y) + ")";
    cv::putText(image, refText, cv::Point(10, image.rows - 110),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 0), 2);

    // 统计信息
    std::string statsText = "Avg: " + std::to_string((int)simStats.avgOffset) + "px | " +
                            "Max: " + std::to_string((int)simStats.maxOffset) + "px | " +
                            "Frames: " + std::to_string(simStats.totalFrames);
    cv::putText(image, statsText, cv::Point(10, image.rows - 20),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 0), 1);

    // 绘制图像中心参考点（固定位置）
    int centerX = (int)imageCenterReference.x;
    int centerY = (int)imageCenterReference.y;

    // 确保中心点在图像范围内
    if (centerX >= 0 && centerX < image.cols && centerY >= 0 && centerY < image.rows) {
        // 绘制中心十字（较大，便于识别）
        cv::line(image, cv::Point(centerX-20, centerY), cv::Point(centerX+20, centerY),
                 cv::Scalar(0, 255, 255), 3);
        cv::line(image, cv::Point(centerX, centerY-20), cv::Point(centerX, centerY+20),
                 cv::Scalar(0, 255, 255), 3);

        // 在中心点附近添加文字标识
        cv::putText(image, "CENTER", cv::Point(centerX-30, centerY-25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);

        // 绘制水平偏移向量（只在X方向）
        if (std::abs(gazeOffset.x) > 3.0) {
            cv::Point offsetEnd(centerX + gazeOffset.x, centerY);  // Y坐标保持不变

            // 确保箭头终点在图像范围内
            offsetEnd.x = std::max(0, std::min(image.cols - 1, offsetEnd.x));

            cv::arrowedLine(image, cv::Point(centerX, centerY), offsetEnd,
                            cv::Scalar(0, 0, 255), 3, 8, 0, 0.3);
        }
    }

    // 绘制水平偏移轨迹
    drawHorizontalGazeOffsetTrajectory(image, centerX, centerY);
}


void eyeTrack::drawHorizontalGazeOffsetTrajectory(cv::Mat& image, int centerX, int centerY) {
    // 绘制最近的水平偏移轨迹
    if (simStats.recentOffsets.size() > 1) {
        for (size_t i = 1; i < simStats.recentOffsets.size(); i++) {
            // 只使用X偏移，Y坐标固定在中心
            cv::Point p1(centerX + simStats.recentOffsets[i-1].x, centerY);
            cv::Point p2(centerX + simStats.recentOffsets[i].x, centerY);

            // 渐变色彩，最新的更亮
            double alpha = (double)i / simStats.recentOffsets.size();
            cv::Scalar color(0, 255 * alpha, 255 * alpha);
            cv::line(image, p1, p2, color, 2);
        }
    }
}
void eyeTrack::displayNystagmusImage(const cv::Mat& displacedImage, const cv::Point2f& gazeOffset) {
    QImage qimg = matToQImage(displacedImage);
    if (qimg.isNull()) {
        return;
    }

    QPixmap pixmap = QPixmap::fromImage(qimg);
    if (!ui->VideoLabel) {
        return;
    }

    QSize labelSize = ui->VideoLabel->size();
    if (labelSize.width() > 0 && labelSize.height() > 0) {
        // 使用最快的缩放方式
        pixmap = pixmap.scaled(labelSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        ui->VideoLabel->setPixmap(pixmap);
    }
}
void eyeTrack::addQtNystagmusOverlay(QPixmap& pixmap, const cv::Point2f& gazeOffset) {
    QPainter painter(&pixmap);
    painter.setPen(QPen(Qt::yellow, 2));
    painter.setFont(QFont("Arial", 12, QFont::Bold));

    // 显示当前水平偏移信息
    QString offsetInfo = QString("Horizontal Offset: %1px")
                             .arg(gazeOffset.x, 0, 'f', 1);
    painter.drawText(10, 25, offsetInfo);

    // 显示模拟状态
    QString statusInfo = "Horizontal Nystagmus Simulation";
    painter.setPen(QPen(Qt::red, 2));
    painter.drawText(10, 50, statusInfo);

    // 显示统计信息
    QString statsInfo = QString("Avg: %1px | Max: %2px")
                            .arg(simStats.avgOffset, 0, 'f', 1)
                            .arg(simStats.maxOffset, 0, 'f', 1);
    painter.setPen(QPen(Qt::cyan, 2));
    painter.drawText(10, 75, statsInfo);
}


void eyeTrack::drawGazeOffsetTrajectory(cv::Mat& image, int centerX, int centerY) {
    // 绘制最近的偏移轨迹
    if (simStats.recentOffsets.size() > 1) {
        for (size_t i = 1; i < simStats.recentOffsets.size(); i++) {
            cv::Point p1(centerX + simStats.recentOffsets[i-1].x, centerY + simStats.recentOffsets[i-1].y);
            cv::Point p2(centerX + simStats.recentOffsets[i].x, centerY + simStats.recentOffsets[i].y);

            // 渐变色彩，最新的更亮
            double alpha = (double)i / simStats.recentOffsets.size();
            cv::Scalar color(0, 255 * alpha, 255 * alpha);
            cv::line(image, p1, p2, color, 2);
        }
    }
}
void eyeTrack::applySpatialCorrection(const cv::Mat& inputImage, cv::Mat& outputImage, const cv::Point2f& offset) {
    if (inputImage.empty()) {
        qDebug() << "输入图像为空";
        return;
    }

    // 简化为整数偏移，避免浮点运算
    int offsetX = static_cast<int>(std::round(offset.x));
    int offsetY = static_cast<int>(std::round(offset.y));

    // 限制偏移范围以避免过大计算
    offsetX = std::max(-50, std::min(50, offsetX));
    offsetY = std::max(-50, std::min(50, offsetY));

    // 直接使用零初始化，避免复杂填充
    outputImage = cv::Mat::zeros(inputImage.size(), inputImage.type());

    if (offsetX == 0 && offsetY == 0) {
        inputImage.copyTo(outputImage);
        return;
    }

    cv::Rect srcRect, dstRect;

    // 简化边界计算
    if (offsetX >= 0 && offsetY >= 0) {
        srcRect = cv::Rect(0, 0, inputImage.cols - offsetX, inputImage.rows - offsetY);
        dstRect = cv::Rect(offsetX, offsetY, srcRect.width, srcRect.height);
    } else if (offsetX < 0 && offsetY >= 0) {
        srcRect = cv::Rect(-offsetX, 0, inputImage.cols + offsetX, inputImage.rows - offsetY);
        dstRect = cv::Rect(0, offsetY, srcRect.width, srcRect.height);
    } else if (offsetX >= 0 && offsetY < 0) {
        srcRect = cv::Rect(0, -offsetY, inputImage.cols - offsetX, inputImage.rows + offsetY);
        dstRect = cv::Rect(offsetX, 0, srcRect.width, srcRect.height);
    } else {
        srcRect = cv::Rect(-offsetX, -offsetY, inputImage.cols + offsetX, inputImage.rows + offsetY);
        dstRect = cv::Rect(0, 0, srcRect.width, srcRect.height);
    }

    // 快速边界检查
    srcRect &= cv::Rect(0, 0, inputImage.cols, inputImage.rows);
    dstRect &= cv::Rect(0, 0, outputImage.cols, outputImage.rows);

    if (srcRect.width > 0 && srcRect.height > 0) {
        inputImage(srcRect).copyTo(outputImage(dstRect));
    }
}


void eyeTrack::handleBoundaryEffects(Mat &image, const Point2f &offset)
{
    // 如果偏移较大，可以通过填充或镜像边界来减少黑边
    if (cv::norm(offset) > 10.0) {
        cv::copyMakeBorder(image, image,
                           abs(offset.y), abs(offset.y),
                           abs(offset.x), abs(offset.x),
                           cv::BORDER_REFLECT);
    }
}

void eyeTrack::addCorrectionOverlay(QPixmap &pixmap)
{
    QPainter painter(&pixmap);
    painter.setPen(QPen(Qt::green, 2));
    painter.setFont(QFont("Arial", 12));

    // 显示当前偏移信息
    QString offsetInfo = QString("Offset: (%1, %2)")
                             .arg(smoothOffset.x, 0, 'f', 2)
                             .arg(smoothOffset.y, 0, 'f', 2);        painter.drawText(10, 25, offsetInfo);

    // 显示矫正状态
    QString statusInfo = correctionParams.enableCorrection ? "Correction: ON" : "Correction: OFF";
    painter.drawText(10, 45, statusInfo);

    // 绘制矫正向量
    if (cv::norm(smoothOffset) > correctionParams.deadZone) {
        int centerX = pixmap.width() / 2;
        int centerY = pixmap.height() / 2;

        painter.setPen(QPen(Qt::red, 3));
        painter.drawLine(centerX, centerY,
                         centerX - smoothOffset.x * 5,
                         centerY - smoothOffset.y * 5);

        // 绘制箭头
        drawArrow(painter, QPoint(centerX, centerY),
                  QPoint(centerX - smoothOffset.x * 5, centerY - smoothOffset.y * 5));
    }
}

void eyeTrack::drawArrow(QPainter &painter, const QPoint &start, const QPoint &end)
{
    painter.drawLine(start, end);

    // 计算箭头头部
    double angle = atan2((end.y() - start.y()), (end.x() - start.x()));
    int arrowLength = 10;
    double arrowAngle = M_PI / 6;

    QPoint arrowP1(
        end.x() - arrowLength * cos(angle - arrowAngle),
        end.y() - arrowLength * sin(angle - arrowAngle)
        );

    QPoint arrowP2(
        end.x() - arrowLength * cos(angle + arrowAngle),
        end.y() - arrowLength * sin(angle + arrowAngle)
        );

    painter.drawLine(end, arrowP1);
    painter.drawLine(end, arrowP2);
}

QImage eyeTrack::matToQImage(const Mat &mat)
{
    switch (mat.type()) {
    case CV_8UC4: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image.rgbSwapped();
    }
    case CV_8UC3: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    case CV_8UC1: {
        QImage image(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return image;
    }
    }
    return QImage();
}

void eyeTrack::recordCorrectionData(const Point2f &rawOffset, const Point2f &smoothedOffset)
{
    QMutexLocker locker(&m_dataStorageMutex);

    // 可以将数据保存到成员变量中用于后续分析
    struct CorrectionData {
        double timestamp;
        cv::Point2f rawOffset;
        cv::Point2f smoothedOffset;
        double correctionMagnitude;
    };

    static std::vector<CorrectionData> correctionHistory;

    CorrectionData data;
    data.timestamp = QEtimer.elapsed();
    data.rawOffset = rawOffset;
    data.smoothedOffset = smoothedOffset;
    data.correctionMagnitude = cv::norm(smoothedOffset);

    correctionHistory.push_back(data);

    // 限制历史数据大小
    if (correctionHistory.size() > 1000) {
        correctionHistory.erase(correctionHistory.begin());
    }
}

void eyeTrack::setCorrectionParameters(double gainFactor, double maxOffset, double deadZone, double smoothingFactor)
{
    correctionParams.gainFactor = gainFactor;
    correctionParams.maxOffset = maxOffset;
    correctionParams.deadZone = deadZone;
    this->smoothingFactor = smoothingFactor;
}

void eyeTrack::enableCorrection(bool enable)
{
    correctionParams.enableCorrection = enable;

    if (!enable) {
        // 重置偏移量
        currentOffset = cv::Point2f(0, 0);
        smoothOffset = cv::Point2f(0, 0);

        // 显示原始图像
        if (!baseImage.empty()) {
            QImage qimg = matToQImage(baseImage);
            QPixmap pixmap = QPixmap::fromImage(qimg);
            pixmap = pixmap.scaled(ui->VideoLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            ui->VideoLabel->setPixmap(pixmap);
        }
    }
}

void eyeTrack::startRealNystagmusSimulation() {
    qDebug() << QString("开始基于图像中心的水平眼震视野模拟 - 参考点(%.0f, %.0f)")
                    .arg(imageCenterReference.x)
                    .arg(imageCenterReference.y);

    // 保存原始背景图像
    if (!fieldImage.empty()) {
        originalFieldImage = fieldImage.clone();

        // 验证背景图像尺寸
        if (fieldImage.cols != IMAGE_WIDTH || fieldImage.rows != IMAGE_HEIGHT) {
            qDebug() << QString("背景图像尺寸: %1x%2, 期望尺寸: %3x%4")
                            .arg(fieldImage.cols).arg(fieldImage.rows)
                            .arg(IMAGE_WIDTH).arg(IMAGE_HEIGHT);

            // 如果尺寸不匹配，调整参考点
            if (fieldImage.cols > 0 && fieldImage.rows > 0) {
                cv::Point2f adjustedCenter(fieldImage.cols / 2.0f, fieldImage.rows / 2.0f);
                qDebug() << QString("使用实际图像中心: (%.0f, %.0f)").arg(adjustedCenter.x).arg(adjustedCenter.y);
                imageCenterReference = adjustedCenter;
            }
        }
    } else {
        qWarning() << "背景图像为空，无法开始模拟";
        return;
    }

    // 重置模拟统计
    simStats.reset();

    // 更新UI
    ui->NystagmusSimulation->setText("停止水平眼震模拟");

    // 显示模拟信息
    QString simInfo = QString("水平眼震视野模拟已启动 - 图像中心(%.0f, %.0f)")
                          .arg(imageCenterReference.x)
                          .arg(imageCenterReference.y);
    performanceLabel->setText(simInfo);
    performanceLabel->setStyleSheet("color: orange; font-weight: bold; background-color: rgba(255,165,0,0.2); padding: 5px;");

    qDebug() << simInfo;
}

void eyeTrack::stopRealNystagmusSimulation() {
    qDebug() << "停止眼震视野模拟";

    // 恢复原始背景图像和矫正系统
    if (!originalFieldImage.empty()) {
        fieldImage = originalFieldImage.clone();
        baseImage = fieldImage.clone();
        image = fieldImage.clone();
    }

    // 重新启用预测矫正系统
    correctionParams.enableCorrection = true;

    // 更新显示
    updateCorrectedImageDisplay();

    // 重置UI
    ui->NystagmusSimulation->setText("眼震视野模拟");
    QString statusInfo = QString("眼震模拟已停止 - 使用图像中心参考点(%.0f, %.0f)")
                             .arg(imageCenterReference.x)
                             .arg(imageCenterReference.y);
    performanceLabel->setText(statusInfo);
    performanceLabel->setStyleSheet("color: green; font-weight: bold; background-color: rgba(0,0,0,0.1); padding: 5px;");

    // 输出模拟统计
    outputNystagmusSimulationStats();
}

cv::Point2f eyeTrack::getGazeOffsetFromImageCenter(const cv::Point2f& gazePoint) {
    return gazePoint - imageCenterReference;
}

void eyeTrack::displayReferencePointInfo() {
    qDebug() << "=== 参考点信息 ===";
    qDebug() << QString("图像尺寸: %1 x %2").arg(IMAGE_WIDTH).arg(IMAGE_HEIGHT);
    qDebug() << QString("图像中心参考点: (%.0f, %.0f)").arg(imageCenterReference.x).arg(imageCenterReference.y);
    qDebug() << QString("当前模式: %1").arg(currentCorrectionMode == MODE_NYSTAGMUS_SIMULATION ? "眼震模拟" : "预测矫正");

    if (!fieldImage.empty()) {
        qDebug() << QString("实际背景图像尺寸: %1 x %2").arg(fieldImage.cols).arg(fieldImage.rows);
    }
}
void eyeTrack::outputNystagmusSimulationStats() {
    qDebug() << "=== 水平眼震视野模拟统计报告 ===";
    qDebug() << QString("总帧数: %1 帧").arg(simStats.totalFrames);
    qDebug() << QString("平均X轴偏移: %1 像素").arg(simStats.avgOffset, 0, 'f', 2);
    qDebug() << QString("最大X轴偏移: %1 像素").arg(simStats.maxOffset, 0, 'f', 2);
    qDebug() << QString("模拟模式: 基于真实注视点的水平视野震颤");

    // 在UI中显示最终统计
    QString finalStats = QString("水平眼震模拟完成: %1帧, 平均X偏移%2px, 最大X偏移%3px")
                             .arg(simStats.totalFrames)
                             .arg(simStats.avgOffset, 0, 'f', 1)
                             .arg(simStats.maxOffset, 0, 'f', 1);
    performanceLabel->setText(finalStats);

    // 计算一些有意思的统计
    if (!simStats.offsetMagnitudes.empty()) {
        // 计算偏移分布
        int smallOffsets = 0, mediumOffsets = 0, largeOffsets = 0;
        for (double mag : simStats.offsetMagnitudes) {
            if (mag < 10.0) smallOffsets++;
            else if (mag < 30.0) mediumOffsets++;
            else largeOffsets++;
        }

        double total = simStats.offsetMagnitudes.size();
        qDebug() << QString("X轴偏移分布: 小(<10px)=%.1f%%, 中(10-30px)=%.1f%%, 大(>30px)=%.1f%%")
                        .arg(smallOffsets/total*100)
                        .arg(mediumOffsets/total*100)
                        .arg(largeOffsets/total*100);
    }
}
void eyeTrack::on_NystagmusSimulation_clicked()
{
    nystagmusSimulationActive = !nystagmusSimulationActive;
    if (nystagmusSimulationActive) {  // 改为：当为true时启动模拟
        qDebug()<<"启动眼震模拟"<<nystagmusSimulationActive;
        startRealNystagmusSimulation();
        ui->NystagmusSimulation->setText("停止眼震模拟");  // 按钮显示"停止"
    } else {
        // 当为false时停止模拟
        qDebug()<<"停止眼震模拟"<<nystagmusSimulationActive;
        stopRealNystagmusSimulation();
        ui->NystagmusSimulation->setText("开始眼震模拟");  // 按钮显示"开始"
    }
}

cv::Point2f eyeTrack::applyAsymmetryCorrection(const cv::Point2f& basePrediction,
                                               const cv::Point2f& currentMeasurement,
                                               int frameId) {
    cv::Point2f corrected = basePrediction;

    // 如果没有历史数据，直接返回
    if (!hasLastMeasurement) {
        lastValidMeasurement = currentMeasurement;
        hasLastMeasurement = true;
        return basePrediction;
    }

    // 计算速度
    cv::Point2f velocity = currentMeasurement - lastValidMeasurement;

    //X轴非对称性修正
    if (velocity.x < -50) {  // 高速负向运动
        qDebug()<<"X轴非对称性修正";
        // 根据数据分析，误差约等于 -0.9 * velocity
        float compensation = std::abs(velocity.x) * 0.9f;
        corrected.x -= compensation;  // 进一步向负方向预测

        if (frameId % 10 == 0 || std::abs(velocity.x) > 100) {
            qDebug() << QString("非对称修正 - X轴: 速度=%1, 补偿=%2, 原预测=%3, 修正后=%4")
                            .arg(velocity.x, 0, 'f', 1)
                            .arg(compensation, 0, 'f', 1)
                            .arg(basePrediction.x, 0, 'f', 1)
                            .arg(corrected.x, 0, 'f', 1);
        }
    }


    // 更新历史
    lastValidMeasurement = currentMeasurement;

    // 边界检查
    corrected.x = std::max(0.0f, std::min(1920.0f, corrected.x));
    corrected.y = std::max(0.0f, std::min(1080.0f, corrected.y));

    return corrected;
}


bool eyeTrack::detectSimplePeak(const cv::Point2f& currentGazePoint, int frameId) {
    // 需要至少3个历史点
    static std::deque<cv::Point2f> recentPositions;
    static std::deque<int> recentFrames;

    // 添加当前位置
    recentPositions.push_back(currentGazePoint);
    recentFrames.push_back(frameId);

    // 只保留最近3个位置
    if (recentPositions.size() > 3) {
        recentPositions.pop_front();
        recentFrames.pop_front();
    }

    // 需要3个点才能检测峰值
    if (recentPositions.size() < 3) {
        return false;
    }

    cv::Point2f pos1 = recentPositions[0];  // 最早的点
    cv::Point2f pos2 = recentPositions[1];  // 中间的点（潜在峰值）
    cv::Point2f pos3 = recentPositions[2];  // 当前点

    // === 简单的峰值检测条件 ===

    // 1. 检测X轴的峰值：pos2.x 是局部最大值
    bool isXPeak = (pos2.x > pos1.x) && (pos2.x > pos3.x);

    // 2. 确保峰值足够明显（避免噪声）
    float leftRise = pos2.x - pos1.x;   // 上升幅度
    float rightFall = pos2.x - pos3.x;  // 下降幅度

    bool significantPeak = (leftRise > 10.0f) && (rightFall > 10.0f);

    // 3. X位置合理性检查
    bool validPosition = pos2.x > 550.0f;

    // 4. 时间间隔检查
    int actualPeakFrame = recentFrames[1];  // 峰值在中间帧
    bool validInterval = (actualPeakFrame - peakInfo.lastPeakFrame) > 5;

    // 所有条件满足才确认峰值
    if (isXPeak && significantPeak && validPosition && validInterval) {

        // 更新峰值信息
        peakInfo.lastPeakFrame = actualPeakFrame;
        peakInfo.lastPeakPosition = pos2;
        peakInfo.totalPeaksDetected++;

        // 根据X位置决定补偿帧数
        if (pos2.x > 650.0f) {
            peakInfo.compensationFrameCount = 2;
        } else {
            peakInfo.compensationFrameCount = 1;
        }

        // 启动补偿
        peakInfo.compensationActive = true;
        peakInfo.compensationStartFrame = frameId;

        qDebug() << QString(" 简单峰值检测[帧%1]: 峰值帧=%2, 位置(%3f,%4f), 上升=%5f, 下降=%6f")
                        .arg(frameId).arg(actualPeakFrame)
                        .arg(pos2.x).arg(pos2.y)
                        .arg(leftRise).arg(rightFall);

        QString peakMsg = QString(" 峰值[帧%1]: X轴从%2降到%3f, 补偿%4帧")
                              .arg(actualPeakFrame)
                              .arg(pos2.x).arg(pos3.x)
                              .arg(peakInfo.compensationFrameCount);


        return true;
    }

    return false;
}

void eyeTrack::initializeDefaultMappingCoefficients() {
    m_mappingCoefficients.clear();
    m_mappingCoefficients.resize(4);

    // 可以从配置文件或预设值中加载
    static const std::vector<std::vector<float>> defaultXCoeffs = {
        // 光斑 1 X系数
        {709.460632f, 11.855237f, -1.977625f, -0.012898f, 0.000192f, 0.012238f, 0.000111f, -0.000002f},
        // 光斑 2 X系数
        {1224.723999f, 11.907899f, -1.564755f, 0.008515f, 0.000191f, 0.012655f, 0.000026f, -0.000001f},
        // 光斑 3 X系数
        {1296.670532f, 11.641463f, -1.451853f, 0.008834f, 0.000329f, 0.008453f, 0.000013f, -0.000002f},
        // 光斑 4 X系数
        {795.380859f, 12.003286f, -1.795737f, -0.032124f, 0.000368f, 0.002715f, 0.000284f, -0.000003f}
    };

    static const std::vector<std::vector<float>> defaultYCoeffs = {
        // 光斑 1 Y系数
        {1362.719116f, -0.906065f, -10.206346f, -0.004179f, -0.042488f, 0.002962f, -0.000051f},
        // 光斑 2 Y系数
        {1298.638184f, -1.237444f, -10.240284f, -0.004194f, -0.044179f, -0.009231f, -0.000080f},
        // 光斑 3 Y系数
        {1909.829224f, -0.751395f, -11.713892f, -0.013533f, -0.011984f, -0.007396f, 0.000063f},
        // 光斑 4 Y系数
        {1984.473633f, -0.444348f, -12.664707f, -0.012776f, -0.005881f, -0.001961f, 0.000079f}
    };

    for(int i = 0; i < 4; i++) {
        m_mappingCoefficients[i].xCoeff = defaultXCoeffs[i];
        m_mappingCoefficients[i].yCoeff = defaultYCoeffs[i];
    }
}

void eyeTrack::printceCoefficient(const std::vector<MappingCoefficients> &coeffs, const MappingCoefficients &coeff)
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
void eyeTrack::on_StopPushButton_clicked()
{
    static bool stopFlag = 1;
    if(stopFlag)
    {
        m_stopButton->setText("恢复");
        pip->pausePipeLine();
    }
    else
    {
        m_stopButton->setText("暂停");
        pip->resumePipeLine();

    }
    stopFlag = !stopFlag;

}

