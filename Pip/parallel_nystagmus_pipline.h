#ifndef PARALLEL_NYSTAGMUS_PIPLINE_H
#define PARALLEL_NYSTAGMUS_PIPLINE_H

#include <opencv2/opencv.hpp>
#include <deque>
#include <chrono>
#include <cmath>
#include <sstream>
#include <numeric>
#include <algorithm>
#include <memory>
#include <map>
#include <vector>
#include <QDebug>
#include "eigen-3.4.0/Eigen/Dense"

/**
 * ⭐ 并行眼震预测管道 - 实现真正的预测功能
 * 版本: 4.0 - 完全分离滤波与预测
 *
 * 核心特性:
 * - 真正的时间预测（非补偿滤波）
 * - 滤波与预测完全分离
 * - 多步预测轨迹
 * - 预测不确定性量化
 * - 实时预测性能评估
 * - 并行处理管道
 */
class ParallelNystagmusPipeline {
private:
    // ⭐ 增强型1D UKF滤波器 - 支持真正的预测
    class EnhancedXAxisUKF {
    private:
        // UKF参数
        static const int STATE_DIM = 4;  // 状态：[x, vx, ax, jx] 增加加加速度
        static const int MEAS_DIM = 1;   // 测量：[x]

        // Sigma点参数 - 优化的初始值
        double alpha = 0.001;   // 更小的alpha提高数值稳定性
        double beta = 2.0;      // 高斯分布优化
        double kappa = 3 - STATE_DIM;  // 标准设置

        // 状态和协方差
        Eigen::VectorXd state;
        Eigen::MatrixXd P;     // 状态协方差
        Eigen::MatrixXd Q;     // 过程噪声协方差
        Eigen::MatrixXd R;     // 测量噪声协方差

        // UKF权重
        Eigen::VectorXd Wm;    // 均值权重
        Eigen::VectorXd Wc;    // 协方差权重
        double lambda;

        bool initialized;
        float lastX;
        std::deque<float> velocityHistory;
        std::deque<float> measurementHistory;
        std::deque<float> positionHistory;
        std::deque<float> accelerationHistory;
        static const int HISTORY_SIZE = 20;

        // ⭐ 多尺度峰值检测器
        struct MultiScalePeakDetector {
            std::deque<float> positions;
            std::deque<float> velocities;
            std::deque<float> accelerations;
            static const int WINDOW_SIZE = 15;

            bool isPeak = false;
            bool isApproachingPeak = false;
            float peakConfidence = 0.0;
            int peakType = 0; // 0: none, 1: max, -1: min

            void update(float pos, float vel, float acc) {
                positions.push_back(pos);
                velocities.push_back(vel);
                accelerations.push_back(acc);

                while (positions.size() > WINDOW_SIZE) {
                    positions.pop_front();
                    velocities.pop_front();
                    accelerations.pop_front();
                }

                detectPeak();
            }

            void detectPeak() {
                if (positions.size() < 7) return;

                isPeak = false;
                isApproachingPeak = false;
                peakConfidence = 0.0;

                // 多尺度检测
                bool peak3 = detectPeakAtScale(3);
                bool peak5 = detectPeakAtScale(5);
                bool peak7 = detectPeakAtScale(7);

                // 速度零交叉检测
                bool velocityZeroCross = false;
                if (velocities.size() >= 3) {
                    size_t n = velocities.size();
                    float v1 = velocities[n-3];
                    float v2 = velocities[n-2];
                    float v3 = velocities[n-1];

                    velocityZeroCross = (v1 * v3 < 0) || (std::abs(v2) < 5.0 && std::abs(v3) < 10.0);

                    // 接近峰值检测
                    if (std::abs(v3) < std::abs(v2) && std::abs(v3) < 20.0 && std::abs(accelerations.back()) > 150.0) {
                        isApproachingPeak = true;
                    }
                }

                // 综合判断
                int peakVotes = (peak3 ? 1 : 0) + (peak5 ? 1 : 0) + (peak7 ? 1 : 0) + (velocityZeroCross ? 2 : 0);

                if (peakVotes >= 2) {
                    isPeak = true;
                    peakConfidence = peakVotes / 5.0f;

                    // 确定峰值类型
                    size_t n = positions.size();
                    float avgBefore = (positions[n-4] + positions[n-3]) / 2.0f;
                    float current = positions[n-2];
                    float avgAfter = (positions[n-1]) / 1.0f;

                    if (current > avgBefore && current > avgAfter) {
                        peakType = 1;  // 最大值
                    } else if (current < avgBefore && current < avgAfter) {
                        peakType = -1; // 最小值
                    }
                }
            }

            bool detectPeakAtScale(int scale) {
                if (positions.size() < scale) return false;

                size_t n = positions.size();
                size_t mid = n - scale/2 - 1;

                float centerVal = positions[mid];
                bool isLocalMax = true;
                bool isLocalMin = true;

                for (int i = 0; i < scale; i++) {
                    if (i == scale/2) continue;
                    float val = positions[n - scale + i];
                    if (val >= centerVal) isLocalMax = false;
                    if (val <= centerVal) isLocalMin = false;
                }

                return isLocalMax || isLocalMin;
            }

            void reset() {
                positions.clear();
                velocities.clear();
                accelerations.clear();
                isPeak = false;
                isApproachingPeak = false;
                peakConfidence = 0.0;
                peakType = 0;
            }
        } peakDetector;

        // ⭐ 增强的眼震检测器
        struct EnhancedNystagmusDetector {
            bool isNystagmus = false;
            double frequency = 0;
            double amplitude = 0;
            double phase = 0;
            int directionChanges = 0;
            double lastDirection = 0;
            std::deque<double> velocities;
            std::deque<double> positions;
            std::deque<double> timestamps;
            static const int WINDOW_SIZE = 30;

            // 周期性参数
            double estimatedPeriod = 0;
            double periodConfidence = 0;
            cv::Point2f lastPeakPos;
            double lastPeakTime = 0;
            std::deque<double> peakIntervals;

