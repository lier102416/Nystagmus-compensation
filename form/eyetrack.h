#ifndef EYETRACK_H
#define EYETRACK_H

#include <QWidget>
#include <opencv2/opencv.hpp>
#include <QTimer>
#include "class.h"
#include <QCameraDevice>
#include <QMediaDevices>
#include "qcustomplot.h"
#include <pipline.h>
#include "videocapturepip.h"
#include "rolextractionpip.h"
#include "pupilextractionpip.h"
#include "spotextractionpip.h"
#include "gazeukf.h"
#include <QTextEdit>
#include "datesave.h"
#include "improvegazeukf.h"
#include "nystagmuadaptiveukf.h"
#include "nystagmusphasedetector.h"
#include "improvenystagmusphashdetector.h"
#include "nystagmusanalyzer.h"
#include "enhancenystagmusphasedetector.h"
#include "smart_frame_jump_handler.h"
#include "enhanced_naystagmus_prediction_optimizer.h"
#include "adaptiveparametercontroller.h"
#include "conservativenystagmuspredictor.h"
#include "advancedoutliercontroller.h"
#include "enhancedpredictor.h"
#include "temporalconsistencyfilter.h"
#include "enhancedoutliercontroller.h"
#include "mergedprocessingpip.h"
#include "class.h"
#include "improvednystagmuspredictor.h"
#include "enhancednystagmuspredictor.h"
#include "tunednystagmuspredictor.h"
#include "AntiOvershootNystagmusPredictor.h"
#include "finetunednystagmuspredictor.h"
#include "optimizedtunednystagmuspredictor.h"
#include "simplifiedoptimizedpredictor.h"
#include "horizontalnystagmuspredictor.h"
#include "purexaxispredictor.h"
#include "purexaxisukfpredictor.h"
#include "FinalPureXAxisUKFPredictor.h"
#include "RobustPureXAxisUKFPredictor.h"
#include "balancedpurexaxisukfpredictor.h"
#include "improvedbalancedpurexaxisukfpredictor.h"
#include "stableukfpredictor.h"
#include "lowlatencyukfpredictor.h"
#include "asymmetricnystagmuspredictor.h"
#include "optimizednystagmuspredictor.h"
#include "nystagmusawarepredictor.h"
#include "aggressivenystagmuspredictor.h"

#include "nystagmusoptimizedpredictor.h"
#include "simplestablenystagmuspredictor.h"
#include "zerolagpredictor.h"
#include "balancedlowlatencypredictor.h"
#include "nystagmuspeakawarepredictor.h"
#include "simplepeakoptimizer.h"

#include "improvednystagmuspeakdetector.h"
#include "PredictorComparison.h"
#include "enhancedarxpredictor.h"

#include "SingleAlphaBetaGammaPredictor.h"
#include "ARXPredictor.h"
#include "kalmanfilterpredictor.h"
#include "optimizedpurexaxisukfpredictor.h"

#include "L2L3Predictor.h"
#include "L1L2Predictor.h"
#include "L1OnlyPredictor.h"

namespace Ui {
class eyeTrack;
}

enum SystemState {
    STOPPED,
    STARTING,
    RUNNING,
    STOPPING
};


struct VarianceMetrics {
    double varianceX = 0.0;
    double varianceY = 0.0;
    double totalVariance = 0.0;
    double meanError = 0.0;
    double accuracy = 0.0;
};



class eyeTrack : public QWidget
{
    Q_OBJECT

private:
    //峰值检测
    struct PeakDetectionInfo {
        int lastPeakFrame = -1;
        cv::Point2f lastPeakPosition;
        cv::Point2f lastPeakDirection;
        float lastPeakVelocity = 0;
        int totalPeaksDetected = 0;
        int compensationFrameCount = 2;
        cv::Point2f baseCompensationError;
        // 新增：补偿相关
        bool compensationActive = false;
        bool skipNextCompensation = false;  //跳过下一次补偿标志
        int compensationStartFrame = -1;
    } peakInfo;

