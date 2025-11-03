#ifndef ROLEXTRACTIONPIP_H
#define ROLEXTRACTIONPIP_H

#include "pipline.h"
#include "class.h"
#include "rolextraction.h"
#include "sharedpipelinedate.h"

class rolExtractionPip : public QObject, public AbstractPipe {
    Q_OBJECT
public:
    rolExtractionPip() : QObject(), AbstractPipe("RolPipe", PIPE_PROCESS_E) {};

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

                // ========== 2. 最暗区域检测 ==========
                stepTimer.restart();
                cv::Point darkestCenter = rolExtraction.getDarkestArea(src);
                double darkestDetectionTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 3. ROI区域创建 ==========
                stepTimer.restart();
                cv::Rect IrisRol = rolExtraction.createIrisRol(src, darkestCenter);
                double roiCreationTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 4. ROI点保存 ==========
                stepTimer.restart();
                cv::Point roiPoint(IrisRol.x, IrisRol.y);
                SharedPipelineData::setRoiPoint(frameId, roiPoint);
                double roiSaveTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 5. 暗点坐标调整 ==========
                stepTimer.restart();
                cv::Point originalDarkest = darkestCenter;
                darkestCenter.x -= (IrisRol.x - 30);
                darkestCenter.y -= (IrisRol.y - 30);
                SharedPipelineData::setDarkPoint(frameId, darkestCenter);
                double coordinateAdjustTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 6. ROI图像提取 ==========
                stepTimer.restart();
                cv::Mat rolImage;
                rolExtraction.rolProcessImage(src, IrisRol, rolImage);
                double roiExtractionTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 7. 图像传递 ==========
                stepTimer.restart();
                pOutFrame->image = rolImage;
                pOutFrame->frameId = frameId;
                double imageTransferTime = stepTimer.nsecsElapsed() / 1e6;

                // ========== 8. 时间记录 ==========
                stepTimer.restart();
                qint64 ns = totalTimer.nsecsElapsed();
                double totalMs = ns / 1e6;
                SharedPipelineData::setTime(frameId, 2, totalMs);
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
    RolExtraction rolExtraction;
    int expectedIrisRadius = 250;
    int xStart;
    int yStart;
    bool debugFlag = 0;

};

#endif // ROLEXTRACTIONPIP_H