            void update(double position, double velocity, double timestamp) {
                positions.push_back(position);
                velocities.push_back(velocity);
                timestamps.push_back(timestamp);

                while (positions.size() > WINDOW_SIZE) {
                    positions.pop_front();
                    velocities.pop_front();
                    timestamps.pop_front();
                }

                if (velocities.size() < 10) return;

                // 方向变化检测
                double currentDir = velocity > 0 ? 1 : -1;
                if (std::abs(lastDirection - currentDir) > 1.5 && std::abs(velocity) > 15.0) {
                    directionChanges++;

                    // 记录峰值间隔
                    if (lastPeakTime > 0) {
                        double interval = timestamp - lastPeakTime;
                        peakIntervals.push_back(interval);
                        if (peakIntervals.size() > 10) {
                            peakIntervals.pop_front();
                        }

                        // 估计周期
                        if (peakIntervals.size() >= 3) {
                            estimatedPeriod = std::accumulate(peakIntervals.begin(),
                                                              peakIntervals.end(), 0.0) / peakIntervals.size();

                            // 计算周期稳定性
                            double variance = 0;
                            for (double interval : peakIntervals) {
                                variance += (interval - estimatedPeriod) * (interval - estimatedPeriod);
                            }
                            variance /= peakIntervals.size();
                            periodConfidence = 1.0 / (1.0 + std::sqrt(variance) / estimatedPeriod);
                        }
                    }
                    lastPeakTime = timestamp;
                }
                lastDirection = currentDir;

                // 频率和振幅计算
                if (timestamps.size() >= 15) {
                    double timeSpan = timestamps.back() - timestamps.front();
                    frequency = directionChanges / (2.0 * timeSpan);

                    // 振幅计算
                    double maxPos = *std::max_element(positions.begin(), positions.end());
                    double minPos = *std::min_element(positions.begin(), positions.end());
                    amplitude = (maxPos - minPos) / 2.0;

                    // 速度方差
                    double mean = 0, variance = 0;
                    for (double v : velocities) {
                        mean += std::abs(v);
                    }
                    mean /= velocities.size();

                    for (double v : velocities) {
                        variance += (std::abs(v) - mean) * (std::abs(v) - mean);
                    }
                    variance /= velocities.size();

                    // 眼震判断 - 更精确的条件
                    isNystagmus = (frequency > 0.5 && frequency < 6.0 &&
                                   variance > 100.0 && amplitude > 20.0);
                }
            }

            double predictNextPeakTime(double currentTime) {
                if (periodConfidence > 0.7 && estimatedPeriod > 0) {
                    return lastPeakTime + estimatedPeriod;
                }
                return -1;
            }

            void reset() {
                isNystagmus = false;
                frequency = 0;
                amplitude = 0;
                phase = 0;
                directionChanges = 0;
                lastDirection = 0;
                velocities.clear();
                positions.clear();
                timestamps.clear();
                estimatedPeriod = 0;
                periodConfidence = 0;
                lastPeakTime = 0;
                peakIntervals.clear();
            }
        } nystagmusDetector;

        // ⭐ 运动模式识别器
        struct MotionPatternRecognizer {
            enum MotionType {
                STABLE = 0,
                SMOOTH_PURSUIT = 1,
                SACCADE = 2,
                NYSTAGMUS = 3,
                TRANSITION = 4
            };

            MotionType currentType = STABLE;
            float confidence = 0;
            std::deque<float> velocityWindow;
            std::deque<float> accelerationWindow;
            static const int WINDOW_SIZE = 10;

            MotionType detectPattern(float velocity, float acceleration) {
                velocityWindow.push_back(velocity);
                accelerationWindow.push_back(acceleration);

                while (velocityWindow.size() > WINDOW_SIZE) {
                    velocityWindow.pop_front();
                    accelerationWindow.pop_front();
                }

                if (velocityWindow.size() < 5) return STABLE;

                // 计算特征
                float avgVel = std::accumulate(velocityWindow.begin(), velocityWindow.end(), 0.0f) / velocityWindow.size();
                float maxVel = *std::max_element(velocityWindow.begin(), velocityWindow.end());
                float minVel = *std::min_element(velocityWindow.begin(), velocityWindow.end());
                float velRange = maxVel - minVel;

                float avgAcc = std::accumulate(accelerationWindow.begin(), accelerationWindow.end(), 0.0f) / accelerationWindow.size();
                float maxAcc = *std::max_element(accelerationWindow.begin(), accelerationWindow.end());

                // 模式识别
                if (std::abs(avgVel) < 10.0 && velRange < 20.0) {
                    currentType = STABLE;
                    confidence = 1.0 - velRange / 20.0;
                } else if (std::abs(avgVel) < 50.0 && velRange < 40.0) {
                    currentType = SMOOTH_PURSUIT;
                    confidence = 1.0 - velRange / 40.0;
                } else if (std::abs(maxAcc) > 500.0 && std::abs(maxVel) > 100.0) {
                    currentType = SACCADE;
                    confidence = std::min(1.0f, std::abs(maxAcc) / 1000.0f);
                } else {
                    currentType = TRANSITION;
                    confidence = 0.5;
                }

                return currentType;
            }

            void reset() {
                currentType = STABLE;
                confidence = 0;
                velocityWindow.clear();
                accelerationWindow.clear();
            }
        } motionPattern;

        // 采样间隔
        double dt = 1.0 / 60.0;

        // 时间戳管理
        double currentTimestamp = 0;

        // ⭐ 新增：预测模式标志
        bool isPredictingFuture = false;

    public:
        EnhancedXAxisUKF() : initialized(false), lastX(0), currentTimestamp(0) {
            // 初始化状态向量
            state = Eigen::VectorXd::Zero(STATE_DIM);

            // 初始化协方差矩阵 - 优化的值
            P = Eigen::MatrixXd::Identity(STATE_DIM, STATE_DIM);
            P(0, 0) = 25.0;    // 位置不确定性
            P(1, 1) = 100.0;   // 速度不确定性
            P(2, 2) = 400.0;   // 加速度不确定性
            P(3, 3) = 1600.0;  // 加加速度不确定性

            // 基础过程噪声 - 优化的值
            Q = Eigen::MatrixXd::Zero(STATE_DIM, STATE_DIM);
            Q(0, 0) = 0.5;     // 位置过程噪声
            Q(1, 1) = 10.0;    // 速度过程噪声
            Q(2, 2) = 50.0;    // 加速度过程噪声
            Q(3, 3) = 200.0;   // 加加速度过程噪声

            // 测量噪声
            R = Eigen::MatrixXd::Identity(MEAS_DIM, MEAS_DIM) * 8.0;

            // 计算UKF参数
            lambda = alpha * alpha * (STATE_DIM + kappa) - STATE_DIM;
            initializeWeights();
        }

        void initializeWeights() {
            int n_sigma = 2 * STATE_DIM + 1;
            Wm = Eigen::VectorXd(n_sigma);
            Wc = Eigen::VectorXd(n_sigma);

            Wm(0) = lambda / (STATE_DIM + lambda);
            Wc(0) = lambda / (STATE_DIM + lambda) + (1 - alpha * alpha + beta);

            for (int i = 1; i < n_sigma; i++) {
                Wm(i) = 0.5 / (STATE_DIM + lambda);
                Wc(i) = 0.5 / (STATE_DIM + lambda);
            }
        }

