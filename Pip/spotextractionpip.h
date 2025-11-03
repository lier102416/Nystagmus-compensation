#ifndef SPOTEXTRACTIONPIP_H
#define SPOTEXTRACTIONPIP_H

#include "pipline.h"
#include "class.h"
#include "spotextraction.h"
#include "sharedpipelinedate.h"
#include "smartspotprocessor.h"

class SpotExtractionPip:public QObject, public AbstractPipe
{
    Q_OBJECT
public:
    SpotExtractionPip():  QObject(),AbstractPipe("SpotPipe", PIPE_PROCESS_E){};
    void pipe(QSemaphore & inSem, QSemaphore & outSem){
        FrameImage* pInFrame = (FrameImage*)m_pInImage;
        FrameImage* pOutFrame = (FrameImage*)m_pOutImage;

        while(!exit()) {
            inSem.acquire();
            QElapsedTimer totalTimer, stepTimer;
            totalTimer.start();
            int frameId;
            if(pInFrame && !pInFrame->image.empty()) {
                // ========== 1. 图像克隆 ==========
                stepTimer.start();
                cv::Mat src = pInFrame->image.clone();
                frameId = pInFrame->frameId;
                double cloneTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 2. 图像预处理 ==========
                cv::Mat blur, outPutLightImage;
                stepTimer.restart();
                cv::normalize(src, src, 0, 255, cv::NORM_MINMAX);
                double normalizeTime = stepTimer.nsecsElapsed() / 1e6;

                // 2.2 高斯模糊
                stepTimer.restart();
                cv::GaussianBlur(src, blur, cv::Size(5, 5), 0);
                double blurTime = stepTimer.nsecsElapsed() / 1e6;

                // 2.3 二值化
                stepTimer.restart();
                cv::threshold(blur, outPutLightImage, 220, 255, cv::THRESH_BINARY);
                double thresholdTime1 = stepTimer.nsecsElapsed() / 1e6;

                // ========== 3. 获取帧数据 ==========
                stepTimer.restart();
                FrameData frameData;
                bool hasFrameData = SharedPipelineData::getFrameData(frameId, frameData);
                double getFrameDataTime = stepTimer.nsecsElapsed() / 1e6;

                cv::Mat processedBlur;
                double lightDetectionTime = 0.0;
                double spotProcessingTime = 0.0;
                double coordinateAdjustTime = 0.0;
                double spotArrangementTime = 0.0;
                double dataStorageTime = 0.0;
                double cloneTime2 = 0.0;

                if(hasFrameData) {
                    // ========== 4. 光斑检测 ==========
                    stepTimer.restart();
                    std::vector<Circle> lightSpots = spotExtraction.lightExpection(outPutLightImage, frameData.darkPoint);
                    lightDetectionTime = stepTimer.nsecsElapsed() / 1e6;

                    // ========== 5. 图像克隆2 ==========
                    stepTimer.restart();
                    processedBlur = blur.clone();
                    cloneTime2 = stepTimer.nsecsElapsed() / 1e6;

                    // ========== 6. 光斑处理 ==========
                    stepTimer.restart();
                    spotProcessor.processLightSpots(processedBlur, lightSpots,
                                                    cv::Point2f(frameData.darkPoint.x, frameData.darkPoint.y), 30);
                    spotProcessingTime = stepTimer.nsecsElapsed() / 1e6;

                    stepTimer.restart();
                    for(auto& spot : lightSpots) {
                        spot.center.x += frameData.roiPoint.x;
                        spot.center.y += frameData.roiPoint.y;
                    }
                    coordinateAdjustTime = stepTimer.nsecsElapsed() / 1e6;

                    // ========== 8. 光斑补全 ==========
                    stepTimer.restart();
                    std::vector<Circle> result;
                    bool flag = spotExtraction.arrangeSpots(lightSpots, result);
                    if(!flag){
                        qDebug()<<"光斑检测失败"<<frameData.frameId;
                        SharedPipelineData::setDisplayFlag(frameId, flag);
                    }
                    spotArrangementTime = stepTimer.nsecsElapsed() / 1e6;
                    // ========== 9. 数据存储 ==========
                    stepTimer.restart();
                    SharedPipelineData::setLightPoints(frameId, result);
                    dataStorageTime = stepTimer.nsecsElapsed() / 1e6;
                }

                // ========== 10. 最终处理 ==========
                stepTimer.restart();
                if(hasFrameData && !processedBlur.empty()) {
                    cv::threshold(processedBlur, pOutFrame->image, 100, 255, cv::THRESH_BINARY);
                } else {
                    // 容错：使用原始模糊图像
                    cv::threshold(blur, pOutFrame->image, 100, 255, cv::THRESH_BINARY);
                    qDebug() << "Frame" << frameId << "FrameData failed, using fallback processing";
                }
                pOutFrame->frameId = frameId;
                double finalProcessingTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 11. 时间记录 ==========
                stepTimer.restart();
                qint64 ns = totalTimer.nsecsElapsed();
                double totalMs = ns / 1e6;
                SharedPipelineData::setTime(frameId, 3, totalMs);
                double recordingTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 性能报告输出 ==========
                static int frameCount = 0;
                frameCount++;

            }
            outSem.release();
            sendOverSign(frameId);
        }

    }
signals:
    void sendOverSign(int frameId);
private:
    QElapsedTimer QEtimer;
    SpotExtraction spotExtraction;
    int maxSpotsCount;
    int minRequiredSpots = 4; // 期望找到4个光斑
    SmartSpotProcessor spotProcessor; // 光斑处理器
    bool debugFlag = 0;
};

#endif // SPOTEXTRACTIONPIP_H
