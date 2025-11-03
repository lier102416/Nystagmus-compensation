#ifndef VIDEOCAPTUREPIP_H
#define VIDEOCAPTUREPIP_H


#include <opencv2/opencv.hpp>
#include <pipline.h>
#include <qlabel.h>
#include "QFileDialog"
#include <QMutex>
#include <QTimer>
#include <QElapsedTimer>
#include "sharedpipelinedate.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libavfilter/avfilter.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
}

class videoCapturePip: public QObject, public AbstractPipe
{
    Q_OBJECT
public:
    videoCapturePip():QObject(),AbstractPipe("videoCapturePipi", PIPE_SOURCE_E),
        sourceType(0), cameraIndex(0),
        m_width(1280), m_height(720), m_fps(60),
        m_isFrameReady(false), m_formatContext(nullptr), m_codecContext(nullptr),
        m_codec(nullptr), m_frame(nullptr), m_frameRGB(nullptr),
        m_packet(nullptr), m_swsContext(nullptr), m_buffer(nullptr),
        m_videoStreamIndex(-1), m_isopened(false)
    {
        avdevice_register_all();
        m_performanceTimer.start();
    }


    ~videoCapturePip(){
        resetSource();
    }

    void setSource(int type, const QVariant & source){
        sourceType = type;
        m_source = source;
    }

    // 设置分辨率
    void setResolution(int width, int height){
        m_width = width;
        m_height = height;
    }

    // 设置帧率
    void setFrameRate(double fps){
        m_fps = fps;
    }