        // ⭐ 改进的Sigma点生成
        Eigen::MatrixXd generateSigmaPoints(const Eigen::VectorXd& x, const Eigen::MatrixXd& P_in) {
            int n = x.size();
            int n_sigma = 2 * n + 1;
            Eigen::MatrixXd sigma_points(n, n_sigma);

            sigma_points.col(0) = x;

            // 数值稳定性增强
            Eigen::MatrixXd P_stable = P_in;
            P_stable = (P_stable + P_stable.transpose()) / 2.0;

            // 添加正则化项
            double regularization = 1e-9;
            P_stable += regularization * Eigen::MatrixXd::Identity(n, n);

            try {
                Eigen::LLT<Eigen::MatrixXd> llt((n + lambda) * P_stable);

                if (llt.info() == Eigen::Success) {
                    Eigen::MatrixXd A = llt.matrixL();

                    for (int i = 0; i < n; i++) {
                        sigma_points.col(i + 1) = x + A.col(i);
                        sigma_points.col(i + n + 1) = x - A.col(i);
                    }
                } else {
                    // 改进的SVD分解
                    Eigen::JacobiSVD<Eigen::MatrixXd> svd(P_stable, Eigen::ComputeFullU | Eigen::ComputeFullV);
                    Eigen::VectorXd s = svd.singularValues();

                    // 确保所有奇异值为正
                    for (int i = 0; i < s.size(); i++) {
                        s(i) = std::max(1e-9, s(i));
                    }

                    Eigen::MatrixXd S = s.asDiagonal();
                    Eigen::MatrixXd U = svd.matrixU();
                    Eigen::MatrixXd A = U * S.cwiseSqrt() * std::sqrt(n + lambda);

                    for (int i = 0; i < n; i++) {
                        sigma_points.col(i + 1) = x + A.col(i);
                        sigma_points.col(i + n + 1) = x - A.col(i);
                    }
                }
            } catch (...) {
                // 紧急备选方案
                for (int i = 0; i < n; i++) {
                    double spread = std::sqrt((n + lambda) * std::max(1e-9, P_stable(i, i)));
                    Eigen::VectorXd delta = Eigen::VectorXd::Zero(n);
                    delta(i) = spread;
                    sigma_points.col(i + 1) = x + delta;
                    sigma_points.col(i + n + 1) = x - delta;
                }
            }

            return sigma_points;
        }

        // ⭐ 用于滤波的状态转移函数（原始版本）
        Eigen::VectorXd stateTransition(const Eigen::VectorXd& x) {
            if (isPredictingFuture) {
                return stateTransitionForPrediction(x);
            }

            Eigen::VectorXd x_next(STATE_DIM);

            // 高阶运动学模型
            x_next(0) = x(0) + x(1) * dt + 0.5 * x(2) * dt * dt + (1.0/6.0) * x(3) * dt * dt * dt;
            x_next(1) = x(1) + x(2) * dt + 0.5 * x(3) * dt * dt;
            x_next(2) = x(2) + x(3) * dt;
            x_next(3) = x(3);  // 加加速度保持

            // ⭐ 智能动态衰减系统（用于滤波）
            double baseDecay = 0.95;
            double accelDecay = 0.90;
            double jerkDecay = 0.85;

            // 基于运动模式的衰减调整
            switch (motionPattern.currentType) {
            case MotionPatternRecognizer::STABLE:
                baseDecay = 0.85;
                accelDecay = 0.80;
                jerkDecay = 0.75;
                break;

            case MotionPatternRecognizer::SMOOTH_PURSUIT:
                baseDecay = 0.92;
                accelDecay = 0.88;
                jerkDecay = 0.85;
                break;

            case MotionPatternRecognizer::SACCADE:
                baseDecay = 0.98;
                accelDecay = 0.95;
                jerkDecay = 0.92;
                break;

            case MotionPatternRecognizer::NYSTAGMUS:
                baseDecay = 0.93;
                accelDecay = 0.90;
                jerkDecay = 0.87;
                break;

            default:
                break;
            }

            // 峰值状态的特殊处理
            if (peakDetector.isPeak) {
                baseDecay = 0.99;
                accelDecay = 0.97;
                jerkDecay = 0.95;

                // 峰值反转补偿
                if (peakDetector.peakType != 0) {
                    x_next(2) *= -0.5; // 加速度反向
                    x_next(3) *= -0.8; // 加加速度反向
                }
            } else if (peakDetector.isApproachingPeak) {
                baseDecay = 0.96;
                accelDecay = 0.93;

                // 预测性补偿
                double compensation = peakDetector.peakConfidence * 0.1;
                x_next(0) += x(1) * dt * compensation;
            }

            // 应用衰减
            x_next(1) *= baseDecay;
            x_next(2) *= accelDecay;
            x_next(3) *= jerkDecay;

            // ⭐ 周期性预测增强
            if (nystagmusDetector.isNystagmus && nystagmusDetector.periodConfidence > 0.7) {
                double nextPeakTime = nystagmusDetector.predictNextPeakTime(currentTimestamp);
                if (nextPeakTime > 0) {
                    double timeToNextPeak = nextPeakTime - currentTimestamp;
                    if (timeToNextPeak > 0 && timeToNextPeak < nystagmusDetector.estimatedPeriod) {
                        // 基于周期的预测调整
                        double phase = (timeToNextPeak / nystagmusDetector.estimatedPeriod) * 2 * M_PI;
                        double phaseCompensation = std::sin(phase) * nystagmusDetector.amplitude * 0.05;
                        x_next(0) += phaseCompensation;
                    }
                }
            }

            // 物理约束
            x_next(1) = std::max(-300.0, std::min(300.0, x_next(1)));
            x_next(2) = std::max(-800.0, std::min(800.0, x_next(2)));
            x_next(3) = std::max(-2000.0, std::min(2000.0, x_next(3)));

            return x_next;
        }

        // ⭐ 专门用于预测的状态转移函数
        Eigen::VectorXd stateTransitionForPrediction(const Eigen::VectorXd& x) {
            Eigen::VectorXd x_next(STATE_DIM);

            // 基础运动学模型
            x_next(0) = x(0) + x(1) * dt + 0.5 * x(2) * dt * dt + (1.0/6.0) * x(3) * dt * dt * dt;
            x_next(1) = x(1) + x(2) * dt + 0.5 * x(3) * dt * dt;
            x_next(2) = x(2) + x(3) * dt;
            x_next(3) = x(3);  // 假设加加速度不变

            // 预测专用的衰减参数（与滤波时不同）
            double velocityDecay = 0.98;    // 更慢的衰减
            double accelDecay = 0.95;        // 更慢的衰减
            double jerkDecay = 0.90;         // 更慢的衰减

            // 基于运动模式的预测调整
            switch (motionPattern.currentType) {
            case MotionPatternRecognizer::STABLE:
                velocityDecay = 0.90;
                accelDecay = 0.85;
                jerkDecay = 0.80;
                break;

            case MotionPatternRecognizer::SMOOTH_PURSUIT:
                velocityDecay = 0.95;
                accelDecay = 0.92;
                jerkDecay = 0.88;
                break;

            case MotionPatternRecognizer::SACCADE:
                // 眼跳时假设会快速停止
                velocityDecay = 0.85;
                accelDecay = 0.70;
                jerkDecay = 0.60;
                break;

            case MotionPatternRecognizer::NYSTAGMUS:
                // 眼震时使用周期性预测
                if (nystagmusDetector.isNystagmus && nystagmusDetector.periodConfidence > 0.7) {
                    double futureTime = currentTimestamp + dt;
                    double phase = std::fmod(futureTime - nystagmusDetector.lastPeakTime,
                                             nystagmusDetector.estimatedPeriod);
                    double phaseRatio = phase / nystagmusDetector.estimatedPeriod;

                    // 基于相位的周期性调整
                    double sinComponent = std::sin(2 * M_PI * phaseRatio);
                    x_next(0) += nystagmusDetector.amplitude * sinComponent * 0.1;
                    x_next(1) += nystagmusDetector.amplitude * std::cos(2 * M_PI * phaseRatio) * 0.5;
                }
                velocityDecay = 0.95;
                accelDecay = 0.92;
                break;

            default:
                break;
            }

            // 应用衰减
            x_next(1) *= velocityDecay;
            x_next(2) *= accelDecay;
            x_next(3) *= jerkDecay;

            // 物理约束
            x_next(1) = std::max(-300.0, std::min(300.0, x_next(1)));
            x_next(2) = std::max(-800.0, std::min(800.0, x_next(2)));
            x_next(3) = std::max(-2000.0, std::min(2000.0, x_next(3)));

            return x_next;
        }

