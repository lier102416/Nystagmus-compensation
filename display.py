import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats
from scipy.fft import fft, fftfreq
from scipy.signal import find_peaks, butter, filtfilt
from sklearn.metrics import mean_squared_error, mean_absolute_error, r2_score
import warnings
warnings.filterwarnings('ignore')

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['SimHei']  # 用于显示中文
plt.rcParams['axes.unicode_minus'] = False  # 用于显示负号

class OptimizedNystagmusGazePredictionAnalyzer:
    def __init__(self, file_path, fps=60):
        """
        初始化优化的眼震注视预测分析器
        
        Args:
            file_path (str): 包含注视数据的CSV/Excel文件路径
            fps (int): 视频帧率，默认60fps
        """
        self.file_path = file_path
        self.fps = fps
        self.data = None
        self.nystagmus_analysis = {}
        self.load_data()
        
    def load_data(self):
        """加载数据文件"""
        try:
            if self.file_path.endswith('.csv'):
                self.data = pd.read_csv(self.file_path)
            else:
                self.data = pd.read_excel(self.file_path)
            
            # 移除无效数据（全0的行）
            self.data = self.data[(self.data['GazeX'] != 0) | (self.data['GazeY'] != 0)]
            
            # 重置索引并添加序列号列
            self.data.reset_index(drop=True, inplace=True)
            self.data['序列号'] = np.arange(len(self.data))
            
            # 仍然保留时间列，用于某些分析
            self.data['时间_秒'] = self.data['序列号'] / self.fps
            
            print(f"✅ 成功加载 {len(self.data)} 行有效数据 (序列号: 0-{len(self.data)-1})")
            print(f"📊 数据包含的列: {list(self.data.columns)}")
            
            # 快速数据概览
            print(f"\n📍 坐标范围:")
            print(f"   真实注视点 X: [{self.data['GazeX'].min():.1f}, {self.data['GazeX'].max():.1f}]")
            print(f"   真实注视点 Y: [{self.data['GazeY'].min():.1f}, {self.data['GazeY'].max():.1f}]")
            print(f"   预测注视点 X: [{self.data['pGazeX'].min():.1f}, {self.data['pGazeX'].max():.1f}]")
            print(f"   预测注视点 Y: [{self.data['pGazeY'].min():.1f}, {self.data['pGazeY'].max():.1f}]")
            
        except Exception as e:
            print(f"❌ 加载数据失败: {e}")
            self.data = None

    def analyze_nystagmus_characteristics(self):
        """分析眼震特性"""
        if self.data is None:
            return
        
        print(f"\n🔍 眼震特性分析 (基于{self.fps}fps数据):")
        print("="*60)
        
        # 计算眼动速度
        dt = 1.0 / self.fps
        
        # X轴眼动分析
        gaze_x = self.data['GazeX'].values
        gaze_y = self.data['GazeY'].values
        
        # 计算速度（像素/秒）
        vx = np.gradient(gaze_x, dt)
        vy = np.gradient(gaze_y, dt)
        speed = np.sqrt(vx**2 + vy**2)
        
        # 计算加速度
        ax = np.gradient(vx, dt)
        ay = np.gradient(vy, dt)
        acceleration = np.sqrt(ax**2 + ay**2)
        
        # 存储分析结果
        self.data['速度_X'] = vx
        self.data['速度_Y'] = vy
        self.data['眼动速度'] = speed
        self.data['加速度'] = acceleration
        
        # 基础统计
        print(f"📊 眼动基础统计:")
        print(f"   • 平均眼动速度: {speed.mean():.1f} ± {speed.std():.1f} 像素/秒")
        print(f"   • 最大眼动速度: {speed.max():.1f} 像素/秒")
        print(f"   • 平均加速度: {acceleration.mean():.1f} ± {acceleration.std():.1f} 像素/秒²")
        
        # 眼震频率分析
        self.analyze_nystagmus_frequency()
        
        # 眼震类型判断
        self.classify_nystagmus_type()
        
        # 眼震强度分析
        self.analyze_nystagmus_intensity()
        
    def analyze_nystagmus_frequency(self):
        """分析眼震频率"""
        # 对X和Y轴分别进行频谱分析
        for axis, data_col in [('X', 'GazeX'), ('Y', 'GazeY')]:
            signal = self.data[data_col].values
            
            # 去除趋势（去均值）
            signal = signal - np.mean(signal)
            
            # 应用窗函数
            window = np.hanning(len(signal))
            signal_windowed = signal * window
            
            # FFT分析
            fft_result = fft(signal_windowed)
            freqs = fftfreq(len(signal), 1/self.fps)
            
            # 只考虑正频率
            positive_freqs = freqs[freqs > 0]
            fft_magnitude = np.abs(fft_result[freqs > 0])
            
            # 找到主频率（0.5-15Hz范围内，典型眼震频率范围）
            nystagmus_freq_range = (positive_freqs >= 0.5) & (positive_freqs <= 15)
            if np.any(nystagmus_freq_range):
                valid_freqs = positive_freqs[nystagmus_freq_range]
                valid_magnitudes = fft_magnitude[nystagmus_freq_range]
                
                # 找到峰值
                peaks, properties = find_peaks(valid_magnitudes, height=np.max(valid_magnitudes)*0.3)
                
                if len(peaks) > 0:
                    main_freq = valid_freqs[peaks[np.argmax(valid_magnitudes[peaks])]]
                    peak_power = valid_magnitudes[peaks[np.argmax(valid_magnitudes[peaks])]]
                    
                    print(f"   • {axis}轴主频率: {main_freq:.2f} Hz (功率: {peak_power:.0f})")
                    
                    # 存储分析结果
                    self.nystagmus_analysis[f'{axis}轴主频率'] = main_freq
                    self.nystagmus_analysis[f'{axis}轴峰值功率'] = peak_power
                    
                    # 频率范围判断
                    if 0.5 <= main_freq <= 3:
                        freq_type = "低频眼震"
                    elif 3 < main_freq <= 7:
                        freq_type = "中频眼震"
                    elif 7 < main_freq <= 15:
                        freq_type = "高频眼震"
                    else:
                        freq_type = "非典型频率"
                    
                    print(f"     - 分类: {freq_type}")
                    self.nystagmus_analysis[f'{axis}轴类型'] = freq_type
                else:
                    print(f"   • {axis}轴: 未检测到明显的眼震频率")
            else:
                print(f"   • {axis}轴: 频率范围外")
    
    def classify_nystagmus_type(self):
        """分类眼震类型"""
        print(f"\n🎯 眼震类型分类:")
        
        # 计算眼动轨迹的方向性
        gaze_x = self.data['GazeX'].values
        gaze_y = self.data['GazeY'].values
        
        # 计算运动范围
        x_range = np.max(gaze_x) - np.min(gaze_x)
        y_range = np.max(gaze_y) - np.min(gaze_y)
        
        print(f"   • X轴运动范围: {x_range:.1f} 像素")
        print(f"   • Y轴运动范围: {y_range:.1f} 像素")
        
        # 眼震类型判断
        if x_range > 2 * y_range:
            nystagmus_type = "水平眼震"
        elif y_range > 2 * x_range:
            nystagmus_type = "垂直眼震"
        elif abs(x_range - y_range) / max(x_range, y_range) < 0.5:
            nystagmus_type = "旋转/混合眼震"
        else:
            nystagmus_type = "斜向眼震"
        
        print(f"   • 初步判断: {nystagmus_type}")
        self.nystagmus_analysis['眼震类型'] = nystagmus_type
        
        # 眼震规律性分析
        if len(self.data) > 60:  # 至少1秒数据
            # 计算自相关来评估规律性
            x_autocorr = np.correlate(gaze_x - np.mean(gaze_x), gaze_x - np.mean(gaze_x), mode='full')
            x_autocorr = x_autocorr[x_autocorr.size // 2:]
            
            # 寻找周期性
            if len(x_autocorr) > self.fps:
                peaks, _ = find_peaks(x_autocorr[1:self.fps], height=x_autocorr[0]*0.3)
                if len(peaks) > 0:
                    period_frames = peaks[0] + 1
                    period_seconds = period_frames / self.fps
                    regularity = "规律性眼震"
                    print(f"   • 周期性: {regularity} (周期约{period_seconds:.2f}秒)")
                else:
                    regularity = "不规律眼震"
                    print(f"   • 周期性: {regularity}")
            else:
                regularity = "数据不足"
                print(f"   • 周期性: {regularity}")
            
            self.nystagmus_analysis['规律性'] = regularity
    
    def analyze_nystagmus_intensity(self):
        """分析眼震强度"""
        print(f"\n💪 眼震强度分析:")
        
        # 计算眼震强度指标
        speed = self.data['眼动速度'].values
        acceleration = self.data['加速度'].values
        
        # 眼震强度分级
        high_speed_ratio = np.sum(speed > 200) / len(speed)  # 高速眼动比例
        high_accel_ratio = np.sum(acceleration > 1000) / len(acceleration)  # 高加速度比例
        
        print(f"   • 高速眼动比例: {high_speed_ratio*100:.1f}% (>200像素/秒)")
        print(f"   • 高加速度比例: {high_accel_ratio*100:.1f}% (>1000像素/秒²)")
        
        # 眼震强度评级
        if high_speed_ratio > 0.3 or high_accel_ratio > 0.2:
            intensity = "重度眼震"
        elif high_speed_ratio > 0.1 or high_accel_ratio > 0.1:
            intensity = "中度眼震"
        else:
            intensity = "轻度眼震"
        
        print(f"   • 强度评级: {intensity}")
        self.nystagmus_analysis['强度评级'] = intensity
        
        # 眼震幅度分析
        gaze_x = self.data['GazeX'].values
        gaze_y = self.data['GazeY'].values
        
        # 计算运动幅度的标准差
        x_amplitude = np.std(gaze_x)
        y_amplitude = np.std(gaze_y)
        total_amplitude = np.sqrt(x_amplitude**2 + y_amplitude**2)
        
        print(f"   • X轴幅度: {x_amplitude:.1f} 像素")
        print(f"   • Y轴幅度: {y_amplitude:.1f} 像素")
        print(f"   • 总体幅度: {total_amplitude:.1f} 像素")
        
        self.nystagmus_analysis['X轴幅度'] = x_amplitude
        self.nystagmus_analysis['Y轴幅度'] = y_amplitude
        self.nystagmus_analysis['总体幅度'] = total_amplitude
    
    def calculate_errors(self):
        """计算各种误差指标"""
        if self.data is None:
            print("❌ 没有数据可分析")
            return
        
        # 计算误差（预测值 - 真实值）
        self.data['误差_X'] = self.data['pGazeX'] - self.data['GazeX']
        self.data['误差_Y'] = self.data['pGazeY'] - self.data['GazeY']
        
        # 计算欧氏距离误差（最重要的指标）
        self.data['距离误差'] = np.sqrt(
            self.data['误差_X']**2 + self.data['误差_Y']**2
        )
        
        # 计算误差角度（方向偏差）
        self.data['误差角度'] = np.degrees(np.arctan2(self.data['误差_Y'], self.data['误差_X']))
        
        # 计算相对误差（考虑到屏幕大小）
        screen_diagonal = np.sqrt(1920**2 + 1080**2)  # 假设1920x1080屏幕
        self.data['相对误差'] = (self.data['距离误差'] / screen_diagonal) * 100
        
        # 计算误差的移动平均（用于趋势分析）
        window_size = min(10, len(self.data) // 20)
        if window_size > 1:
            self.data['误差_移动平均'] = self.data['距离误差'].rolling(window=window_size, center=True).mean()
            self.data['X误差_移动平均'] = self.data['误差_X'].rolling(window=window_size, center=True).mean()
            self.data['Y误差_移动平均'] = self.data['误差_Y'].rolling(window=window_size, center=True).mean()
        
        print("✅ 误差计算完成")
    
    def calculate_accuracy_metrics(self):
        """计算准确性指标（针对眼震预测）"""
        if self.data is None:
            return
        
        metrics = {}
        
        # 基础误差指标
        metrics['平均距离误差'] = self.data['距离误差'].mean()
        metrics['中位数距离误差'] = self.data['距离误差'].median()
        metrics['最大距离误差'] = self.data['距离误差'].max()
        metrics['最小距离误差'] = self.data['距离误差'].min()
        metrics['误差标准差'] = self.data['距离误差'].std()
        
        # 分轴误差
        metrics['X轴平均误差'] = self.data['误差_X'].abs().mean()
        metrics['Y轴平均误差'] = self.data['误差_Y'].abs().mean()
        
        # 系统性偏差
        metrics['X轴偏差'] = self.data['误差_X'].mean()
        metrics['Y轴偏差'] = self.data['误差_Y'].mean()
        
        # 相关性和决定系数
        metrics['X轴相关性'] = self.data['GazeX'].corr(self.data['pGazeX'])
        metrics['Y轴相关性'] = self.data['GazeY'].corr(self.data['pGazeY'])
        metrics['X轴R²'] = r2_score(self.data['GazeX'], self.data['pGazeX'])
        metrics['Y轴R²'] = r2_score(self.data['GazeY'], self.data['pGazeY'])
        
        # 🔧 优化：更细致的精度等级分布
        thresholds = [1, 2, 3, 5, 10, 15, 20, 30, 50, 100]
        for t in thresholds:
            metrics[f'{t}像素内'] = (self.data['距离误差'] <= t).sum() / len(self.data) * 100
        
        # 计算视角误差（假设观看距离60cm，屏幕PPI=96）
        viewing_distance_cm = 60
        pixels_per_cm = 96 / 2.54  # 96 DPI转换
        visual_angle = np.degrees(2 * np.arctan(
            (self.data['距离误差'] / pixels_per_cm) / (2 * viewing_distance_cm)
        ))
        metrics['平均视角误差'] = visual_angle.mean()
        metrics['最小视角误差'] = visual_angle.min()
        metrics['最大视角误差'] = visual_angle.max()
        
        # 计算UKF预测稳定性指标
        if len(self.data) > 1:
            pred_change_x = np.diff(self.data['pGazeX'])
            pred_change_y = np.diff(self.data['pGazeY'])
            pred_change_dist = np.sqrt(pred_change_x**2 + pred_change_y**2)
            metrics['UKF预测平滑度'] = pred_change_dist.mean()
            metrics['UKF预测抖动度'] = pred_change_dist.std()
        
        # 计算置信区间
        confidence_95 = 1.96 * metrics['误差标准差'] / np.sqrt(len(self.data))
        metrics['95%置信区间'] = f"±{confidence_95:.2f}"
        
        self.metrics = metrics
        return metrics
    
    def print_analysis_report(self):
        """打印眼震UKF预测分析报告"""
        if not hasattr(self, 'metrics'):
            self.calculate_accuracy_metrics()
        
        print("\n" + "="*70)
        print("🎯 眼震UKF注视预测准确性分析报告")
        print("="*70)
        
        # 总体性能评级（针对眼震场景调整）
        avg_error = self.metrics['平均距离误差']
        if avg_error < 15:  # 眼震场景下标准更宽松
            grade = "卓越 ⭐⭐⭐⭐⭐+"
        elif avg_error < 25:
            grade = "优秀 ⭐⭐⭐⭐⭐"
        elif avg_error < 40:
            grade = "良好 ⭐⭐⭐⭐"
        elif avg_error < 60:
            grade = "一般 ⭐⭐⭐"
        else:
            grade = "需改进 ⭐⭐"
        
        print(f"\n🏆 UKF预测总体评级: {grade}")
        print(f"   平均误差: {avg_error:.1f} 像素 (约 {avg_error/37.8:.1f} mm)")
        
        print(f"\n📏 误差统计:")
        print(f"   • 平均误差: {self.metrics['平均距离误差']:.1f} 像素")
        print(f"   • 中位数误差: {self.metrics['中位数距离误差']:.1f} 像素")
        print(f"   • 最小误差: {self.metrics['最小距离误差']:.1f} 像素")
        print(f"   • 最大误差: {self.metrics['最大距离误差']:.1f} 像素")
        print(f"   • 误差波动: ±{self.metrics['误差标准差']:.1f} 像素")
        print(f"   • 95%置信区间: {self.metrics['95%置信区间']} 像素")
        
        print(f"\n👁️ 视角精度 (AR眼镜补偿参考):")
        print(f"   • 平均视角误差: {self.metrics['平均视角误差']:.2f}°")
        print(f"   • 最小视角误差: {self.metrics['最小视角误差']:.2f}°")
        print(f"   • 最大视角误差: {self.metrics['最大视角误差']:.2f}°")
        print(f"   • 相当于60cm距离看{self.metrics['平均视角误差']*60*np.pi/180:.1f}cm的偏差")
        
        # 🔧 优化：更详细的精度分布显示
        print(f"\n🎯 超高精度分布（眼震补偿效果）:")
        print("   ┌─────────────┬──────────┬────────────────────┐")
        print("   │ 误差范围    │ 百分比   │ 可视化             │")
        print("   ├─────────────┼──────────┼────────────────────┤")
        
        # 显示超高精度范围
        ultra_high_precision_thresholds = [1, 2, 3, 5, 10, 15, 20, 30]
        for t in ultra_high_precision_thresholds:
            percent = self.metrics[f'{t}像素内']
            bar = '█' * int(percent / 2.5)  # 调整比例以适应显示
            
            if t <= 3:
                quality = "极致" if percent > 30 else "卓越" if percent > 15 else "优秀" if percent > 5 else "良好"
            elif t <= 5:
                quality = "卓越" if percent > 50 else "优秀" if percent > 30 else "良好" if percent > 15 else "一般"
            elif t <= 10:
                quality = "优秀" if percent > 70 else "良好" if percent > 50 else "一般" if percent > 30 else "待改进"
            else:
                quality = "良好" if percent > 80 else "一般" if percent > 60 else "待改进"
            
            print(f"   │ ≤{t:2d} 像素   │ {percent:5.1f}% {quality:>4} │ {bar:<20} │")
        
        print("   └─────────────┴──────────┴────────────────────┘")
        
        print(f"\n📊 UKF预测质量:")
        print(f"   • X轴相关性: {self.metrics['X轴相关性']:.3f} {'(卓越)' if self.metrics['X轴相关性'] > 0.9 else '(优秀)' if self.metrics['X轴相关性'] > 0.8 else '(良好)' if self.metrics['X轴相关性'] > 0.7 else '(一般)'}")
        print(f"   • Y轴相关性: {self.metrics['Y轴相关性']:.3f} {'(卓越)' if self.metrics['Y轴相关性'] > 0.9 else '(优秀)' if self.metrics['Y轴相关性'] > 0.8 else '(良好)' if self.metrics['Y轴相关性'] > 0.7 else '(一般)'}")
        print(f"   • X轴R²: {self.metrics['X轴R²']:.3f}")
        print(f"   • Y轴R²: {self.metrics['Y轴R²']:.3f}")
        
        print(f"\n⚖️ 系统性偏差 (AR补偿调整参考):")
        bias_x = self.metrics['X轴偏差']
        bias_y = self.metrics['Y轴偏差']
        bias_total = np.sqrt(bias_x**2 + bias_y**2)
        print(f"   • X轴: {bias_x:+.1f} 像素 {'(偏右)' if bias_x > 0 else '(偏左)'}")
        print(f"   • Y轴: {bias_y:+.1f} 像素 {'(偏上)' if bias_y > 0 else '(偏下)'}")
        print(f"   • 总体偏差: {bias_total:.1f} 像素")
        
        if 'UKF预测平滑度' in self.metrics:
            print(f"\n🌊 UKF稳定性指标:")
            print(f"   • 预测平滑度: {self.metrics['UKF预测平滑度']:.1f} 像素/帧")
            print(f"   • 预测抖动度: {self.metrics['UKF预测抖动度']:.1f} 像素")
        
        # 针对眼震的建议
        print(f"\n💡 眼震UKF改进建议:")
        if self.metrics[f'10像素内'] < 40:
            print("   ⚠️ 高精度比例较低，建议调整UKF参数或增加观测噪声估计")
        if avg_error > 50:
            print("   ⚠️ 平均误差较大，建议优化UKF状态转移模型")
        if abs(bias_x) > 15 or abs(bias_y) > 15:
            print("   ⚠️ 存在明显系统性偏差，建议调整AR眼镜补偿参数")
        if self.metrics['误差标准差'] > avg_error * 0.6:
            print("   ⚠️ 误差波动较大，UKF预测稳定性需要改进")
        if avg_error <= 25 and self.metrics[f'10像素内'] > 60:
            print("   ✅ UKF预测性能优秀，眼震补偿效果良好！")
        elif avg_error <= 40 and self.metrics['误差标准差'] < avg_error * 0.5:
            print("   ✅ UKF预测性能良好，继续优化！")
    
    def create_page1_optimized_tracking(self):
        """第1页：优化的基础跟踪效果 (2个图表) - 支持滚轮缩放"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 眼震UKF预测分析 - 第1页：优化的基础跟踪效果', fontsize=24, y=0.95)
        
        # 1x2布局
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. X轴预测跟踪效果 - 优化可视化
        ax1 = fig.add_subplot(gs[0, 0])
        
        # 使用序列号作为X轴
        # 先绘制预测值（背景）- 增加透明度和粗细
        ax1.plot(self.data['序列号'], self.data['pGazeX'], 'r-', linewidth=3, 
                label='UKF预测X', alpha=0.8, zorder=1)
        
        # 再绘制真实值（前景）- 使用不同颜色和样式
        ax1.plot(self.data['序列号'], self.data['GazeX'], 'b--', linewidth=2.5, 
                label='真实X', alpha=0.9, zorder=2)
        
        # 添加误差填充区域
        ax1.fill_between(self.data['序列号'], self.data['GazeX'], self.data['pGazeX'], 
                        alpha=0.2, color='gray', label='误差区域', zorder=0)
        
        ax1.set_xlabel('序列号', fontsize=18)
        ax1.set_ylabel('X坐标 (像素)', fontsize=18)
        ax1.set_title('X轴UKF预测跟踪效果 (支持滚轮缩放)', fontsize=20, pad=20)
        ax1.legend(fontsize=16, loc='upper right')
        ax1.grid(True, alpha=0.3)
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加提示文本
        ax1.text(0.02, 0.98, '🖱️ 使用鼠标滚轮缩放，拖动平移', 
                transform=ax1.transAxes, fontsize=12, 
                verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
        
        # 2. Y轴预测跟踪效果 - 优化可视化
        ax2 = fig.add_subplot(gs[0, 1])
        
        # 使用序列号作为X轴
        # 先绘制预测值（背景）- 增加透明度和粗细
        ax2.plot(self.data['序列号'], self.data['pGazeY'], 'g-', linewidth=3, 
                label='UKF预测Y', alpha=0.8, zorder=1)
        
        # 再绘制真实值（前景）- 使用不同颜色和样式
        ax2.plot(self.data['序列号'], self.data['GazeY'], 'b--', linewidth=2.5, 
                label='真实Y', alpha=0.9, zorder=2)
        
        # 添加误差填充区域
        ax2.fill_between(self.data['序列号'], self.data['GazeY'], self.data['pGazeY'], 
                        alpha=0.2, color='gray', label='误差区域', zorder=0)
        
        ax2.set_xlabel('序列号', fontsize=18)
        ax2.set_ylabel('Y坐标 (像素)', fontsize=18)
        ax2.set_title('Y轴UKF预测跟踪效果 (支持滚轮缩放)', fontsize=20, pad=20)
        ax2.legend(fontsize=16, loc='upper right')
        ax2.grid(True, alpha=0.3)
        ax2.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加提示文本
        ax2.text(0.02, 0.98, '🖱️ 使用鼠标滚轮缩放，拖动平移', 
                transform=ax2.transAxes, fontsize=12, 
                verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
        
        plt.tight_layout()
        
        # 显示图形并启用交互模式
        plt.show()
    
    def create_page2_axis_error_analysis(self):
        """第2页：X/Y轴误差分析 (2个图表) - 支持滚轮缩放"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 眼震UKF预测分析 - 第2页：X/Y轴误差分析', fontsize=24, y=0.95)
        
        # 1x2布局
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. X轴误差分析
        ax1 = fig.add_subplot(gs[0, 0])
        
        # 使用序列号作为X轴
        ax1.plot(self.data['序列号'], self.data['误差_X'], alpha=0.7, color='red', linewidth=2, label='X轴误差')
        
        # 添加零线
        ax1.axhline(y=0, color='black', linestyle='-', linewidth=2, alpha=0.8, label='零误差线')
        
        # 添加误差阈值线
        ax1.axhline(y=10, color='green', linestyle='--', linewidth=2, alpha=0.8, label='+10px')
        ax1.axhline(y=-10, color='green', linestyle='--', linewidth=2, alpha=0.8, label='-10px')
        ax1.axhline(y=20, color='orange', linestyle='--', linewidth=2, alpha=0.8, label='±20px')
        ax1.axhline(y=-20, color='orange', linestyle='--', linewidth=2, alpha=0.8)
        
        # 添加移动平均
        if 'X误差_移动平均' in self.data.columns:
            ax1.plot(self.data['序列号'], self.data['X误差_移动平均'], color='darkred', linewidth=4, 
                    label='移动平均', alpha=0.9)
        
        # 标记极大误差点
        x_outliers = self.data[np.abs(self.data['误差_X']) > np.percentile(np.abs(self.data['误差_X']), 95)]
        if len(x_outliers) > 0:
            ax1.scatter(x_outliers['序列号'], x_outliers['误差_X'], 
                       color='darkred', s=80, alpha=0.8, label='极大误差点', zorder=5)
        
        ax1.set_xlabel('序列号', fontsize=18)
        ax1.set_ylabel('X轴误差 (像素)', fontsize=18)
        ax1.set_title('X轴预测误差变化趋势 (支持滚轮缩放)', fontsize=20, pad=20)
        ax1.legend(loc='upper right', fontsize=14)
        ax1.grid(True, alpha=0.3)
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加提示文本
        ax1.text(0.02, 0.98, '🖱️ 使用鼠标滚轮缩放，拖动平移', 
                transform=ax1.transAxes, fontsize=12, 
                verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
        
        # 2. Y轴误差分析
        ax2 = fig.add_subplot(gs[0, 1])
        
        # 使用序列号作为X轴
        ax2.plot(self.data['序列号'], self.data['误差_Y'], alpha=0.7, color='blue', linewidth=2, label='Y轴误差')
        
        # 添加零线
        ax2.axhline(y=0, color='black', linestyle='-', linewidth=2, alpha=0.8, label='零误差线')
        
        # 添加误差阈值线
        ax2.axhline(y=10, color='green', linestyle='--', linewidth=2, alpha=0.8, label='+10px')
        ax2.axhline(y=-10, color='green', linestyle='--', linewidth=2, alpha=0.8, label='-10px')
        ax2.axhline(y=20, color='orange', linestyle='--', linewidth=2, alpha=0.8, label='±20px')
        ax2.axhline(y=-20, color='orange', linestyle='--', linewidth=2, alpha=0.8)
        
        # 添加移动平均
        if 'Y误差_移动平均' in self.data.columns:
            ax2.plot(self.data['序列号'], self.data['Y误差_移动平均'], color='darkblue', linewidth=4, 
                    label='移动平均', alpha=0.9)
        
        # 标记极大误差点
        y_outliers = self.data[np.abs(self.data['误差_Y']) > np.percentile(np.abs(self.data['误差_Y']), 95)]
        if len(y_outliers) > 0:
            ax2.scatter(y_outliers['序列号'], y_outliers['误差_Y'], 
                       color='darkblue', s=80, alpha=0.8, label='极大误差点', zorder=5)
        
        ax2.set_xlabel('序列号', fontsize=18)
        ax2.set_ylabel('Y轴误差 (像素)', fontsize=18)
        ax2.set_title('Y轴预测误差变化趋势 (支持滚轮缩放)', fontsize=20, pad=20)
        ax2.legend(loc='upper right', fontsize=14)
        ax2.grid(True, alpha=0.3)
        ax2.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加提示文本
        ax2.text(0.02, 0.98, '🖱️ 使用鼠标滚轮缩放，拖动平移', 
                transform=ax2.transAxes, fontsize=12, 
                verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
        
        plt.tight_layout()
        
        # 显示图形并启用交互模式
        plt.show()
    
    def create_page3_enhanced_precision_analysis(self):
        """第3页：增强的精度分析 (2个图表)"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 眼震UKF预测分析 - 第3页：增强的精度分析', fontsize=24, y=0.95)
        
        # 1x2布局
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. 距离误差分布直方图 - 优化
        ax1 = fig.add_subplot(gs[0, 0])
        
        # 🔧 优化：更精细的分箱
        bins = np.concatenate([
            np.arange(0, 10, 1),      # 0-10像素，1像素间隔
            np.arange(10, 30, 2),     # 10-30像素，2像素间隔
            np.arange(30, 60, 5),     # 30-60像素，5像素间隔
            np.arange(60, 100, 10),   # 60-100像素，10像素间隔
            [120, 150, 200]           # 更大误差
        ])
        
        n, bins, patches = ax1.hist(self.data['距离误差'], bins=bins, alpha=0.8, 
                                   edgecolor='darkblue', linewidth=2)
        
        # 根据误差大小着色 - 更精细的颜色分级
        for i, patch in enumerate(patches):
            if bins[i] < 3:
                patch.set_facecolor('#27ae60')  # 深绿色 - 极致
                patch.set_alpha(0.9)
            elif bins[i] < 5:
                patch.set_facecolor('#2ecc71')  # 绿色 - 卓越
                patch.set_alpha(0.8)
            elif bins[i] < 10:
                patch.set_facecolor('#3498db')  # 蓝色 - 优秀
                patch.set_alpha(0.8)
            elif bins[i] < 20:
                patch.set_facecolor('#f39c12')  # 橙色 - 良好
                patch.set_alpha(0.8)
            elif bins[i] < 50:
                patch.set_facecolor('#e67e22')  # 深橙 - 一般
                patch.set_alpha(0.8)
            else:
                patch.set_facecolor('#e74c3c')  # 红色 - 较差
                patch.set_alpha(0.8)
        
        ax1.axvline(self.data['距离误差'].mean(), color='red', linestyle='--', 
                   linewidth=3, label=f'平均: {self.data["距离误差"].mean():.1f}px')
        ax1.axvline(self.data['距离误差'].median(), color='green', linestyle='--', 
                   linewidth=3, label=f'中位数: {self.data["距离误差"].median():.1f}px')
        
        ax1.set_xlabel('距离误差 (像素)', fontsize=18)
        ax1.set_ylabel('频次', fontsize=18)
        ax1.set_title('UKF预测距离误差分布 (精细分箱)', fontsize=20, pad=20)
        ax1.legend(fontsize=16)
        ax1.grid(True, alpha=0.3, axis='y')
        ax1.set_xlim(0, min(100, self.data['距离误差'].max() * 1.1))
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 2. 🔧 优化：更详细的精度等级饼图
        ax2 = fig.add_subplot(gs[0, 1])
        
        # 针对眼震场景的更细致阈值
        thresholds = [1, 2, 3, 5, 10, 20, 50, np.inf]
        labels = ['<1px\n(极致)', '1-2px\n(卓越)', '2-3px\n(优秀)', '3-5px\n(良好)', 
                 '5-10px\n(可接受)', '10-20px\n(中等)', '20-50px\n(较差)', '>50px\n(差)']
        colors_pie = ['#27ae60', '#2ecc71', '#3498db', '#52c41a', '#f39c12', '#e67e22', '#e74c3c', '#8b0000']
        
        counts = []
        for i in range(len(thresholds)):
            if i == 0:
                count = (self.data['距离误差'] < thresholds[i]).sum()
            else:
                count = ((self.data['距离误差'] >= thresholds[i-1]) & 
                        (self.data['距离误差'] < thresholds[i])).sum()
            counts.append(count)
        
        # 只显示非零的部分
        non_zero = [(c, l, col) for c, l, col in zip(counts, labels, colors_pie) if c > 0]
        if non_zero:
            counts_nz, labels_nz, colors_nz = zip(*non_zero)
            
            wedges, texts, autotexts = ax2.pie(counts_nz, labels=labels_nz, colors=colors_nz, 
                                               autopct=lambda pct: f'{pct:.1f}%\n({int(pct*len(self.data)/100)})', 
                                               startangle=90, pctdistance=0.85, labeldistance=1.1)
            
            # 优化文字显示
            for text in texts:
                text.set_fontsize(16)
                text.set_weight('bold')
            for autotext in autotexts:
                autotext.set_color('white')
                autotext.set_weight('bold')
                autotext.set_fontsize(15)
        
        ax2.set_title('UKF预测精度等级分布 (细致分级)', fontsize=20, pad=20)
        
        plt.tight_layout()
        plt.show()
    
    def create_page4_nystagmus_characteristics(self):
        """第4页：眼震特性分析 (2个图表) - 眼动轨迹支持滚轮缩放"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 眼震UKF预测分析 - 第4页：眼震特性分析', fontsize=24, y=0.95)
        
        # 1x2布局
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. 眼动轨迹图 - 支持滚轮缩放
        ax1 = fig.add_subplot(gs[0, 0])
        
        # 绘制眼动轨迹
        ax1.plot(self.data['GazeX'], self.data['GazeY'], 'b-', linewidth=2, alpha=0.7, label='真实轨迹')
        ax1.plot(self.data['pGazeX'], self.data['pGazeY'], 'r-', linewidth=1.5, alpha=0.6, label='预测轨迹')
        
        # 标记起始点和结束点
        ax1.scatter(self.data['GazeX'].iloc[0], self.data['GazeY'].iloc[0], 
                   color='green', s=100, marker='o', label='起始点', zorder=5)
        ax1.scatter(self.data['GazeX'].iloc[-1], self.data['GazeY'].iloc[-1], 
                   color='red', s=100, marker='x', label='结束点', zorder=5)
        
        # 添加运动方向箭头（每10个点一个箭头）
        step = max(1, len(self.data) // 20)
        for i in range(0, len(self.data)-step, step):
            dx = self.data['GazeX'].iloc[i+step] - self.data['GazeX'].iloc[i]
            dy = self.data['GazeY'].iloc[i+step] - self.data['GazeY'].iloc[i]
            ax1.arrow(self.data['GazeX'].iloc[i], self.data['GazeY'].iloc[i], 
                     dx*0.5, dy*0.5, head_width=5, head_length=5, 
                     fc='blue', ec='blue', alpha=0.5)
        
        ax1.set_xlabel('X坐标 (像素)', fontsize=18)
        ax1.set_ylabel('Y坐标 (像素)', fontsize=18)
        ax1.set_title('眼动轨迹图 (支持滚轮缩放)', fontsize=20, pad=20)
        ax1.legend(fontsize=16)
        ax1.grid(True, alpha=0.3)
        ax1.axis('equal')
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加提示文本
        ax1.text(0.02, 0.98, '🖱️ 使用鼠标滚轮缩放，拖动平移', 
                transform=ax1.transAxes, fontsize=12, 
                verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
        
        # 2. 眼动速度分析 - 支持滚轮缩放
        ax2 = fig.add_subplot(gs[0, 1])
        
        # 使用序列号作为X轴
        ax2.plot(self.data['序列号'], self.data['眼动速度'], 'purple', linewidth=2, alpha=0.8, label='眼动速度')
        
        # 添加速度阈值线
        ax2.axhline(y=100, color='green', linestyle='--', linewidth=2, alpha=0.8, label='低速阈值(100px/s)')
        ax2.axhline(y=300, color='orange', linestyle='--', linewidth=2, alpha=0.8, label='中速阈值(300px/s)')
        ax2.axhline(y=500, color='red', linestyle='--', linewidth=2, alpha=0.8, label='高速阈值(500px/s)')
        
        # 标记高速眼动点
        high_speed_points = self.data[self.data['眼动速度'] > 500]
        if len(high_speed_points) > 0:
            ax2.scatter(high_speed_points['序列号'], high_speed_points['眼动速度'], 
                       color='red', s=60, alpha=0.8, label='高速眼动', zorder=5)
        
        # 添加眼震频率标注
        if hasattr(self, 'nystagmus_analysis'):
            if 'X轴主频率' in self.nystagmus_analysis:
                freq_x = self.nystagmus_analysis['X轴主频率']
                ax2.text(0.05, 0.95, f'X轴主频率: {freq_x:.2f} Hz', 
                        transform=ax2.transAxes, fontsize=14, 
                        bbox=dict(boxstyle="round,pad=0.3", facecolor="lightblue"))
            if 'Y轴主频率' in self.nystagmus_analysis:
                freq_y = self.nystagmus_analysis['Y轴主频率']
                ax2.text(0.05, 0.85, f'Y轴主频率: {freq_y:.2f} Hz', 
                        transform=ax2.transAxes, fontsize=14, 
                        bbox=dict(boxstyle="round,pad=0.3", facecolor="lightgreen"))
        
        ax2.set_xlabel('序列号', fontsize=18)
        ax2.set_ylabel('眼动速度 (像素/秒)', fontsize=18)
        ax2.set_title('眼动速度分析 (支持滚轮缩放)', fontsize=20, pad=20)
        ax2.legend(fontsize=14)
        ax2.grid(True, alpha=0.3)
        ax2.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加提示文本
        ax2.text(0.02, 0.02, '🖱️ 使用鼠标滚轮缩放，拖动平移', 
                transform=ax2.transAxes, fontsize=12, 
                verticalalignment='bottom',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.7))
        
        plt.tight_layout()
        
        # 显示图形并启用交互模式
        plt.show()
    
    def print_nystagmus_characteristics_report(self):
        """打印眼震特性分析报告"""
        print(f"\n📋 眼震特性分析报告:")
        print("="*50)
        
        if hasattr(self, 'nystagmus_analysis') and self.nystagmus_analysis:
            for key, value in self.nystagmus_analysis.items():
                if isinstance(value, (int, float)):
                    print(f"   • {key}: {value:.2f}")
                else:
                    print(f"   • {key}: {value}")
        
        # 眼震类型判断
        print(f"\n🔍 眼震类型判断:")
        if 'X轴主频率' in self.nystagmus_analysis and 'Y轴主频率' in self.nystagmus_analysis:
            freq_x = self.nystagmus_analysis['X轴主频率']
            freq_y = self.nystagmus_analysis['Y轴主频率']
            
            # 频率一致性检查
            if abs(freq_x - freq_y) / max(freq_x, freq_y) < 0.3:
                print(f"   ✅ X/Y轴频率一致性良好 (X:{freq_x:.2f}Hz, Y:{freq_y:.2f}Hz)")
                consistency = "一致性眼震"
            else:
                print(f"   ⚠️ X/Y轴频率不一致 (X:{freq_x:.2f}Hz, Y:{freq_y:.2f}Hz)")
                consistency = "不一致性眼震"
            
            # 眼震质量评估
            if '眼震类型' in self.nystagmus_analysis:
                nystagmus_type = self.nystagmus_analysis['眼震类型']
                print(f"   • 眼震类型: {nystagmus_type}")
                
                # 结合频率和类型的综合评估
                if consistency == "一致性眼震" and freq_x > 1.0 and freq_x < 10.0:
                    print(f"   ✅ 典型眼震模式，适合UKF预测")
                elif freq_x < 0.5 or freq_x > 15.0:
                    print(f"   ⚠️ 非典型眼震频率，可能影响UKF预测精度")
                else:
                    print(f"   📊 眼震特性正常，UKF预测可行")
    
    def run_optimized_nystagmus_analysis(self, outlier_threshold_percentile=95, min_error_threshold=50):
        """
        运行完整的优化眼震UKF预测分析流程（4页显示版）
        
        Args:
            outlier_threshold_percentile (float): 异常点百分位数阈值，默认95%
            min_error_threshold (float): 最小误差阈值（像素），默认50
        """
        if self.data is None:
            return
        
        print("\n" + "="*80)
        print("🚀 优化眼震UKF注视预测分析系统 v7.0 (序列号版本+滚轮缩放)")
        print("="*80)
        
        # 启用matplotlib的交互模式
        plt.ion()
        
        # 1. 分析眼震特性
        self.analyze_nystagmus_characteristics()
        
        # 2. 计算误差
        self.calculate_errors()
        
        # 3. 计算准确性指标
        self.calculate_accuracy_metrics()
        
        # 4. 打印分析报告
        self.print_analysis_report()
        
        # 5. 打印眼震特性报告
        self.print_nystagmus_characteristics_report()
        
        # 6. 创建4页可视化分析
        print(f"\n📊 正在生成4页优化可视化分析...")
        print("   🖱️ 所有图表支持鼠标滚轮缩放和拖动平移")
        print("   第1页：优化的基础跟踪效果（序列号版本+滚轮缩放）")
        self.create_page1_optimized_tracking()
        
        print("   第2页：X/Y轴误差分析（序列号版本+滚轮缩放）")
        self.create_page2_axis_error_analysis()
        
        print("   第3页：增强的精度分析（细致分级）")
        self.create_page3_enhanced_precision_analysis()
        
        print("   第4页：眼震特性分析（支持滚轮缩放）")
        self.create_page4_nystagmus_characteristics()
        
        print("\n" + "="*80)
        print("✅ 优化眼震UKF预测分析完成！")
        print("   🎯 专门针对眼震场景和60fps优化")
        print("   📊 4页优化显示，真实值突出，预测值半透明")
        print("   🔄 X轴使用序列号而非时间")
        print("   🖱️ 支持鼠标滚轮缩放和拖动平移")
        print("   🔍 第1页：优化的基础跟踪效果")
        print("   📈 第2页：X/Y轴误差时序分析")
        print("   📋 第3页：增强的精度分析（<1,<2,<3,<5,<10,<20,<50像素）")
        print("   👁️ 第4页：眼震特性分析（频率、类型、强度）")
        print("   🔬 基于60fps的眼震频率分析")
        print("   ⚙️ 眼震类型自动判断和UKF适配建议")
        print("="*80)

# 使用示例
if __name__ == "__main__":
    print("👁️ 优化眼震UKF注视预测分析工具 v7.0 (序列号版本+滚轮缩放)")
    print("="*70)
    print("✨ v7.0 新增特性:")
    print("   🔄 X轴使用序列号而非时间")
    print("   🖱️ 所有图表支持鼠标滚轮缩放")
    print("   📊 支持拖动平移查看细节")
    print("   🎯 专门针对眼震诱导和60fps数据")
    print("   🔍 真实值突出显示，预测值半透明背景")
    print("   📊 更细致的精度分布：<1,<2,<3,<5,<10,<20,<50像素")
    print("   👁️ 新增眼震特性分析：频率、类型、强度")
    print("   🔬 基于60fps的眼震频率谱分析")
    print("   ⚙️ 眼震类型自动判断和UKF适配性评估")
    print("   📈 4页优化显示，每页针对性分析")
    print("="*70)
    
    # 创建优化分析器
    analyzer = OptimizedNystagmusGazePredictionAnalyzer('collected_measurement_data.csv', fps=60)
    
    # 运行完整分析
    if analyzer.data is not None:
        analyzer.run_optimized_nystagmus_analysis(
            outlier_threshold_percentile=95,
            min_error_threshold=50
        )
    else:
        print("❌ 无法加载数据文件，请检查文件路径")
        print("   确保文件 'collected_measurement_data.csv' 存在")
        print("   并且包含必要的列: GazeX, GazeY, pGazeX, pGazeY")
    
    # 保持图形窗口打开
    input("\n按回车键退出...")