    void resetSource(){
        cleanup();
        qDebug()<<"视频源清理完毕";
    }
    void closeCamera() {
        qDebug() << "开始关闭摄像头流程...";

        // 设置关闭标志，让 pipe 线程停止读取
        m_shouldClose = true;

        // 等待一小段时间，确保 readFrame 不再被调用
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // 现在安全地关闭资源
        QMutexLocker locker(&m_mutex);

        if (m_isopened) {
            qDebug() << "正在关闭" << (sourceType == 0 ? "摄像头" : "视频文件");

            // 标记为未打开状态
            m_isopened = false;

            // 刷新解码器缓冲区
            if (m_codecContext) {
                avcodec_send_packet(m_codecContext, nullptr);
                AVFrame* tmpFrame = av_frame_alloc();
                while (avcodec_receive_frame(m_codecContext, tmpFrame) == 0) {
                    // 清空解码器中的所有帧
                }
                av_frame_free(&tmpFrame);
            }

            // 释放解码相关资源
            if (m_swsContext) {
                sws_freeContext(m_swsContext);
                m_swsContext = nullptr;
            }

            if (m_buffer) {
                av_free(m_buffer);
                m_buffer = nullptr;
            }

            if (m_frameRGB) {
                av_frame_free(&m_frameRGB);
                m_frameRGB = nullptr;
            }

            if (m_frame) {
                av_frame_free(&m_frame);
                m_frame = nullptr;
            }

            if (m_packet) {
                av_packet_free(&m_packet);
                m_packet = nullptr;
            }

            if (m_codecContext) {
                avcodec_free_context(&m_codecContext);
                m_codecContext = nullptr;
            }

            if (m_formatContext) {
                // 对于摄像头，需要特殊处理
                if (sourceType == 0) {
                    // 先停止数据流
                    av_read_pause(m_formatContext);
                }
                avformat_close_input(&m_formatContext);
                m_formatContext = nullptr;
            }

            // 重置其他状态
            m_videoStreamIndex = -1;
            m_currentFrame.release();
            m_isFrameReady = false;

            qDebug() << (sourceType == 0 ? "摄像头" : "视频文件") << "资源已释放";
        }

        // 重置关闭标志
        m_shouldClose = false;

// Windows下额外等待，确保设备完全释放
#ifdef _WIN32
        if (sourceType == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
#endif

        qDebug() << "摄像头关闭流程完成";
    }



    bool isCameraOpened() const {
        QMutexLocker locker(&m_mutex);
        return m_isopened;
    }

    //先关闭当前摄像头，然后重新初始化

    bool reopenCamera() {
        closeCamera();

        // 等待一小段时间确保资源完全释放
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return initializeFFmpeg();
    }
    bool initializeFFmpeg(){
        QMutexLocker locker(&m_mutex);

        if(sourceType == 0) {
            // 摄像头模式 - MJPEG专用设置
            const AVInputFormat *inputFormat = av_find_input_format("dshow");
            if(!inputFormat){
                qDebug() << "找不到摄像头格式";
                return false;
            }

            QString deviceName = QString("video=%1").arg(m_source.toString());

            // MJPEG专用参数设置
            AVDictionary * options = nullptr;
            av_dict_set(&options, "video_size",
                        QString("%1x%2").arg(m_width).arg(m_height).toLocal8Bit().data(), 0);

            // 强制使用MJPEG，这是关键
            av_dict_set(&options, "vcodec", "mjpeg", 0);

            // MJPEG专用缓冲区设置 - 需要足够大来处理高质量JPEG帧
            av_dict_set(&options, "rtbufsize", "5M", 0);      // 减小缓冲区到5M
            av_dict_set(&options, "buffer_size", "2M", 0);    // 额外缓冲区控制
            av_dict_set(&options, "fflags", "+nobuffer+flush_packets", 0);  // 禁用内部缓冲
            av_dict_set(&options, "flags", "+low_delay", 0);  // 低延迟模式

            // 其他设置也调整为更激进的低延迟配置：
            av_dict_set(&options, "probesize", "1M", 0);           // 从10M减小到1M
            av_dict_set(&options, "analyzeduration", "500000", 0); // 从2秒减小到0.5秒
            av_dict_set(&options, "max_delay", "100000", 0);       // 从0.5秒减小到0.1秒

            qDebug() << "🔧 使用小缓冲区配置 - rtbufsize: 5M, buffer_size: 2M";


            qDebug() << "打开MJPEG摄像头" << deviceName;
            qDebug() << "参数：" << m_width << "x" << m_height << "@" << m_fps << "fps";

            int ret = avformat_open_input(&m_formatContext,
                                          deviceName.toLocal8Bit().data(),
                                          inputFormat,
                                          &options);
            av_dict_free(&options);

            if(ret < 0){
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                qDebug() << "无法打开摄像头:" << errbuf;
                cleanup();
                return false;
            }
        } else {
            // 文件模式保持不变
            QString filePath = m_source.toString();
            int ret = avformat_open_input(&m_formatContext, filePath.toLocal8Bit().data(), nullptr, nullptr);
            if(ret < 0){
                char errbuf[AV_ERROR_MAX_STRING_SIZE];
                av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
                qDebug() << "无法打开文件:" << errbuf;
                cleanup();
                return false;
            }
            qDebug() << "打开文件成功:" << filePath;
        }

        // 查找流信息
        int ret = avformat_find_stream_info(m_formatContext, nullptr);
        if(ret < 0){
            qDebug() << "无法获取流信息";
            cleanup();
            return false;
        }

        // 查找视频流
        m_videoStreamIndex = -1;
        for(unsigned int i = 0; i < m_formatContext->nb_streams; i++){
            if(m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
                m_videoStreamIndex = i;
                break;
            }
        }
        if(m_videoStreamIndex == -1){
            qDebug() << "未找到视频流";
            cleanup();
            return false;
        }

        // 获取解码器 - MJPEG专用设置
        AVCodecParameters *codecpar = m_formatContext->streams[m_videoStreamIndex]->codecpar;
        m_codec = (AVCodec *)avcodec_find_decoder(codecpar->codec_id);
        if(!m_codec){
            qDebug() << "未找到解码器";
            cleanup();
            return false;
        }

        // 创建解码器上下文
        m_codecContext = avcodec_alloc_context3(m_codec);
        if(!m_codecContext){
            qDebug() << "无法分配解码器上下文";
            cleanup();
            return false;
        }

        // 复制参数
        avcodec_parameters_to_context(m_codecContext, codecpar);

        // MJPEG解码器专用设置
        m_codecContext->thread_count = 4;  // 增加线程数处理MJPEG
        m_codecContext->thread_type = FF_THREAD_FRAME;  // 只用帧线程，避免片线程问题

        // MJPEG错误处理设置
        m_codecContext->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;  // 错误掩盖
        m_codecContext->skip_frame = AVDISCARD_DEFAULT;  // 不跳过帧
        m_codecContext->skip_idct = AVDISCARD_DEFAULT;   // 不跳过IDCT
        m_codecContext->skip_loop_filter = AVDISCARD_DEFAULT;  // 不跳过环路滤波

        // MJPEG错误处理设置（移除过时的标志）
        m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;     // 快速模式
        m_codecContext->flags2 |= AV_CODEC_FLAG2_SHOW_ALL; // 显示所有帧，包括损坏的

        // 设置错误容忍度
        m_codecContext->err_recognition = AV_EF_IGNORE_ERR; // 忽略错误继续解码

        ret = avcodec_open2(m_codecContext, m_codec, nullptr);
        if(ret < 0){
            qDebug() << "无法打开解码器";
            cleanup();
            return false;
        }

        // 分配帧和数据包
        m_frame = av_frame_alloc();
        m_frameRGB = av_frame_alloc();
        m_packet = av_packet_alloc();

        if(!m_frame || !m_frameRGB || !m_packet){
            qDebug() << "无法分配帧内存";
            cleanup();
            return false;
        }

        // 计算灰度帧缓冲区大小
        int numBytes = av_image_get_buffer_size(AV_PIX_FMT_GRAY8,
                                                m_codecContext->width,
                                                m_codecContext->height, 1);
        m_buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

        // 设置灰度帧数据指针
        av_image_fill_arrays(m_frameRGB->data, m_frameRGB->linesize, m_buffer,
                             AV_PIX_FMT_GRAY8, m_codecContext->width, m_codecContext->height, 1);

        // 初始化转换上下文 - MJPEG专用
        m_swsContext = sws_getContext(
            m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
            m_codecContext->width, m_codecContext->height, AV_PIX_FMT_GRAY8,
            SWS_BILINEAR | SWS_ACCURATE_RND,  // 高质量转换
            nullptr, nullptr, nullptr);

        // MJPEG色彩空间设置 - 专门处理YUVJ422P
        if (m_swsContext) {
            // MJPEG通常使用JPEG色彩空间，全范围YUV
            int srcRange = 1;  // JPEG使用全范围 (0-255)
            int dstRange = 1;  // 灰度输出也使用全范围

            // MJPEG通常使用BT.601色彩矩阵
            const int* coefs = sws_getCoefficients(SWS_CS_ITU601);

            int colorResult = sws_setColorspaceDetails(m_swsContext,
                                                       coefs, srcRange,
                                                       coefs, dstRange,
                                                       0, 1 << 16, 1 << 16);
            if (colorResult >= 0) {
                qDebug() << "MJPEG色彩空间设置成功";
            } else {
                qDebug() << "MJPEG色彩空间设置失败，使用默认";
            }
        }

        if(!m_swsContext){
            qDebug() << "无法初始化图像转换上下文";
            cleanup();
            return false;
        }

        m_isopened = true;
        qDebug() << "MJPEG初始化成功";

        AVRational frameRate = m_formatContext->streams[m_videoStreamIndex]->r_frame_rate;
        if(frameRate.den != 0) {
            double actualFrameRate = (double)frameRate.num / frameRate.den;
            qDebug() << "实际帧率" << actualFrameRate << "fps";
        }
        qDebug() << "像素格式" << av_get_pix_fmt_name(m_codecContext->pix_fmt);

        return true;
    }


    cv::Mat readFrame(){
        if (!m_isopened || m_shouldClose) {
            return cv::Mat();
        }

        if (!m_mutex.tryLock()) {
            return cv::Mat();
        }

        struct LockGuard {
            QMutex* mutex;
            LockGuard(QMutex* m) : mutex(m) {}
            ~LockGuard() { if (mutex) mutex->unlock(); }
        } lockGuard(&m_mutex);

        // 添加详细的时间测量
        QElapsedTimer totalTimer, readTimer, decodeTimer, convertTimer;
        totalTimer.start();

        // 统计变量（静态以便跨帧统计）
        static double totalReadTime = 0;
        static double totalDecodeTime = 0;
        static double totalConvertTime = 0;
        static int frameCount = 0;
        static int cacheHits = 0;  // 缓冲区命中次数

        int maxAttempts = (sourceType == 0) ? 20 : 5;
        bool isVideoFile = (sourceType == 1);

        for(int attempts = 0; attempts < maxAttempts; attempts++) {
            readTimer.start();
            int ret = av_read_frame(m_formatContext, m_packet);
            double readTime = readTimer.nsecsElapsed() / 1e6;

            // 判断是否从缓冲区读取（读取时间特别短）
            if (readTime < 0.5) {
                cacheHits++;
            }

            if(ret < 0){
                if(ret == AVERROR_EOF && isVideoFile){
                    qDebug() << "视频文件结束，重新开始播放";
                    avcodec_flush_buffers(m_codecContext);
                    int seekRet = avformat_seek_file(m_formatContext, -1,
                                                     INT64_MIN, 0, INT64_MAX,
                                                     AVSEEK_FLAG_BACKWARD);
                    if (seekRet < 0) {
                        QString filePath = m_source.toString();
                        cleanup();
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        if (!initializeFFmpeg()) {
                            return cv::Mat();
                        }
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    continue;
                } else if (ret == AVERROR(EAGAIN)) {
                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                    continue;
                } else {
                    if (attempts < maxAttempts - 1) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                        continue;
                    } else {
                        return cv::Mat();
                    }
                }
            }

            if(m_packet->stream_index != m_videoStreamIndex){
                av_packet_unref(m_packet);
                continue;
            }

            // 解码计时
            decodeTimer.start();
            ret = avcodec_send_packet(m_codecContext, m_packet);
            if(ret < 0){
                av_packet_unref(m_packet);
                if (ret == AVERROR(EAGAIN)) {
                    AVFrame* tempFrame = av_frame_alloc();
                    if (tempFrame) {
                        while (avcodec_receive_frame(m_codecContext, tempFrame) == 0) {
                        }
                        av_frame_free(&tempFrame);
                    }
                    continue;
                }
                continue;
            }

            ret = avcodec_receive_frame(m_codecContext, m_frame);
            double decodeTime = decodeTimer.nsecsElapsed() / 1e6;

            if(ret == 0){
                if (m_frame->width <= 0 || m_frame->height <= 0) {
                    av_packet_unref(m_packet);
                    continue;
                }

                // 转换计时
                convertTimer.start();
                int scaleResult = sws_scale(m_swsContext,
                                            (uint8_t const * const *)m_frame->data,
                                            m_frame->linesize,
                                            0,
                                            m_codecContext->height,
                                            m_frameRGB->data,
                                            m_frameRGB->linesize);

                double convertTime = convertTimer.nsecsElapsed() / 1e6;

                if (scaleResult <= 0) {
                    av_packet_unref(m_packet);
                    continue;
                }

                cv::Mat grayFrame(m_codecContext->height, m_codecContext->width, CV_8UC1,
                                  m_frameRGB->data[0], m_frameRGB->linesize[0]);

                if (grayFrame.empty()) {
                    av_packet_unref(m_packet);
                    continue;
                }

                // 统计
                totalReadTime += readTime;
                totalDecodeTime += decodeTime;
                totalConvertTime += convertTime;
                frameCount++;

                // 每100帧输出详细统计
                if (frameCount % 100 == 0) {
                    double avgRead = totalReadTime / frameCount;
                    double avgDecode = totalDecodeTime / frameCount;
                    double avgConvert = totalConvertTime / frameCount;
                    double cacheHitRate = (double)cacheHits / frameCount * 100;

                    // 重置统计
                    totalReadTime = 0;
                    totalDecodeTime = 0;
                    totalConvertTime = 0;
                    frameCount = 0;
                    cacheHits = 0;
                }

                cv::Mat result = grayFrame.clone();
                av_packet_unref(m_packet);

                double totalTime = totalTimer.nsecsElapsed() / 1e6;

                return result;
            }
            else if(ret == AVERROR(EAGAIN)) {
                av_packet_unref(m_packet);
                continue;
            }

            av_packet_unref(m_packet);
        }

        return cv::Mat();
    }


    void cleanup(){
        QMutexLocker locker(&m_mutex);
        if(m_swsContext){
            sws_freeContext(m_swsContext);
            m_swsContext = nullptr;
        }
        if(m_buffer){
            av_free(m_buffer);
            m_buffer = nullptr;
        }
        if(m_frameRGB){
            av_frame_free(&m_frameRGB);
        }
        if(m_frame){
            av_frame_free(&m_frame);
        }
        if(m_packet){
            av_packet_free(&m_packet);
        }
        if(m_codecContext){
            avcodec_free_context(&m_codecContext);
        }
        if(m_formatContext){
            avformat_close_input(&m_formatContext);
        }
        m_isopened = false;
    }

    void pipe(QSemaphore & inSem, QSemaphore & outSem){
        bool pauseLogShown = false;
        while(m_paused && !exit() && !m_shouldClose) {
            if (!pauseLogShown) {
                qDebug() << "管道启动时即处于暂停状态，开始缓冲区管理...";
                pauseLogShown = true;
            }

            // 即使在初始化之前也要尝试清理（如果摄像头已打开）
            handlePauseBufferManagement();
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        }

        if (pauseLogShown) {
            qDebug() << "🔧 管道从暂停状态恢复，继续正常流程";
        }

        if (m_shouldClose || exit()) {
            return;
        }

        if(!initializeFFmpeg()){
            qDebug() << "初始化失败";
            return;
        }

        static const cv::Rect roi(0, 0, 800, 720);
        FrameImage* pOutFrame = (FrameImage*)m_pOutImage;
        int frameId = 0;

        // 区分摄像头和视频文件的帧率控制参数
        bool isVideoFile = (sourceType == 1);
        bool isCamera = (sourceType == 0);

        const int FILE_TARGET_FPS = 60;
        const double FILE_INTERVAL_MS = 1000.0 / FILE_TARGET_FPS;
        auto lastFrameTime = std::chrono::high_resolution_clock::now();
        bool isFirstFrame = true;

        const int TARGET_FPS = 60;
        const double TARGET_INTERVAL_MS = 1000.0 / TARGET_FPS;
        auto startTime = std::chrono::high_resolution_clock::now();

        int maxFailures = isCamera ? 50 : 10;
        int consecutiveFailures = 0;
        int totalFrames = 0;
        int successfulFrames = 0;

        // 添加性能统计变量
        double totalProcessingTime = 0;
        double totalReadTime = 0;
        double totalWaitTime = 0;
        int statFrameCount = 0;

        qDebug() << "开始处理" << (isVideoFile ? "视频文件" : "摄像头")
                 << "目标帧率:" << (isVideoFile ? FILE_TARGET_FPS : TARGET_FPS) << "fps";

        while (!exit() && !m_shouldClose) {
            QElapsedTimer completeLoopTimer;
            completeLoopTimer.start();

            // 添加处理时间计时器
            QElapsedTimer processingTimer;
            double waitTimeMs = 0;

            if(m_isopened && !m_shouldClose){
                // 暂停处理逻辑
                bool wasPaused = false;
                while(m_paused && !exit() && !m_shouldClose) {
                    if (!wasPaused) {
                        wasPaused = true;
                    }

                    // 暂停期间积极清理缓冲区
                    handlePauseBufferManagement();

                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                if (wasPaused) {
                    // 重置时间控制和统计数据
                    if (isVideoFile) {
                        lastFrameTime = std::chrono::high_resolution_clock::now();
                        isFirstFrame = true;
                    } else {
                        startTime = std::chrono::high_resolution_clock::now();
                    }
                    totalFrames = 0;
                    successfulFrames = 0;
                    consecutiveFailures = 0;

                    // 重置性能统计
                    totalProcessingTime = 0;
                    totalReadTime = 0;
                    totalWaitTime = 0;
                    statFrameCount = 0;
                }

                if(exit()) break;

                // 帧率控制逻辑 - 记录等待时间
                QElapsedTimer waitTimer;
                waitTimer.start();

                if (isVideoFile) {
                    if (!isFirstFrame) {
                        auto currentTime = std::chrono::high_resolution_clock::now();
                        auto timeSinceLastFrame = std::chrono::duration_cast<std::chrono::microseconds>(
                            currentTime - lastFrameTime);

                        double elapsedMs = timeSinceLastFrame.count() / 1000.0;
                        if (elapsedMs < FILE_INTERVAL_MS) {
                            waitTimeMs = FILE_INTERVAL_MS - elapsedMs;
                            std::this_thread::sleep_for(std::chrono::microseconds((int)(waitTimeMs * 1000)));
                        }
                    } else {
                        isFirstFrame = false;
                    }
                } else if (isCamera) {
                    auto currentTime = std::chrono::high_resolution_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                        currentTime - startTime);

                    int expectedFrames = (elapsed.count() / 1000.0) / TARGET_INTERVAL_MS;

                    if (totalFrames >= expectedFrames) {
                        waitTimeMs = TARGET_INTERVAL_MS - (elapsed.count() / 1000.0 - expectedFrames * TARGET_INTERVAL_MS);
                        if (waitTimeMs > 0) {
                            std::this_thread::sleep_for(std::chrono::microseconds((int)(waitTimeMs * 1000)));
                        }
                    }
                }

                double actualWaitTime = waitTimer.nsecsElapsed() / 1e6;
                totalWaitTime += actualWaitTime;

                // 开始实际处理计时
                processingTimer.start();

                // 读取帧
                QElapsedTimer readTimer;
                readTimer.start();
                cv::Mat src = readFrame();

                double readTime = readTimer.nsecsElapsed() / 1e6;
                totalReadTime += readTime;

                totalFrames++;

                if(src.empty()){
                    consecutiveFailures++;

                    if (isVideoFile) {
                        qDebug() << "视频文件结束，重新开始播放";
                        lastFrameTime = std::chrono::high_resolution_clock::now();
                        isFirstFrame = true;
                        consecutiveFailures = 0;
                        continue;
                    }

                    if (consecutiveFailures >= maxFailures) {
                        qDebug() << "连续失败过多，重新初始化";
                        cleanup();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                        if (!initializeFFmpeg()) {
                            qDebug() << "重新初始化失败，退出";
                            break;
                        }

                        consecutiveFailures = 0;
                        totalFrames = 0;
                        successfulFrames = 0;

                        if (isCamera) {
                            startTime = std::chrono::high_resolution_clock::now();
                        } else {
                            lastFrameTime = std::chrono::high_resolution_clock::now();
                            isFirstFrame = true;
                        }
                        continue;
                    }

                    std::this_thread::sleep_for(std::chrono::microseconds(500));
                    continue;
                }

                // 成功处理帧
                consecutiveFailures = 0;
                successfulFrames++;

                frameId = SharedPipelineData::generateFrameId();

                // ROI处理
                cv::Mat roiFrame;
                // if(src.cols > roi.x + roi.width && src.rows > roi.y + roi.height){
                    roiFrame = src(roi);
                // } else {
                //     qDebug()<<"输出原图";
                //     roiFrame = src;
                // }

                SharedPipelineData::createFrameData(frameId, roiFrame);
                pOutFrame->image = roiFrame.clone();
                pOutFrame->frameId = frameId;

                // 更新时间记录
                if (isVideoFile) {
                    lastFrameTime = std::chrono::high_resolution_clock::now();
                }

                // 计算实际处理时间（不包括等待）
                double actualProcessingTime = processingTimer.nsecsElapsed() / 1e6;
                totalProcessingTime += actualProcessingTime;
                statFrameCount++;

                // 完整循环时间
                double completeMs = completeLoopTimer.nsecsElapsed() / 1e6;

                // 性能统计 - 每60帧输出一次
                if (frameId % 60 == 0 && statFrameCount > 0) {
                    double avgProcessingTime = totalProcessingTime / statFrameCount;
                    double avgReadTime = totalReadTime / statFrameCount;
                    double avgWaitTime = totalWaitTime / statFrameCount;
                    double avgCompleteTime = avgProcessingTime + avgWaitTime;

                    qDebug() << QString("%1帧 %2 性能统计:").arg(isVideoFile ? "文件" : "摄像头").arg(frameId);
                    qDebug() << QString("  - 平均读取时间: %1 ms").arg(avgReadTime, 0, 'f', 2);
                    qDebug() << QString("  - 平均处理时间: %1 ms (不含等待)").arg(avgProcessingTime, 0, 'f', 2);
                    qDebug() << QString("  - 平均等待时间: %1 ms").arg(avgWaitTime, 0, 'f', 2);
                    qDebug() << QString("  - 平均总循环时间: %1 ms").arg(avgCompleteTime, 0, 'f', 2);
                    qDebug() << QString("  - 实际处理FPS: %1").arg(1000.0 / avgProcessingTime, 0, 'f', 1);
                    qDebug() << QString("  - 输出FPS: %1").arg(1000.0 / avgCompleteTime, 0, 'f', 1);
                    qDebug() << QString("  - 成功率: %1%").arg((double)successfulFrames/totalFrames*100, 0, 'f', 1);

                    // 重置统计
                    totalProcessingTime = 0;
                    totalReadTime = 0;
                    totalWaitTime = 0;
                    statFrameCount = 0;
                }

                // 保存实际处理时间（不包括等待）到SharedPipelineData
                SharedPipelineData::setTime(frameId, 1, actualProcessingTime);

                outSem.release();
                sendOverSign(frameId);

            } else {
                if (m_shouldClose) break;
                qDebug() << "视频源未打开";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        if (!m_shouldClose) {
            resetSource();
        }
    }

    void forceCloseCamera() {

        // 立即设置所有标志
        m_shouldClose = true;
        m_isopened = false;

        // 不等待，直接清理
        cleanup();

// Windows下等待更长时间
#ifdef _WIN32
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#endif

        m_shouldClose = false;
        qDebug() << "强制关闭完成";
    }
signals:
    void sendOverSign(int frameId);

private:
    int sourceType;  // 0: 摄像头 1:文件
    int cameraIndex; // 摄像头索引
    std::string filePath; // 选到的文件
    QVariant m_source;
    std::atomic<bool> m_shouldClose{false};  // 添加关闭标志

    int m_width;  // 分辨率宽度
    int m_height; // 分辨率高度
    double m_fps; // 帧率

    // 帧同步相关
    cv::Mat m_currentFrame;
    bool m_isFrameReady;
    QMutex m_frameMutex;

    // FFmpeg相关
    AVFormatContext *m_formatContext;
    AVCodecContext  *m_codecContext;
    int m_videoStreamIndex;
    AVCodec *m_codec;
    AVFrame *m_frame;
    AVFrame *m_frameRGB;
    AVPacket *m_packet;
    SwsContext *m_swsContext;
    uint8_t *m_buffer;
    bool m_isopened;

    // 线程安全
    mutable  QMutex m_mutex;

    // 性能监控
    QElapsedTimer m_performanceTimer;


    void handlePauseBufferManagement(){
        static int clearCount = 0;
        static auto lastClearTime = std::chrono::high_resolution_clock::now();

        auto now = std::chrono::high_resolution_clock::now();
        auto timeSinceLastClear = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastClearTime);

        // 每25ms清理一次缓冲区，比较频繁
        if (timeSinceLastClear.count() >= 25) {
            if (m_isopened && m_formatContext) {
                // 🔧 关键：连续读取多帧快速清空缓冲区
                for (int i = 0; i < 5; ++i) {  // 一次读取最多5帧
                    cv::Mat discardFrame = readFrame();
                    if (discardFrame.empty()) {
                        break; // 缓冲区已空，退出循环
                    }
                    // 立即丢弃帧，不做任何处理
                }

                clearCount++;
                // 每隔一段时间输出一次调试信息，避免日志刷屏
                if (clearCount % 40 == 0) {
                    qDebug() << "🔧 缓冲区清理进行中... 已清理" << clearCount << "次";
                }
            }
            lastClearTime = now;
        }
    };
};

#endif // VIDEOCAPTURE_H