        // 测量函数
        Eigen::VectorXd measurementFunction(const Eigen::VectorXd& x) {
            Eigen::VectorXd z(MEAS_DIM);
            z(0) = x(0);
            return z;
        }

        // ⭐ 更新滤波器状态（原 predictX 重命名）
        float updateFilter(float measurementX, int frameId) {
            // 更新时间戳
            currentTimestamp = frameId * dt;

            // 输入验证
            if (std::isnan(measurementX) || std::isinf(measurementX)) {
                return initialized ? state(0) : measurementX;
            }

            // 范围限制
            measurementX = std::max(0.0f, std::min(1920.0f, measurementX));

            Eigen::VectorXd z(MEAS_DIM);
            z(0) = measurementX;

            if (!initialized) {
                state(0) = measurementX;
                state(1) = 0;
                state(2) = 0;
                state(3) = 0;
                initialized = true;
                lastX = measurementX;
                measurementHistory.push_back(measurementX);
                positionHistory.push_back(measurementX);
                return measurementX;
            }

            // 计算速度和加速度
            float velocity = (measurementX - lastX) / dt;
            velocity = std::max(-350.0f, std::min(350.0f, velocity));

            float acceleration = 0;
            if (!velocityHistory.empty()) {
                acceleration = (velocity - velocityHistory.back()) / dt;
                acceleration = std::max(-1000.0f, std::min(1000.0f, acceleration));
            }

            // 更新各种检测器
            peakDetector.update(measurementX, velocity, acceleration);
            nystagmusDetector.update(measurementX, velocity, currentTimestamp);
            motionPattern.detectPattern(velocity, acceleration);

            // 检测大跳跃
            float jump = std::abs(measurementX - lastX);
            bool largeJump = jump > 120.0;

            if (largeJump) {
                // 大跳跃时的智能处理
                handleLargeJump(measurementX, velocity);

                lastX = measurementX;
                updateHistory(measurementX, velocity, acceleration);
                return state(0);
            }

            // 自适应参数调整
            adaptParameters(velocity, acceleration);

            // === UKF预测步骤 ===
            float filteredValue = measurementX; // 默认值

            try {
                // 生成sigma点
                Eigen::MatrixXd sigma_points = generateSigmaPoints(state, P);
                int n_sigma = 2 * STATE_DIM + 1;

                // 传播sigma点
                Eigen::MatrixXd sigma_points_pred(STATE_DIM, n_sigma);
                for (int i = 0; i < n_sigma; i++) {
                    sigma_points_pred.col(i) = stateTransition(sigma_points.col(i));
                }

                // 计算预测均值
                Eigen::VectorXd x_pred = Eigen::VectorXd::Zero(STATE_DIM);
                for (int i = 0; i < n_sigma; i++) {
                    x_pred += Wm(i) * sigma_points_pred.col(i);
                }

                // 计算预测协方差
                Eigen::MatrixXd P_pred = Q;
                for (int i = 0; i < n_sigma; i++) {
                    Eigen::VectorXd diff = sigma_points_pred.col(i) - x_pred;
                    P_pred += Wc(i) * diff * diff.transpose();
                }

                // === UKF更新步骤 ===
                sigma_points = generateSigmaPoints(x_pred, P_pred);

                Eigen::MatrixXd sigma_points_meas(MEAS_DIM, n_sigma);
                for (int i = 0; i < n_sigma; i++) {
                    sigma_points_meas.col(i) = measurementFunction(sigma_points.col(i));
                }

                // 测量预测
                Eigen::VectorXd z_pred = Eigen::VectorXd::Zero(MEAS_DIM);
                for (int i = 0; i < n_sigma; i++) {
                    z_pred += Wm(i) * sigma_points_meas.col(i);
                }

                // 创新协方差
                Eigen::MatrixXd S = R;
                for (int i = 0; i < n_sigma; i++) {
                    Eigen::VectorXd diff = sigma_points_meas.col(i) - z_pred;
                    S += Wc(i) * diff * diff.transpose();
                }

                // 交叉协方差
                Eigen::MatrixXd Pxz = Eigen::MatrixXd::Zero(STATE_DIM, MEAS_DIM);
                for (int i = 0; i < n_sigma; i++) {
                    Pxz += Wc(i) * (sigma_points.col(i) - x_pred) *
                           (sigma_points_meas.col(i) - z_pred).transpose();
                }

                // 卡尔曼增益
                Eigen::MatrixXd K = Pxz * S.inverse();
                Eigen::VectorXd innovation = z - z_pred;

                // ⭐ 创新界限检查
                double innovationMagnitude = std::abs(innovation(0));
                if (innovationMagnitude > 50.0) {
                    // 大创新值时降低增益
                    K *= std::max(0.3, 1.0 - (innovationMagnitude - 50.0) / 100.0);
                }

                // 更新状态和协方差
                state = x_pred + K * innovation;
                P = P_pred - K * S * K.transpose();

                // 确保协方差矩阵正定
                ensureCovariancePositive(P);

                // 状态约束
                constrainState();

                filteredValue = state(0);

            } catch (...) {
                // 异常处理
                handleException();
                return measurementX;
            }

            // ⭐ 后处理优化
            filteredValue = postProcessPrediction(filteredValue, measurementX);

            // 更新历史
            lastX = measurementX;
            updateHistory(measurementX, velocity, acceleration);

            return filteredValue;
        }

        // ⭐ 新增：真正的预测函数
        float predictFutureX(int stepsAhead = 1) {
            if (!initialized) {
                qDebug() << "UKF未初始化，无法预测";
                return 0.0f;
            }

            // 设置预测模式
            isPredictingFuture = true;

            try {
                // 保存当前状态（避免修改滤波器状态）
                Eigen::VectorXd currentState = state;
                Eigen::MatrixXd currentP = P;

                // 进行多步预测
                Eigen::VectorXd predictedState = currentState;
                for (int step = 0; step < stepsAhead; step++) {
                    // 生成sigma点
                    Eigen::MatrixXd sigma_points = generateSigmaPoints(predictedState, currentP);
                    int n_sigma = 2 * STATE_DIM + 1;

                    // 传播sigma点（使用预测专用的状态转移）
                    Eigen::MatrixXd sigma_points_pred(STATE_DIM, n_sigma);
                    for (int i = 0; i < n_sigma; i++) {
                        sigma_points_pred.col(i) = stateTransitionForPrediction(sigma_points.col(i));
                    }

                    // 计算预测均值
                    predictedState = Eigen::VectorXd::Zero(STATE_DIM);
                    for (int i = 0; i < n_sigma; i++) {
                        predictedState += Wm(i) * sigma_points_pred.col(i);
                    }

                    // 更新预测协方差
                    currentP = Q;
                    for (int i = 0; i < n_sigma; i++) {
                        Eigen::VectorXd diff = sigma_points_pred.col(i) - predictedState;
                        currentP += Wc(i) * diff * diff.transpose();
                    }

                    // 确保协方差正定
                    ensureCovariancePositive(currentP);
                }

                // 重置预测模式
                isPredictingFuture = false;

                // 应用物理约束
                float prediction = std::max(0.0f, std::min(1920.0f, (float)predictedState(0)));

                return prediction;

            } catch (...) {
                isPredictingFuture = false;
                return state(0);  // 返回当前位置作为备选
            }
        }