    // 模拟统计
    struct NystagmusSimStats {
        int totalFrames = 0;
        double maxOffset = 0.0;
        double avgOffset = 0.0;
        double totalOffset = 0.0;
        std::deque<cv::Point2f> recentOffsets;
        std::deque<double> offsetMagnitudes;

        void updateStats(const cv::Point2f& offset) {
            totalFrames++;
            double magnitude = cv::norm(offset);

            maxOffset = std::max(maxOffset, magnitude);
            totalOffset += magnitude;
            avgOffset = totalOffset / totalFrames;

            recentOffsets.push_back(offset);
            offsetMagnitudes.push_back(magnitude);

            // 保持最近100帧的数据
            if (recentOffsets.size() > 100) {
                recentOffsets.pop_front();
                offsetMagnitudes.pop_front();
            }
        }

        void reset() {
            totalFrames = 0;
            maxOffset = 0.0;
            avgOffset = 0.0;
            totalOffset = 0.0;
            recentOffsets.clear();
            offsetMagnitudes.clear();
        }
    } simStats;

    struct CorrectionParams {
        double gainFactor = 1.0;     // 矫正增益
        double maxOffset = 50.0;     // 最大偏移限制
        bool enableCorrection = true; // 启用矫正
        double deadZone = 2.0;       // 死区，小于此值的偏移将被忽略
    } correctionParams;
    cv::Mat fieldImage;           //视野图像


    // 性能统计（完全从并行版本复制）
    static struct PerformanceStats {
        int totalFrames = 0;
        int highPrecisionFrames = 0;  // <5像素误差
        int frameJumps = 0;
        std::deque<double> recentErrors;
        const int ERROR_WINDOW = 100;  // 🔧 关键：改为和并行版本一致的100
        double horizontalErrorSum = 0;
        double verticalErrorSum = 0;
        std::chrono::high_resolution_clock::time_point lastReportTime;

        PerformanceStats() {
            lastReportTime = std::chrono::high_resolution_clock::now();
        }

        double getRecentAvgError() const {
            if (recentErrors.empty()) return 0;
            return std::accumulate(recentErrors.begin(), recentErrors.end(), 0.0) / recentErrors.size();
        }
    } performanceStats;

    // 数据验证标志
    struct ValidationResult {
        bool success = false;
        bool hasFrameData = false;
        bool imageValid = false;
        bool gazeValid = false;
        bool lightPointsValid = false;
        bool pupilValid = false;
        std::string failReason;
    };


    Ui::eyeTrack * ui;
    QTimer * timer;
    QList<QCameraDevice> cameras;// 摄像头列表
    std::vector<MappingCoefficients> m_mappingCoefficients;//映射函数系数
    MappingCoefficients combinedMappingCoefficients;//映射函数系数

    QLabel* performanceLabel;

    dateSave imageSave;
    QPushButton* m_stopButton;  //暂停
    QPushButton* m_starButton;  //开始按钮


    //原始注视点
    QCustomPlot * GazePlot;
    QCPGraph *GazePointGraph;
    QVector<double> GazeX, GazeY;

    //预测注视点
    QCustomPlot * PredictPlot;
    QCPGraph *PredictPointGraph;
    QVector<double> PredictX, PredictY;

    MergedProcessingPip * mergedPip;
    //图像处理管线
    pipline *pip;
    videoCapturePip* cameraPipe;  // 保留视频采集

    SystemState currentState = STOPPED;  // 明确初始状态

    bool cameraFlag = false;
    bool hasValidData;

    int xPos = 0;
    int yPos = 0;
    QElapsedTimer QEtimer;//时间检测

    //预测系统 - 在eyetrack中初始化，但实际预测逻辑在processMergedResult中使用静态变量
    BalancedLowLatencyPredictor    predictionSystem;  // 使用稳健版预测器

    SingleAlphaBetaGammaPredictor alphaBetaPredictor;  // Add this
    ARXPredictor arxPredictor;  // Add this

    OptimizedPureXAxisUKFPredictor kalmanPredictor;  // 添加卡尔曼滤波预测器

    L2L3Predictor l2l3Predictor;      // L2+L3 (无自适应前瞻)
    L1L2Predictor l1l2Predictor;      // L1+L2 (无趋势增强)
    L1OnlyPredictor l1OnlyPredictor;  // 仅L1 (自适应前瞻)

