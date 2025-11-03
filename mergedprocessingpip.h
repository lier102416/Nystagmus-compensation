#ifndef MERGEDPROCESSINGPIP_H
#define MERGEDPROCESSINGPIP_H

#include "pipline.h"
#include "class.h"
#include "rolextraction.h"
#include "spotextraction.h"
#include "pupiletraction.h"
#include "sharedpipelinedate.h"
#include "smartspotprocessor.h"
#include <deque>
#include <chrono>

class MergedProcessingPip : public QObject, public AbstractPipe {
    Q_OBJECT
public:
    MergedProcessingPip();
    ~MergedProcessingPip();

    void pipe(QSemaphore& inSem, QSemaphore& outSem) override;

    void setMappingCoefficients(const std::vector<MappingCoefficients>& coefficients);
    void setCombinedMappingCoefficients(const MappingCoefficients& coefficient);
    std::vector<MappingCoefficients> getMappingCoefficients() const { return m_mappingCoefficients; }
    MappingCoefficients getCombinedMappingCoefficients() const { return combinedMappingCoefficients; }

signals:
    void sendOverSign(int frameId);
    void processingComplete(int frameId, bool success);

private:
    // === 🔧 核心处理函数 ===
    bool processFrameComplete(int frameId);
    bool performROIExtraction();
    bool performSpotDetection();
    bool performPupilDetection();
    bool calculateGazePoint();

    // === 🔧 辅助函数 ===
    cv::Point2f calculateGazeFromFourPoints(
        const cv::Point &light1Rol,  // 左上光斑
        const cv::Point &light2Rol,  // 右上光斑
        const cv::Point &light3Rol,  // 左下光斑
        const cv::Point &light4Rol,  // 右下光斑
        const cv::Point &pupil);      // 瞳孔中心
    void logProcessingResult(int frameId, bool success, double totalTime);
    void initializeDefaultMappingCoefficients();
    void saveResultsToSharedData();

    // === 🔧 处理组件 ===
    RolExtraction* rolExtraction;
    SpotExtraction* spotExtraction;
    PupilEtraction* pupilExtraction;
    SmartSpotProcessor* spotProcessor;

    // === 🔧 简化的性能统计 ===
    struct SimplePerformanceStats {
        int totalFrames = 0;
        int successFrames = 0;
        int roiFailures = 0;
        int spotFailures = 0;
        int pupilFailures = 0;
        int gazeFailures = 0;

        double getSuccessRate() const {
            return totalFrames > 0 ? (double)successFrames / totalFrames * 100.0 : 0.0;
        }

        void addFrame(bool success) {
            totalFrames++;
            if (success) successFrames++;
        }

        void reset() {
            totalFrames = 0;
            successFrames = 0;
            roiFailures = 0;
            spotFailures = 0;
            pupilFailures = 0;
            gazeFailures = 0;
        }
    } performanceStats;

    // === 🔧 当前帧数据结构 ===
    struct CurrentFrameData {
        int frameId = -1;
        cv::Mat originalImage;
        cv::Mat roiImage;
        cv::Mat processedImage;

        // ROI相关数据
        cv::Point darkestCenter;
        cv::Point adjustedDarkPoint;
        cv::Point roiPoint;
        cv::Rect roiRect;


        // 检测结果
        std::vector<Circle> lightSpots;
        std::vector<Circle> arrangedSpots;
        Oval pupilCircle;

        // 计算结果
        cv::Point2f gazePoint;
        bool gazeValid = false;

        void clear() {
            frameId = -1;
            originalImage.release();
            roiImage.release();
            lightSpots.clear();
            arrangedSpots.clear();
            gazeValid = false;
            darkestCenter = cv::Point(0, 0);
            adjustedDarkPoint = cv::Point(0, 0);
            roiPoint = cv::Point(0, 0);
            roiRect = cv::Rect(0, 0, 0, 0);
            gazePoint = cv::Point2f(0, 0);
        }
    } currentFrame;

    bool debugFlag = true;
    std::vector<MappingCoefficients> m_mappingCoefficients;
    MappingCoefficients combinedMappingCoefficients;
};

#endif // MERGEDPROCESSINGPIP_H