        // ⭐ 新增：预测完整轨迹
        std::vector<float> predictTrajectory(int numSteps) {
            std::vector<float> trajectory;

            if (!initialized) {
                qDebug() << "UKF未初始化，无法预测轨迹";
                return trajectory;
            }

            // 设置预测模式
            isPredictingFuture = true;

            try {
                Eigen::VectorXd futureState = state;
                Eigen::MatrixXd futureP = P;

                for (int i = 0; i < numSteps; i++) {
                    // 生成sigma点
                    Eigen::MatrixXd sigma_points = generateSigmaPoints(futureState, futureP);
                    int n_sigma = 2 * STATE_DIM + 1;

                    // 传播sigma点
                    Eigen::MatrixXd sigma_points_pred(STATE_DIM, n_sigma);
                    for (int j = 0; j < n_sigma; j++) {
                        sigma_points_pred.col(j) = stateTransitionForPrediction(sigma_points.col(j));
                    }

                    // 计算预测均值
                    futureState = Eigen::VectorXd::Zero(STATE_DIM);
                    for (int j = 0; j < n_sigma; j++) {
                        futureState += Wm(j) * sigma_points_pred.col(j);
                    }

                    // 更新协方差
                    futureP = Q;
                    for (int j = 0; j < n_sigma; j++) {
                        Eigen::VectorXd diff = sigma_points_pred.col(j) - futureState;
                        futureP += Wc(j) * diff * diff.transpose();
                    }

                    trajectory.push_back(std::max(0.0f, std::min(1920.0f, (float)futureState(0))));
                }

                isPredictingFuture = false;
                return trajectory;

            } catch (...) {
                isPredictingFuture = false;
                return trajectory;
            }
        }

        // ⭐ 新增：带不确定性的预测
        struct PredictionWithUncertainty {
            float position;
            float uncertainty;  // 标准差
        };

        std::vector<PredictionWithUncertainty> predictWithUncertainty(int numSteps) {
            std::vector<PredictionWithUncertainty> predictions;

            if (!initialized) return predictions;

            isPredictingFuture = true;

            try {
                Eigen::VectorXd futureState = state;
                Eigen::MatrixXd futureP = P;

                for (int i = 0; i < numSteps; i++) {
                    // 预测状态
                    futureState = stateTransitionForPrediction(futureState);

                    // 预测协方差
                    Eigen::MatrixXd F = computeStateTransitionJacobian(futureState);
                    futureP = F * futureP * F.transpose() + Q;

                    // 提取位置和不确定性
                    PredictionWithUncertainty pred;
                    pred.position = std::max(0.0f, std::min(1920.0f, (float)futureState(0)));
                    pred.uncertainty = std::sqrt(futureP(0, 0));
                    predictions.push_back(pred);
                }

                isPredictingFuture = false;
                return predictions;

            } catch (...) {
                isPredictingFuture = false;
                return predictions;
            }
        }

        // ⭐ 计算状态转移雅可比矩阵（用于不确定性传播）
        Eigen::MatrixXd computeStateTransitionJacobian(const Eigen::VectorXd& x) {
            Eigen::MatrixXd F = Eigen::MatrixXd::Identity(STATE_DIM, STATE_DIM);

            F(0, 1) = dt;
            F(0, 2) = 0.5 * dt * dt;
            F(0, 3) = (1.0/6.0) * dt * dt * dt;
            F(1, 2) = dt;
            F(1, 3) = 0.5 * dt * dt;
            F(2, 3) = dt;

            // 考虑衰减的影响
            F(1, 1) = 0.95;  // 速度衰减
            F(2, 2) = 0.90;  // 加速度衰减
            F(3, 3) = 0.85;  // 加加速度衰减

            return F;
        }

        // 其他辅助函数...
        void adaptParameters(float velocity, float acceleration) {
            float absVel = std::abs(velocity);
            float absAcc = std::abs(acceleration);

            // 基础噪声值
            double baseQ0 = 0.5, baseQ1 = 10.0, baseQ2 = 50.0, baseQ3 = 200.0;
            double baseR = 8.0;
            double baseAlpha = 0.001;

            // 基于运动模式的参数调整
            double motionFactor = 1.0;
            switch (motionPattern.currentType) {
            case MotionPatternRecognizer::STABLE:
                motionFactor = 0.5;
                baseAlpha = 0.0001;
                break;
            case MotionPatternRecognizer::SMOOTH_PURSUIT:
                motionFactor = 0.8;
                baseAlpha = 0.0005;
                break;
            case MotionPatternRecognizer::SACCADE:
                motionFactor = 2.0;
                baseAlpha = 0.01;
                break;
            case MotionPatternRecognizer::NYSTAGMUS:
                motionFactor = 1.2;
                baseAlpha = 0.001;
                break;
            }

            // 峰值状态调整
            if (peakDetector.isPeak) {
                motionFactor *= 2.5;
                baseR *= 0.5;
                baseAlpha = 0.02;
            } else if (peakDetector.isApproachingPeak) {
                motionFactor *= 1.8;
                baseR *= 0.7;
                baseAlpha = 0.01;
            }

            // 应用调整
            Q(0, 0) = baseQ0 * motionFactor;
            Q(1, 1) = baseQ1 * motionFactor;
            Q(2, 2) = baseQ2 * motionFactor;
            Q(3, 3) = baseQ3 * motionFactor;
            R(0, 0) = baseR / (motionFactor * 0.5 + 0.5);

            // 基于历史稳定性的微调
            if (velocityHistory.size() >= 10) {
                double velStd = calculateStandardDeviation(velocityHistory);
                double stabFactor = 1.0 / (1.0 + std::exp(-0.1 * (velStd - 50.0)));
                Q *= (0.5 + stabFactor);
                R *= (1.5 - stabFactor * 0.5);
            }

            // 确保参数在合理范围内
            Q(0, 0) = std::max(0.1, std::min(Q(0, 0), 10.0));
            Q(1, 1) = std::max(1.0, std::min(Q(1, 1), 100.0));
            Q(2, 2) = std::max(10.0, std::min(Q(2, 2), 500.0));
            Q(3, 3) = std::max(50.0, std::min(Q(3, 3), 2000.0));
            R(0, 0) = std::max(2.0, std::min(R(0, 0), 20.0));

            // 更新alpha和UKF参数
            alpha = baseAlpha;
            lambda = alpha * alpha * (STATE_DIM + kappa) - STATE_DIM;
            initializeWeights();
        }