    // 存储各预测器的X轴预测值用于对比
    std::map<int, float> m_l2l3PredictionsX;
    std::map<int, float> m_l1l2PredictionsX;
    std::map<int, float> m_l1OnlyPredictionsX;

    std::map<int, float> m_kalmanPredictionsX;  // 存储卡尔曼预测的X轴值

    std::map<int, float> m_balancedPredictionsX;
    std::map<int, float> m_alphaBetaPredictionsX;
    std::map<int, float> m_arxPredictionsX;
    std::map<int, float> m_actualGazeX;



    // 添加用于图像矫正的成员变量
    cv::Mat baseImage;           // 基准图像
    cv::Point2f currentOffset;   // 当前偏移量
    cv::Point2f smoothOffset;    // 平滑后的偏移量
    double smoothingFactor = 0.3; // 平滑系数


    // 眼震模拟相关成员
    bool nystagmusSimulationActive = false; //false 为真实眼震 true 为矫正后
    QTimer* simulationTimer = nullptr;
    cv::Mat originalFieldImage;  // 保存原始背景图像
    cv::Point2f lastGazePoint;   // 上一个注视点，用于计算偏移
    cv::Point2f centerReference; // 中心参考点
    bool hasGazeReference = false;


    // 固定图像中心参考点
    cv::Point2f imageCenterReference;      // 图像中心作为固定参考点
    cv::Size imageSize;                    // 图像尺寸

    // 模式状态
    enum CorrectionMode {
        MODE_NORMAL_CORRECTION,
        MODE_NYSTAGMUS_SIMULATION
    } currentCorrectionMode;

    // 常量定义
    static const int IMAGE_WIDTH = 1920;
    static const int IMAGE_HEIGHT = 1080;


public:
    explicit eyeTrack(QWidget *parent = nullptr);
    ~eyeTrack();
    cv::Mat image;
    bool flat = true;
    int moveSpeed = 5;  // 移动速度
    int stripeWidth = 30;  // 条纹宽度
    bool horizontalMovement = true;
    bool detectionFlag = 0;
    int labelWidth = 1000;
    int labelHight = 600;
    int dataFlag = 1;

    std::map<int, cv::Point2f> m_actualPredictions;     // 存储真正的预测值（之前对该帧的预测）

    std::map<int, cv::Point2f> m_nextFramePredictions;  // 存储对下一帧的预测
    std::map<int , cv::Point2f> m_trueGazePoints;       // 存储真实眼震


    std::map<int , std::vector<cv::Point2f>> lightTotal;
    std::map<int , cv::Point2f> pupilTotal;
    std::map<int , float> eccentricityTotal;
    std::map<int , float> circularityTotal;

    std::map<int , float> angelTotal;
    std::map<int , float> areaTotal;


    std::map<int , double> videoCaptureTime;
    std::map<int , double> pupilTime;
    std::map<int , double> roiTime;
    std::map<int , double> spotTime;
    std::map<int , double> predictTime;
    std::map<int , double> DrawTime;

    std::vector<float> m_predictionErrors;

    cv::Point2f lastPrediction ;
    bool first = 0;
    int pCount = 0; //预测计数

    mutable QMutex m_dataStorageMutex;

    // 添加帧处理记录，防止重复处理
    QSet<int> m_processedFrameIds;
    mutable QMutex m_processedFrameMutex;

    cv::Point2f lastValidMeasurement;
    bool hasLastMeasurement = false;


    void processVideoFrame(int frameId);
    void scanCreamDevice();



    void acceptanceCoefficient(const std::vector<MappingCoefficients> & coefficients, const MappingCoefficients & coefficient);

    void SaveCollectingData();

    void drawMarkersAndDisplay(cv::Mat& image, const FrameData& frameData, int frameId);
    void displayImage(const cv::Mat& image);
    void displayImageOnly(const cv::Mat& image);

