#ifndef PUPILEXTRACTIONPIP_H
#define PUPILEXTRACTIONPIP_H

#include "pipline.h"
#include "class.h"
#include "pupiletraction.h"
#include "sharedpipelinedate.h"

class pupilExtractionPip : public QObject, public AbstractPipe {
    Q_OBJECT
public:
    pupilExtractionPip() : QObject(), AbstractPipe("PupilPipe", PIPE_PROCESS_E) {};

    void pipe(QSemaphore& inSem, QSemaphore& outSem) {
        FrameImage* pInFrame = (FrameImage*)m_pInImage;
        FrameImage* pOutFrame = (FrameImage*)m_pOutImage;

        while (!exit()) {
            inSem.acquire();
            QElapsedTimer totalTimer, stepTimer;
            totalTimer.start();
            int frameId;
            if (pInFrame && !pInFrame->image.empty()) {
                // ========== 1. 图像克隆 ==========
                stepTimer.start();
                cv::Mat src = pInFrame->image.clone();
                frameId = pInFrame->frameId;
                double cloneTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 2. 瞳孔检测 ==========
                stepTimer.restart();
                Oval pupilCircle;
                bool resultFlag = pupilExtraction.pupilDetection(src, pupilCircle, pInFrame->frameId);

                double pupilDetectionTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 5. 获取帧数据和坐标调整 ==========
                stepTimer.restart();
                FrameData frameData;
                double coordinateAdjustTime = 0.0;
                double frameDataTime = 0.0;
                double dataSaveTime = 0.0;

                if (SharedPipelineData::getFrameData(frameId, frameData)) {
                    frameDataTime = stepTimer.nsecsElapsed() / 1e6;

                    stepTimer.restart();
                    pupilCircle.center.x += frameData.roiPoint.x;
                    pupilCircle.center.y += frameData.roiPoint.y;
                    coordinateAdjustTime = stepTimer.nsecsElapsed() / 1e6;

                    // ========== 7. 数据保存 ==========
                    stepTimer.restart();
                    SharedPipelineData::setPupilCircle(frameId, pupilCircle);
                    dataSaveTime = stepTimer.nsecsElapsed() / 1e6;
                }
                // ========== 8. 图像传递 ==========
                stepTimer.restart();
                pOutFrame->image = src;
                pOutFrame->frameId = frameId;
                double imageTransferTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 9. 完成检查 ==========
                stepTimer.restart();
                bool frameComplete = SharedPipelineData::isFrameComplete(frameId);
                double completionCheckTime = stepTimer.nsecsElapsed() / 1e6;
                if (frameComplete) {

                }
                else{
                    qDebug()<<"frameComplete失败 id:"<<frameData.frameId;
                }
                // ========== 10. 时间记录 ==========
                stepTimer.restart();
                qint64 ns = totalTimer.nsecsElapsed();
                double totalMs = ns / 1e6;
                SharedPipelineData::setTime(frameId, 4, totalMs);
                double recordingTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 性能报告输出 ==========
                static int frameCount = 0;
                frameCount++;

            }
            outSem.release();
            sendOverSign(frameId);

        }
    }

    float calculatePupilArea(const Oval& pupil) {
        if (pupil.size.width <= 0 || pupil.size.height <= 0) {
            return 0.0f;
        }
        // 椭圆面积公式: π * a * b (a和b是半长轴和半短轴)
        float area = CV_PI * (pupil.size.width / 2.0f) * (pupil.size.height / 2.0f);
        return area;
    }

signals:
    void sendOverSign(int frameId);

private:
    QElapsedTimer QEtimer;
    PupilEtraction pupilExtraction;
    bool debugFlag = 0;

};

#endif // PUPILEXTRACTIONPIP_H