        void handleLargeJump(float measurementX, float velocity) {
            // 智能混合：更多地相信预测值
            state(0) = measurementX * 0.4 + state(0) * 0.6;
            state(1) = velocity * 0.3;
            state(2) *= 0.2;
            state(3) = 0;

            // 增加不确定性，但不要过度
            P(0, 0) = std::min(100.0, P(0, 0) * 2.0);
            P(1, 1) = std::min(400.0, P(1, 1) * 2.0);
            P(2, 2) = std::min(1600.0, P(2, 2) * 2.0);
            P(3, 3) = std::min(6400.0, P(3, 3) * 2.0);
        }

        void ensureCovariancePositive(Eigen::MatrixXd& P) {
            // 对称化
            P = (P + P.transpose()) / 2.0;

            // 对角线最小值限制
            for (int i = 0; i < STATE_DIM; i++) {
                double minVal = (i == 0) ? 1.0 : (i == 1) ? 10.0 : (i == 2) ? 50.0 : 200.0;
                double maxVal = minVal * 100.0;

                if (P(i, i) < minVal) {
                    P(i, i) = minVal;
                } else if (P(i, i) > maxVal) {
                    P(i, i) = maxVal;
                }
            }

            // 确保正定性
            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(P);
            if (es.eigenvalues().minCoeff() < 1e-9) {
                Eigen::VectorXd eigenvalues = es.eigenvalues();
                for (int i = 0; i < eigenvalues.size(); i++) {
                    if (eigenvalues(i) < 1e-9) {
                        eigenvalues(i) = 1e-9;
                    }
                }
                P = es.eigenvectors() * eigenvalues.asDiagonal() * es.eigenvectors().transpose();
            }
        }

        void constrainState() {
            state(0) = std::max(0.0, std::min(1920.0, state(0)));
            state(1) = std::max(-300.0, std::min(300.0, state(1)));
            state(2) = std::max(-800.0, std::min(800.0, state(2)));
            state(3) = std::max(-2000.0, std::min(2000.0, state(3)));
        }

        float postProcessPrediction(float prediction, float measurement) {
            float result = prediction;

            // 峰值补偿
            if (peakDetector.isPeak || peakDetector.isApproachingPeak) {
                float diff = measurement - prediction;
                float compensation = peakDetector.peakConfidence * 0.4;
                result += diff * compensation;
            }

            // 眼震周期补偿
            if (nystagmusDetector.isNystagmus && nystagmusDetector.periodConfidence > 0.8) {
                // 基于周期相位的微调
                double phaseError = std::fmod(currentTimestamp - nystagmusDetector.lastPeakTime,
                                              nystagmusDetector.estimatedPeriod);
                double phaseFactor = std::sin(2 * M_PI * phaseError / nystagmusDetector.estimatedPeriod);
                result += phaseFactor * 2.0; // 小幅相位补偿
            }

            return result;
        }

        void updateHistory(float measurement, float velocity, float acceleration) {
            velocityHistory.push_back(velocity);
            measurementHistory.push_back(measurement);
            positionHistory.push_back(state(0));
            accelerationHistory.push_back(acceleration);

            if (velocityHistory.size() > HISTORY_SIZE) {
                velocityHistory.pop_front();
                measurementHistory.pop_front();
                positionHistory.pop_front();
                accelerationHistory.pop_front();
            }
        }

        void handleException() {
            // 部分重置，保留一些状态信息
            P = Eigen::MatrixXd::Identity(STATE_DIM, STATE_DIM);
            P(0, 0) = 50.0;
            P(1, 1) = 200.0;
            P(2, 2) = 800.0;
            P(3, 3) = 3200.0;

            // 保留位置和速度，重置高阶项
            state(2) *= 0.5;
            state(3) = 0;
        }

        double calculateStandardDeviation(const std::deque<float>& data) {
            if (data.empty()) return 0;

            double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
            double variance = 0;
            for (float val : data) {
                variance += (val - mean) * (val - mean);
            }
            return std::sqrt(variance / data.size());
        }

        void reset() {
            initialized = false;
            lastX = 0;
            currentTimestamp = 0;
            isPredictingFuture = false;
            velocityHistory.clear();
            measurementHistory.clear();
            positionHistory.clear();
            accelerationHistory.clear();

            peakDetector.reset();
            nystagmusDetector.reset();
            motionPattern.reset();

            state = Eigen::VectorXd::Zero(STATE_DIM);
            P = Eigen::MatrixXd::Identity(STATE_DIM, STATE_DIM);
            P(0, 0) = 25.0;
            P(1, 1) = 100.0;
            P(2, 2) = 400.0;
            P(3, 3) = 1600.0;

            Q = Eigen::MatrixXd::Zero(STATE_DIM, STATE_DIM);
            Q(0, 0) = 0.5;
            Q(1, 1) = 10.0;
            Q(2, 2) = 50.0;
            Q(3, 3) = 200.0;

            R = Eigen::MatrixXd::Identity(MEAS_DIM, MEAS_DIM) * 8.0;

            alpha = 0.001;
            lambda = alpha * alpha * (STATE_DIM + kappa) - STATE_DIM;
            initializeWeights();
        }

        std::string getStatus() const {
            std::stringstream ss;
            ss << "V=" << std::fixed << std::setprecision(1)
               << (initialized ? state(1) : 0.0) << "px/s";

            if (nystagmusDetector.isNystagmus) {
                ss << ", 眼震(" << std::setprecision(1)
                    << nystagmusDetector.frequency << "Hz, "
                    << nystagmusDetector.amplitude << "px)";
            }

            if (peakDetector.isPeak) {
                ss << ", 峰值(" << (peakDetector.peakType > 0 ? "MAX" : "MIN")
                   << ", " << std::setprecision(1) << peakDetector.peakConfidence << ")";
            } else if (peakDetector.isApproachingPeak) {
                ss << ", 接近峰值";
            }

            ss << ", 模式:" << getMotionTypeName(motionPattern.currentType);

            return ss.str();
        }

        std::string getMotionTypeName(MotionPatternRecognizer::MotionType type) const {
            switch (type) {
            case MotionPatternRecognizer::STABLE: return "稳定";
            case MotionPatternRecognizer::SMOOTH_PURSUIT: return "平滑追踪";
            case MotionPatternRecognizer::SACCADE: return "跳视";
            case MotionPatternRecognizer::NYSTAGMUS: return "眼震";
            case MotionPatternRecognizer::TRANSITION: return "过渡";
            default: return "未知";
            }
        }

        Eigen::VectorXd getState() const { return state; }
        double getCurrentVelocity() const { return initialized ? state(1) : 0.0; }
        double getCurrentAcceleration() const { return initialized ? state(2) : 0.0; }
        bool isNystagmusDetected() const { return nystagmusDetector.isNystagmus; }
        double getNystagmusFrequency() const { return nystagmusDetector.frequency; }
        double getNystagmusAmplitude() const { return nystagmusDetector.amplitude; }
    };

    // ⭐ 增强的异常值处理器
    class EnhancedOutlierFilter {
    private:
        std::deque<float> history;
        std::deque<float> predictions;
        static const int WINDOW_SIZE = 5;