    void initializeComponents();
    void drawFrameDataAndDisplay(cv::Mat& image, const FrameData& frameData,
                                 const cv::Point2f& gazePoint, const cv::Point2f& predictedGaze) ;
    void outputPerformanceReport(int frameId);
    void updateGazePlots(const cv::Point2f& gazePoint, const cv::Point2f& prediction, int frameId);
    void saveInvalidFrameImage(const cv::Mat& image, int frameId, QString fileName);

    void processMergedResult(int frameId, bool success);

    void drawPartialFrameData(cv::Mat& image, const FrameData& frameData, int frameId);

    void drawParallelMarkersAndDisplay(cv::Mat& rgbImage, FrameData *frameData, int frameId);

    bool isSystemRunning() const;
    bool isSystemReady() const;

    // 计算位移差异的函数
    cv::Point2f calculateDisplacement(const cv::Point2f& gazePoint, const cv::Point2f& predictedPoint);
    // 应用震颤矫正
    void applyTremorCorrection(const cv::Point2f& displacement);
    // 更新矫正后的图像显示
    void updateCorrectedImageDisplay();
    // 应用空间矫正
    void applySpatialCorrection(const cv::Mat& inputImage, cv::Mat& outputImage, const cv::Point2f& offset);
    // 处理边界效应
    void handleBoundaryEffects(cv::Mat& image, const cv::Point2f& offset);

    void addCorrectionOverlay(QPixmap& pixmap);
    // 绘制箭头
    void drawArrow(QPainter& painter, const QPoint& start, const QPoint& end);

    QImage matToQImage(const cv::Mat& mat);
    // 记录矫正数据

    void recordCorrectionData(const cv::Point2f& rawOffset, const cv::Point2f& smoothedOffset);

    void setCorrectionParameters(double gainFactor, double maxOffset,
                                 double deadZone, double smoothingFactor);

    void enableCorrection(bool enable);

    void startRealNystagmusSimulation() ;

    void stopRealNystagmusSimulation();

    void outputNystagmusSimulationStats();

    void processNystagmusSimulation(const cv::Point2f &currentGazePoint, int frameId);
    void processNormalCorrection(const cv::Point2f &gazePoint, const cv::Point2f &predictedPoint, int frameId);

    void applyNystagmusDisplacement(const cv::Point2f &gazeOffset);
    void applyGazeBasedDisplacement(const cv::Mat& inputImage, cv::Mat& outputImage, const cv::Point2f& gazeOffset);
    void addNystagmusSimulationOverlay(cv::Mat& image, const cv::Point2f& gazeOffset);
    void drawHorizontalGazeOffsetTrajectory(cv::Mat& image, int centerX, int centerY) ;
    void drawGazeOffsetTrajectory(cv::Mat& image, int centerX, int centerY);
    void displayNystagmusImage(const cv::Mat& displacedImage, const cv::Point2f& gazeOffset);
    void addQtNystagmusOverlay(QPixmap& pixmap, const cv::Point2f& gazeOffset);
    void outputRealTimeNystagmusStats(const cv::Point2f& currentGaze, const cv::Point2f& offset, int frameId) ;

    bool isGazePointValid(const cv::Point2f& gazePoint);
    cv::Point2f getGazeOffsetFromImageCenter(const cv::Point2f& gazePoint) ;

    void displayReferencePointInfo();

    void visualizeDisplacement(cv::Mat& image, const cv::Point2f& displacement, const QString& mode);

    cv::Point2f applyAsymmetryCorrection(const cv::Point2f& basePrediction,
                                         const cv::Point2f& currentMeasurement,
                                         int frameId) ;
    bool detectSimplePeak(const cv::Point2f& currentGazePoint, int frameId);
    void initializeDefaultMappingCoefficients();
    void printceCoefficient(const std::vector<MappingCoefficients> &coeffs, const MappingCoefficients &coeff);

signals:
    void chartSignals();

private slots:
    void on_StarPushButton_clicked();
    void on_OutPushButton_clicked();
    void on_OutSavePushButton_clicked();
    void chartUpdates(const cv::Point2f& gazePoint, const cv::Point2f& predictedPoint, int frameId);


    void on_NystagmusSimulation_clicked();
    void on_StopPushButton_clicked();
};

#endif // EYETRACK_H