        // 统计信息
        double runningMean = 0;
        double runningVariance = 0;
        int sampleCount = 0;

    public:
        float filter(float measurement, float prediction) {
            // 更新统计
            updateStatistics(measurement);

            float error = std::abs(measurement - prediction);
            float result = prediction;

            // 动态阈值：基于运行方差
            float dynamicThreshold = 40.0;
            if (runningVariance > 0 && sampleCount > 10) {
                dynamicThreshold = std::min(100.0, std::max(30.0, 2.0 * std::sqrt(runningVariance)));
            }

            // 多级过滤策略
            if (error < dynamicThreshold) {
                // 小误差：直接使用预测
                result = prediction;
            } else if (error < dynamicThreshold * 2) {
                // 中等误差：加权平均
                float weight = (error - dynamicThreshold) / dynamicThreshold;
                weight = std::min(0.7f, weight);
                result = prediction * (1.0f - weight) + measurement * weight;
            } else {
                // 大误差：使用历史信息
                if (!history.empty()) {
                    // 使用中值滤波
                    std::vector<float> sortedHistory(history.begin(), history.end());
                    std::sort(sortedHistory.begin(), sortedHistory.end());
                    float median = sortedHistory[sortedHistory.size() / 2];

                    // 混合中值和预测
                    result = median * 0.6f + prediction * 0.4f;
                } else {
                    // 保守策略
                    result = prediction * 0.7f + measurement * 0.3f;
                }
            }

            // 更新历史
            history.push_back(result);
            predictions.push_back(prediction);

            if (history.size() > WINDOW_SIZE) {
                history.pop_front();
                predictions.pop_front();
            }

            return result;
        }

        void updateStatistics(float value) {
            sampleCount++;
            double delta = value - runningMean;
            runningMean += delta / sampleCount;
            double delta2 = value - runningMean;
            runningVariance = ((sampleCount - 1) * runningVariance + delta * delta2) / sampleCount;
        }

        void reset() {
            history.clear();
            predictions.clear();
            runningMean = 0;
            runningVariance = 0;
            sampleCount = 0;
        }
    };

private:
    EnhancedXAxisUKF xTracker;
    EnhancedOutlierFilter outlierFilter;

    // ⭐ 新增：预测缓存系统
    struct PredictionBuffer {
        std::map<int, cv::Point2f> predictions;     // frameId -> prediction
        std::map<int, double> errors;              // frameId -> prediction error
        std::map<int, double> timestamps;          // frameId -> timestamp

        void storePrediction(int frameId, cv::Point2f prediction, double timestamp) {
            predictions[frameId] = prediction;
            timestamps[frameId] = timestamp;

            // 保持缓存大小
            if (predictions.size() > 100) {
                auto oldest = predictions.begin();
                predictions.erase(oldest);
                errors.erase(oldest->first);
                timestamps.erase(oldest->first);
            }
        }

        double evaluatePrediction(int frameId, cv::Point2f actual) {
            if (predictions.find(frameId) != predictions.end()) {
                double error = cv::norm(actual - predictions[frameId]);
                errors[frameId] = error;
                return error;
            }
            return -1.0; // 没有找到预测值
        }

        bool hasPrediction(int frameId) const {
            return predictions.find(frameId) != predictions.end();
        }

        cv::Point2f getPrediction(int frameId) const {
            auto it = predictions.find(frameId);
            return (it != predictions.end()) ? it->second : cv::Point2f(0, 0);
        }

        void clear() {
            predictions.clear();
            errors.clear();
            timestamps.clear();
        }

        double getRecentAvgError(int windowSize = 20) const {
            if (errors.empty()) return 0.0;

            double sum = 0.0;
            int count = 0;
            auto it = errors.rbegin();

            while (it != errors.rend() && count < windowSize) {
                sum += it->second;
                count++;
                ++it;
            }

            return count > 0 ? sum / count : 0.0;
        }
    } predictionBuffer;

    // ⭐ 预测统计系统
    struct PredictionStats {
        std::deque<float> filterErrors;
        std::deque<float> predictionErrors;
        float totalFilterError = 0;
        float totalPredictionError = 0;
        float maxFilterError = 0;
        float maxPredictionError = 0;
        int filterCount = 0;
        int predictionCount = 0;

        void addFilterError(float error) {
            filterErrors.push_back(error);
            totalFilterError += error;
            maxFilterError = std::max(maxFilterError, error);
            filterCount++;

            // 保持最近100个错误
            if (filterErrors.size() > 100) {
                totalFilterError -= filterErrors.front();
                filterErrors.pop_front();
            }
        }

        void addPredictionError(float error) {
            predictionErrors.push_back(error);
            totalPredictionError += error;
            maxPredictionError = std::max(maxPredictionError, error);
            predictionCount++;

            // 保持最近100个错误
            if (predictionErrors.size() > 100) {
                totalPredictionError -= predictionErrors.front();
                predictionErrors.pop_front();
            }
        }

        float getAvgFilterError() const {
            return filterErrors.empty() ? 0 : totalFilterError / filterErrors.size();
        }

        float getAvgPredictionError() const {
            return predictionErrors.empty() ? 0 : totalPredictionError / predictionErrors.size();
        }

        void reset() {
            filterErrors.clear();
            predictionErrors.clear();
            totalFilterError = totalPredictionError = 0;
            maxFilterError = maxPredictionError = 0;
            filterCount = predictionCount = 0;
        }
    } stats;

public:
    ParallelNystagmusPipeline() {}

    // ⭐ 主处理函数：分离滤波和预测
    cv::Point2f processFrame(const cv::Point2f& measurement, int frameId,
                             double& processingTimeMs, std::string& diagnosticInfo) {
        auto startTime = std::chrono::high_resolution_clock::now();

        // 步骤1：评估上一帧的预测准确性
        if (frameId > 0 && predictionBuffer.hasPrediction(frameId)) {
            double predictionError = predictionBuffer.evaluatePrediction(frameId, measurement);
            if (predictionError >= 0) {
                stats.addPredictionError(predictionError);
            }
        }

        // 步骤2：更新滤波器状态（使用当前测量值）
        float filteredX = xTracker.updateFilter(measurement.x, frameId);

        // 步骤3：预测下一帧位置（真正的预测）
        float predictedNextX = xTracker.predictFutureX(1);

        // 步骤4：应用异常值过滤
        float finalFiltered = outlierFilter.filter(measurement.x, filteredX);

        // 步骤5：生成对下一帧的预测
        cv::Point2f predictionForNextFrame(predictedNextX, measurement.y);

        // 步骤6：存储预测用于下次评估
        predictionBuffer.storePrediction(frameId + 1, predictionForNextFrame,
                                         std::chrono::duration<double>(
                                             std::chrono::high_resolution_clock::now().time_since_epoch()
                                             ).count());

        // 步骤7：更新滤波统计
        if (frameId > 0) {
            double filterError = std::abs(measurement.x - finalFiltered);
            stats.addFilterError(filterError);
        }

        // 步骤8：生成诊断信息
        std::stringstream ss;
        ss << "🔮 并行预测管道 F" << frameId << " | ";
        if (frameId > 0) {
            ss << "滤波误差:" << std::fixed << std::setprecision(1)
                << std::abs(measurement.x - finalFiltered) << "px | ";

            if (predictionBuffer.hasPrediction(frameId)) {
                double predError = predictionBuffer.evaluatePrediction(frameId, measurement);
                if (predError >= 0) {
                    ss << "预测误差:" << std::setprecision(1) << predError << "px | ";
                }
            }
        }
        ss << "下帧预测:" << std::setprecision(1) << predictedNextX << " | ";
        ss << xTracker.getStatus();
        diagnosticInfo = ss.str();

        auto endTime = std::chrono::high_resolution_clock::now();
        processingTimeMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        // 返回当前帧的滤波结果（不是预测值）
        return cv::Point2f(finalFiltered, measurement.y);
    }

    // ⭐ 新增：获取对指定帧的预测
    cv::Point2f getPredictionForFrame(int targetFrameId) {
        if (predictionBuffer.hasPrediction(targetFrameId)) {
            return predictionBuffer.getPrediction(targetFrameId);
        }
        return cv::Point2f(0, 0);
    }

    // ⭐ 新增：多步预测轨迹
    std::vector<cv::Point2f> predictFutureTrajectory(int numSteps) {
        std::vector<float> xTrajectory = xTracker.predictTrajectory(numSteps);
        std::vector<cv::Point2f> trajectory;

        for (float x : xTrajectory) {
            trajectory.push_back(cv::Point2f(x, 0));  // Y轴暂时设为0
        }

        return trajectory;
    }

    // ⭐ 新增：带置信度的预测
    std::vector<std::pair<cv::Point2f, float>> predictWithConfidence(int numSteps) {
        auto predictions = xTracker.predictWithUncertainty(numSteps);
        std::vector<std::pair<cv::Point2f, float>> result;

        for (const auto& pred : predictions) {
            cv::Point2f point(pred.position, 0);  // Y轴需要单独处理
            float confidence = 1.0f / (1.0f + pred.uncertainty / 10.0f);  // 转换为置信度
            result.push_back({point, confidence});
        }

        return result;
    }

    // ⭐ 新增：预测性能评估
    void evaluatePrediction(int frameId, const cv::Point2f& actualPosition) {
        // 这个函数应该在下一帧到达时调用，比较预测值和实际值
        static std::map<int, cv::Point2f> predictions;

        // 检查是否有对这一帧的预测
        if (predictions.find(frameId) != predictions.end()) {
            cv::Point2f predicted = predictions[frameId];
            float error = cv::norm(actualPosition - predicted);

            // 更新预测统计
            stats.addPredictionError(error);

            if (frameId % 100 == 0) {
                qDebug() << "预测性能：平均误差=" << stats.getAvgPredictionError()
                    << "px，最大误差=" << stats.maxPredictionError << "px";
            }
        }

        // 存储当前预测供未来评估
        cv::Point2f nextPrediction(xTracker.predictFutureX(1), actualPosition.y);
        predictions[frameId + 1] = nextPrediction;

        // 清理旧预测
        if (predictions.size() > 100) {
            predictions.erase(predictions.begin());
        }
    }

    // 获取系统状态
    std::string getDiagnosticInfo() const {
        std::stringstream ss;
        ss << "\n===== 并行眼震预测管道 v4.0 =====\n";
        ss << "架构: 滤波与预测完全分离 + 真正时间预测\n";
        ss << "处理帧数: " << stats.filterCount << " | 预测评估: " << stats.predictionCount << "\n";

        ss << "滤波性能:\n";
        ss << "  平均误差: " << std::fixed << std::setprecision(2) << stats.getAvgFilterError() << " px\n";
        ss << "  最大误差: " << std::fixed << std::setprecision(2) << stats.maxFilterError << " px\n";

        ss << "预测性能:\n";
        ss << "  平均误差: " << std::fixed << std::setprecision(2) << stats.getAvgPredictionError() << " px\n";
        ss << "  最大误差: " << std::fixed << std::setprecision(2) << stats.maxPredictionError << " px\n";

        // 预测精度分布
        int excellentPred = 0, goodPred = 0, acceptablePred = 0;
        for (float error : stats.predictionErrors) {
            if (error < 5.0) excellentPred++;
            if (error < 15.0) goodPred++;
            if (error < 30.0) acceptablePred++;
        }

        if (!stats.predictionErrors.empty()) {
            ss << "预测精度分布:\n";
            ss << "  卓越(<5px): " << std::fixed << std::setprecision(1)
               << (excellentPred * 100.0 / stats.predictionErrors.size()) << "%\n";
            ss << "  良好(<15px): " << std::fixed << std::setprecision(1)
               << (goodPred * 100.0 / stats.predictionErrors.size()) << "%\n";
            ss << "  可接受(<30px): " << std::fixed << std::setprecision(1)
               << (acceptablePred * 100.0 / stats.predictionErrors.size()) << "%\n";
        }

        ss << "当前状态: " << xTracker.getStatus() << "\n";
        ss << "预测缓存: " << predictionBuffer.predictions.size() << " 个\n";
        ss << "缓存平均误差: " << std::fixed << std::setprecision(2)
           << predictionBuffer.getRecentAvgError() << " px\n";

        ss << "核心功能:\n";
        ss << "  ✅ 真正的时间预测（非补偿滤波）\n";
        ss << "  ✅ 滤波与预测完全分离\n";
        ss << "  ✅ 多步预测轨迹支持\n";
        ss << "  ✅ 预测不确定性量化\n";
        ss << "  ✅ 实时预测性能评估\n";
        ss << "  ✅ 智能异常值处理\n";
        ss << "  ✅ 眼震模式识别与预测\n";

        return ss.str();
    }

    void reset() {
        xTracker.reset();
        outlierFilter.reset();
        predictionBuffer.clear();
        stats.reset();
    }

    // 性能指标
    double getFilterAccuracy() const {
        int goodFilter = 0;
        for (float error : stats.filterErrors) {
            if (error < 15.0) goodFilter++;
        }
        return stats.filterErrors.empty() ? 0 : goodFilter * 100.0 / stats.filterErrors.size();
    }

    double getPredictionAccuracy() const {
        int goodPrediction = 0;
        for (float error : stats.predictionErrors) {
            if (error < 15.0) goodPrediction++;
        }
        return stats.predictionErrors.empty() ? 0 : goodPrediction * 100.0 / stats.predictionErrors.size();
    }

    double getRecentFilterError() const {
        return stats.getAvgFilterError();
    }

    double getRecentPredictionError() const {
        return stats.getAvgPredictionError();
    }

    // 眼震参数
    bool isNystagmusDetected() const {
        return xTracker.isNystagmusDetected();
    }

    double getNystagmusFrequency() const {
        return xTracker.getNystagmusFrequency();
    }

    double getNystagmusAmplitude() const {
        return xTracker.getNystagmusAmplitude();
    }

    // ⭐ 获取当前运动状态
    double getCurrentVelocity() const {
        return xTracker.getCurrentVelocity();
    }

    double getCurrentAcceleration() const {
        return xTracker.getCurrentAcceleration();
    }
};

#endif // PARALLEL_NYSTAGMUS_PIPLINE_H
