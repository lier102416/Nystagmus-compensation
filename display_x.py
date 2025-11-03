import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from scipy import stats
from scipy.fft import fft, fftfreq
from scipy.signal import find_peaks, butter, filtfilt, hilbert, savgol_filter
from scipy.interpolate import interp1d
from sklearn.metrics import mean_squared_error, mean_absolute_error, r2_score
from sklearn.cluster import KMeans
import warnings
warnings.filterwarnings('ignore')

# 设置中文字体
plt.rcParams['font.sans-serif'] = ['SimHei']  # 用于显示中文
plt.rcParams['axes.unicode_minus'] = False  # 用于显示负号

class EnhancedXAxisNystagmusAnalyzer:
    def __init__(self, file_path, fps=60):                                          
        """
        增强版X轴眼震UKF预测分析器
        
        Args:
            file_path (str): 包含预测数据的CSV文件路径
            fps (int): 视频帧率，默认60fps
        """
        self.file_path = file_path
        self.fps = fps
        self.data = None
        self.nystagmus_analysis = {}
        self.reduction_analysis = {}
        self.trajectory_analysis = {}  # 新增：轨迹分析结果
        self.reference_point = None 
        self.load_data()
        
    def load_data(self):
        """加载数据文件"""
        try:
            if self.file_path.endswith('.csv'):
                self.data = pd.read_csv(self.file_path)
            else:
                self.data = pd.read_excel(self.file_path)                                                 
            
            # 检查必要的列
            required_columns = ['frameId', 'actualX', 'predictedX']                                                        
            missing_columns = [col for col in required_columns if col not in self.data.columns]
            if missing_columns:
                print(f"❌ 缺少必要的列: {missing_columns}")
                self.data = None
                return
            
            # 移除无效数据（actualX为NA或0的行）
            self.data = self.data.dropna(subset=['actualX', 'predictedX'])
            self.data = self.data[self.data['actualX'] != 0]
            
            # 重置索引
            self.data.reset_index(drop=True, inplace=True)
            
            # 使用frameId作为序列号
            self.data['序列号'] = self.data['frameId']
            
            # 添加时间列（秒）
            self.data['时间_秒'] = self.data['frameId'] / self.fps
            
            # 如果没有误差列，计算它们
            if 'predictionErrorX' not in self.data.columns:
                self.data['predictionErrorX'] = self.data['actualX'] - self.data['predictedX']
            if 'errorMagnitude' not in self.data.columns:
                self.data['errorMagnitude'] = np.sqrt(
                    self.data['predictionErrorX']**2 + 
                    self.data.get('predictionErrorY', 0)**2
                )
            
            print(f"✅ 成功加载 {len(self.data)} 行有效数据")
            print(f"📊 数据包含的列: {list(self.data.columns)}")
            
            # 快速数据概览
            print(f"\n📍 X轴坐标范围:")
            print(f"   真实注视点 X: [{self.data['actualX'].min():.1f}, {self.data['actualX'].max():.1f}]")
            print(f"   预测注视点 X: [{self.data['predictedX'].min():.1f}, {self.data['predictedX'].max():.1f}]")
            print(f"   帧ID范围: [{self.data['frameId'].min()}, {self.data['frameId'].max()}]")
            
            # 如果有errorMagnitude列，显示预测误差统计
            if 'errorMagnitude' in self.data.columns:
                print(f"\n📊 预测误差初步统计:")
                print(f"   平均误差: {self.data['errorMagnitude'].mean():.2f} 像素")
                print(f"   最大误差: {self.data['errorMagnitude'].max():.2f} 像素")
                print(f"   中位数误差: {self.data['errorMagnitude'].median():.2f} 像素")
            
        except Exception as e:
            print(f"❌ 加载数据失败: {e}")
            self.data = None

    def analyze_nystagmus_trajectory_characteristics(self):
        """深度分析X轴眼震轨迹特性"""
        if self.data is None:
            return
        
        print(f"\n🔍 深度X轴眼震轨迹特性分析 (基于{self.fps}fps数据):")
        print("="*80)
        
        # 计算眼动速度和加速度
        dt = 1.0 / self.fps
        actual_x = self.data['actualX'].values
        
        # 使用Savitzky-Golay滤波平滑数据，减少噪声
        if len(actual_x) > 5:
            smooth_x = savgol_filter(actual_x, window_length=min(5, len(actual_x)), polyorder=2)
        else:
            smooth_x = actual_x
        
        # 计算速度（像素/秒）
        vx = np.gradient(smooth_x, dt)
        speed = np.abs(vx)
        
        # 计算加速度
        ax = np.gradient(vx, dt)
        acceleration = np.abs(ax)
        
        # 存储分析结果
        self.data['速度_X'] = vx
        self.data['平滑_X'] = smooth_x
        self.data['X轴眼动速度'] = speed
        self.data['X轴加速度'] = acceleration
        
        # 1. 眼震类型分析
        self.analyze_nystagmus_type()
        
        # 2. 眼震模式识别
        self.analyze_nystagmus_pattern()
        
        # 3. 眼震频率深度分析
        self.analyze_nystagmus_frequency_enhanced()
        
        # 4. 眼震强度和稳定性分析
        self.analyze_nystagmus_intensity_enhanced()
        
        # 5. 眼震波形特征分析
        self.analyze_nystagmus_waveform()
        
        # 6. 眼震方向性分析
        self.analyze_nystagmus_directionality()
        
    def analyze_nystagmus_type(self):
        """分析眼震类型"""
        print(f"\n🎯 眼震类型分析:")
        
        actual_x = self.data['actualX'].values
        speed = self.data['X轴眼动速度'].values
        
        # 计算运动范围和变异系数
        x_range = np.max(actual_x) - np.min(actual_x)
        x_std = np.std(actual_x)
        x_cv = x_std / np.mean(np.abs(actual_x)) if np.mean(np.abs(actual_x)) > 0 else 0
        
        # 计算速度分布特征
        speed_mean = np.mean(speed)
        speed_std = np.std(speed)
        high_speed_ratio = np.sum(speed > speed_mean + 2*speed_std) / len(speed)
        
        # 眼震类型判定
        nystagmus_type = []
        
        # 基于运动幅度分类
        if x_range < 20:
            amplitude_type = "微幅眼震"
        elif x_range < 50:
            amplitude_type = "小幅眼震"
        elif x_range < 100:
            amplitude_type = "中幅眼震"
        else:
            amplitude_type = "大幅眼震"
        
        # 基于速度特征分类
        if speed_mean < 50:
            velocity_type = "慢速眼震"
        elif speed_mean < 200:
            velocity_type = "中速眼震"
        else:
            velocity_type = "快速眼震"
        
        # 基于规律性分类
        if x_cv < 0.3:
            regularity_type = "规律性眼震"
        elif x_cv < 0.6:
            regularity_type = "半规律性眼震"
        else:
            regularity_type = "不规律眼震"
        
        nystagmus_type = [amplitude_type, velocity_type, regularity_type]
        
        print(f"   • 幅度分类: {amplitude_type} (范围: {x_range:.1f}px)")
        print(f"   • 速度分类: {velocity_type} (平均: {speed_mean:.1f}px/s)")
        print(f"   • 规律性: {regularity_type} (变异系数: {x_cv:.3f})")
        print(f"   • 高速运动比例: {high_speed_ratio*100:.1f}%")
        
        # 存储结果
        self.trajectory_analysis['眼震类型'] = nystagmus_type
        self.trajectory_analysis['运动范围'] = x_range
        self.trajectory_analysis['变异系数'] = x_cv
        self.trajectory_analysis['平均速度'] = speed_mean
        self.trajectory_analysis['高速比例'] = high_speed_ratio
        
    def analyze_nystagmus_pattern(self):
        """分析眼震模式（摆动性、冲动性、混合性）"""
        print(f"\n🌊 眼震模式识别:")
        
        actual_x = self.data['actualX'].values
        vx = self.data['速度_X'].values
        
        # 检测速度方向变化
        velocity_changes = np.diff(np.sign(vx))
        direction_changes = np.sum(np.abs(velocity_changes) > 0)
        change_frequency = direction_changes / (len(actual_x) / self.fps)  # 次/秒
        
        # 检测快相和慢相
        speed = np.abs(vx)
        speed_threshold = np.percentile(speed, 75)  # 75分位数作为阈值
        
        fast_phases = speed > speed_threshold
        slow_phases = speed <= speed_threshold
        
        fast_phase_ratio = np.sum(fast_phases) / len(speed)
        slow_phase_ratio = np.sum(slow_phases) / len(speed)
        
        # 分析速度分布的双峰性
        from scipy.stats import kurtosis, skew
        speed_kurtosis = kurtosis(speed)
        speed_skewness = skew(speed)
        
        # 模式判定
        if change_frequency > 3 and fast_phase_ratio > 0.3:
            if speed_kurtosis > 0 and abs(speed_skewness) > 0.5:
                pattern_type = "冲动性眼震"
                pattern_description = "明显的快相-慢相交替模式"
            else:
                pattern_type = "摆动性眼震"
                pattern_description = "规律的往复摆动模式"
        elif change_frequency > 1.5:
            pattern_type = "混合性眼震"
            pattern_description = "摆动性和冲动性特征并存"
        elif change_frequency < 0.5:
            pattern_type = "漂移性眼震"
            pattern_description = "缓慢单向漂移为主"
        else:
            pattern_type = "不规则眼震"
            pattern_description = "无明显模式特征"
        
        print(f"   • 模式类型: {pattern_type}")
        print(f"   • 特征描述: {pattern_description}")
        print(f"   • 方向变化频率: {change_frequency:.2f} 次/秒")
        print(f"   • 快相比例: {fast_phase_ratio*100:.1f}%")
        print(f"   • 慢相比例: {slow_phase_ratio*100:.1f}%")
        print(f"   • 速度分布峰度: {speed_kurtosis:.3f}")
        print(f"   • 速度分布偏度: {speed_skewness:.3f}")
        
        # 存储结果
        self.trajectory_analysis['眼震模式'] = pattern_type
        self.trajectory_analysis['模式描述'] = pattern_description
        self.trajectory_analysis['方向变化频率'] = change_frequency
        self.trajectory_analysis['快相比例'] = fast_phase_ratio
        self.trajectory_analysis['慢相比例'] = slow_phase_ratio
        
    def analyze_nystagmus_frequency_enhanced(self):
        """增强版眼震频率分析"""
        print(f"\n📡 增强版频率分析:")
        
        signal = self.data['平滑_X'].values
        
        # 去除趋势（去均值）
        signal = signal - np.mean(signal)
        
        # 应用汉宁窗
        window = np.hanning(len(signal))
        signal_windowed = signal * window
        
        # FFT分析
        fft_result = fft(signal_windowed)
        freqs = fftfreq(len(signal), 1/self.fps)
        
        # 只考虑正频率
        positive_freqs = freqs[freqs > 0]
        fft_magnitude = np.abs(fft_result[freqs > 0])
        
        # 功率谱密度
        power_spectrum = fft_magnitude ** 2
        
        # 在眼震典型频率范围内寻找峰值 (0.2-20Hz)
        nystagmus_freq_range = (positive_freqs >= 0.2) & (positive_freqs <= 20)
        if np.any(nystagmus_freq_range):
            valid_freqs = positive_freqs[nystagmus_freq_range]
            valid_magnitudes = fft_magnitude[nystagmus_freq_range]
            valid_power = power_spectrum[nystagmus_freq_range]
            
            # 寻找所有显著峰值
            prominence_threshold = np.max(valid_magnitudes) * 0.2
            peaks, properties = find_peaks(valid_magnitudes, 
                                         prominence=prominence_threshold,
                                         distance=int(0.5 * self.fps / np.mean(np.diff(freqs[freqs > 0]))))
            
            if len(peaks) > 0:
                # 主频率（最高峰值）
                main_peak_idx = peaks[np.argmax(valid_magnitudes[peaks])]
                main_freq = valid_freqs[main_peak_idx]
                main_power = valid_power[main_peak_idx]
                
                print(f"   • 主频率: {main_freq:.3f} Hz (功率: {main_power:.0f})")
                
                # 频率稳定性分析
                freq_stability = self.analyze_frequency_stability(signal)
                
                # 频率分类
                freq_category = self.classify_nystagmus_frequency(main_freq)
                print(f"   • 频率分类: {freq_category}")
                print(f"   • 频率稳定性: {freq_stability:.3f}")
                
                # 查找谐波
                harmonics = []
                for i in range(2, 6):  # 检查2-5次谐波
                    harmonic_freq = main_freq * i
                    if harmonic_freq <= 20:
                        # 在谐波频率附近查找峰值
                        harmonic_range = np.abs(valid_freqs - harmonic_freq) < 0.5
                        if np.any(harmonic_range):
                            harmonic_power = np.max(valid_power[harmonic_range])
                            if harmonic_power > main_power * 0.1:  # 至少是主峰的10%
                                harmonics.append((harmonic_freq, harmonic_power))
                
                if harmonics:
                    print(f"   • 检测到谐波: {len(harmonics)}个")
                    for i, (freq, power) in enumerate(harmonics[:3]):
                        print(f"     - {i+2}次谐波: {freq:.2f} Hz (功率: {power:.0f})")
                
                # 次要频率
                if len(peaks) > 1:
                    sorted_peaks = peaks[np.argsort(valid_magnitudes[peaks])[::-1]]
                    secondary_frequencies = []
                    for peak in sorted_peaks[1:4]:  # 最多显示3个次要频率
                        sec_freq = valid_freqs[peak]
                        sec_power = valid_power[peak]
                        if sec_power > main_power * 0.3:  # 至少是主峰的30%
                            secondary_frequencies.append((sec_freq, sec_power))
                    
                    if secondary_frequencies:
                        print(f"   • 次要频率:")
                        for freq, power in secondary_frequencies:
                            print(f"     - {freq:.3f} Hz (功率: {power:.0f})")
                
                # 频带能量分析
                self.analyze_frequency_bands(valid_freqs, valid_power)
                
                # 存储结果
                self.trajectory_analysis['主频率'] = main_freq
                self.trajectory_analysis['主频率功率'] = main_power
                self.trajectory_analysis['频率稳定性'] = freq_stability
                self.trajectory_analysis['频率分类'] = freq_category
                self.trajectory_analysis['谐波数量'] = len(harmonics)
                self.trajectory_analysis['次要频率数量'] = len(secondary_frequencies) if len(peaks) > 1 else 0
                
            else:
                print(f"   • 未检测到显著的眼震频率峰值")
                self.trajectory_analysis['主频率'] = 0
                self.trajectory_analysis['频率分类'] = "无明显频率特征"
        else:
            print(f"   • 频率分析：信号超出眼震典型频率范围")
            
    def analyze_frequency_stability(self, signal):
        """分析频率稳定性"""
        # 使用短时傅里叶变换分析频率随时间的变化
        window_size = min(int(self.fps * 2), len(signal) // 4)  # 2秒窗口或1/4信号长度
        
        if window_size < 10:
            return 0.0
            
        step_size = window_size // 2
        main_freqs = []
        
        for i in range(0, len(signal) - window_size, step_size):
            window_signal = signal[i:i+window_size]
            window_signal = window_signal - np.mean(window_signal)
            
            fft_result = fft(window_signal)
            freqs = fftfreq(len(window_signal), 1/self.fps)
            
            positive_freqs = freqs[freqs > 0]
            fft_magnitude = np.abs(fft_result[freqs > 0])
            
            freq_range = (positive_freqs >= 0.2) & (positive_freqs <= 20)
            if np.any(freq_range):
                valid_freqs = positive_freqs[freq_range]
                valid_magnitudes = fft_magnitude[freq_range]
                
                if len(valid_magnitudes) > 0:
                    main_freq_idx = np.argmax(valid_magnitudes)
                    main_freqs.append(valid_freqs[main_freq_idx])
        
        if len(main_freqs) > 1:
            freq_stability = 1 - (np.std(main_freqs) / np.mean(main_freqs))
            return max(0, min(1, freq_stability))
        else:
            return 0.0
    
    def classify_nystagmus_frequency(self, freq):
        """对眼震频率进行分类"""
        if freq < 0.5:
            return "超低频眼震 (<0.5Hz)"
        elif freq < 1.5:
            return "低频眼震 (0.5-1.5Hz)"
        elif freq < 3:
            return "中低频眼震 (1.5-3Hz)"
        elif freq < 5:
            return "中频眼震 (3-5Hz)"
        elif freq < 8:
            return "中高频眼震 (5-8Hz)"
        elif freq < 12:
            return "高频眼震 (8-12Hz)"
        else:
            return "超高频眼震 (>12Hz)"
    
    def analyze_frequency_bands(self, freqs, power):
        """分析不同频带的能量分布"""
        bands = {
            "超低频 (<1Hz)": (0, 1),
            "低频 (1-3Hz)": (1, 3),
            "中频 (3-7Hz)": (3, 7),
            "高频 (7-15Hz)": (7, 15),
            "超高频 (>15Hz)": (15, 20)
        }
        
        total_power = np.sum(power)
        band_powers = {}
        
        print(f"   • 频带能量分布:")
        for band_name, (low, high) in bands.items():
            band_mask = (freqs >= low) & (freqs < high)
            band_power = np.sum(power[band_mask])
            band_ratio = (band_power / total_power) * 100 if total_power > 0 else 0
            band_powers[band_name] = band_ratio
            
            if band_ratio > 5:  # 只显示占比超过5%的频带
                print(f"     - {band_name}: {band_ratio:.1f}%")
        
        # 找到主导频带
        dominant_band = max(band_powers, key=band_powers.get)
        print(f"   • 主导频带: {dominant_band} ({band_powers[dominant_band]:.1f}%)")
        
        self.trajectory_analysis['主导频带'] = dominant_band
        self.trajectory_analysis['频带分布'] = band_powers
    
    def analyze_nystagmus_intensity_enhanced(self):
        """增强版眼震强度分析"""
        print(f"\n💪 增强版眼震强度分析:")
        
        actual_x = self.data['actualX'].values
        speed = self.data['X轴眼动速度'].values
        acceleration = self.data['X轴加速度'].values
        
        # 多维度强度评估
        # 1. 位移强度
        displacement_range = np.max(actual_x) - np.min(actual_x)
        displacement_std = np.std(actual_x)
        displacement_rms = np.sqrt(np.mean((actual_x - np.mean(actual_x))**2))
        
        # 2. 速度强度
        speed_mean = np.mean(speed)
        speed_max = np.max(speed)
        speed_95th = np.percentile(speed, 95)
        
        # 3. 加速度强度
        accel_mean = np.mean(acceleration)
        accel_max = np.max(acceleration)
        accel_95th = np.percentile(acceleration, 95)
        
        # 4. 运动复杂性
        # 使用近似熵评估信号复杂性
        complexity = self.calculate_approximate_entropy(actual_x)
        
        # 5. 运动连续性
        # 检测运动中断（速度接近零的时间）
        still_threshold = np.percentile(speed, 10)  # 10分位数作为静止阈值
        still_periods = speed < still_threshold
        continuity = 1 - (np.sum(still_periods) / len(speed))
        
        print(f"   • 位移强度:")
        print(f"     - 运动范围: {displacement_range:.1f} 像素")
        print(f"     - 标准差: {displacement_std:.1f} 像素")
        print(f"     - RMS: {displacement_rms:.1f} 像素")
        
        print(f"   • 速度强度:")
        print(f"     - 平均速度: {speed_mean:.1f} 像素/秒")
        print(f"     - 最大速度: {speed_max:.1f} 像素/秒")
        print(f"     - 95分位速度: {speed_95th:.1f} 像素/秒")
        
        print(f"   • 加速度强度:")
        print(f"     - 平均加速度: {accel_mean:.1f} 像素/秒²")
        print(f"     - 最大加速度: {accel_max:.1f} 像素/秒²")
        print(f"     - 95分位加速度: {accel_95th:.1f} 像素/秒²")
        
        print(f"   • 运动特性:")
        print(f"     - 信号复杂性: {complexity:.3f}")
        print(f"     - 运动连续性: {continuity*100:.1f}%")
        
        # 综合强度评级
        intensity_score = self.calculate_intensity_score(
            displacement_range, speed_mean, accel_mean, complexity, continuity
        )
        
        intensity_level = self.classify_intensity_level(intensity_score)
        print(f"   • 综合强度评级: {intensity_level} (评分: {intensity_score:.2f}/10)")
        
        # 存储结果
        self.trajectory_analysis.update({
            '位移范围': displacement_range,
            '位移标准差': displacement_std,
            '位移RMS': displacement_rms,
            '平均速度': speed_mean,
            '最大速度': speed_max,
            '平均加速度': accel_mean,
            '最大加速度': accel_max,
            '信号复杂性': complexity,
            '运动连续性': continuity,
            '强度评分': intensity_score,
            '强度等级': intensity_level
        })
    
    def calculate_approximate_entropy(self, data, m=2, r=None):
        """计算近似熵，评估信号的复杂性和规律性"""
        N = len(data)
        if r is None:
            r = 0.2 * np.std(data)
        
        def _maxdist(xi, xj, N, m):
            return max([abs(ua - va) for ua, va in zip(xi, xj)])
        
        def _phi(m):
            patterns = []
            for i in range(N - m + 1):
                patterns.append(data[i:i + m])
            
            C = []
            for i in range(N - m + 1):
                template_i = patterns[i]
                matches = 0
                for j in range(N - m + 1):
                    if _maxdist(template_i, patterns[j], N, m) <= r:
                        matches += 1
                C.append(matches / (N - m + 1))
            
            phi = np.mean([np.log(c) for c in C if c > 0])
            return phi
        
        try:
            return _phi(m) - _phi(m + 1)
        except:
            return 0.0
    
    def calculate_intensity_score(self, disp_range, speed_mean, accel_mean, complexity, continuity):
        """计算综合强度评分（0-10分）"""
        # 归一化各个指标到0-1范围
        disp_score = min(1, disp_range / 100)  # 100像素为满分
        speed_score = min(1, speed_mean / 500)  # 500像素/秒为满分
        accel_score = min(1, accel_mean / 2000)  # 2000像素/秒²为满分
        complexity_score = min(1, complexity / 1.0)  # 1.0为满分
        continuity_score = continuity  # 已经是0-1范围
        
        # 加权平均
        weights = [0.25, 0.25, 0.2, 0.15, 0.15]  # 位移、速度、加速度、复杂性、连续性
        scores = [disp_score, speed_score, accel_score, complexity_score, continuity_score]
        
        total_score = sum(w * s for w, s in zip(weights, scores)) * 10
        return min(10, max(0, total_score))
    
    def classify_intensity_level(self, score):
        """根据评分分类强度等级"""
        if score >= 8:
            return "极重度眼震"
        elif score >= 6:
            return "重度眼震"
        elif score >= 4:
            return "中度眼震"
        elif score >= 2:
            return "轻度眼震"
        else:
            return "微弱眼震"
    
    def analyze_nystagmus_waveform(self):
        """分析眼震波形特征"""
        print(f"\n🌊 眼震波形特征分析:")
        
        smooth_x = self.data['平滑_X'].values
        vx = self.data['速度_X'].values
        
        # 波形对称性分析
        symmetry = self.calculate_waveform_symmetry(smooth_x)
        
        # 波形平滑度分析
        smoothness = self.calculate_waveform_smoothness(smooth_x)
        
        # 峰值分析
        peaks_positive, _ = find_peaks(smooth_x, distance=self.fps//10)  # 最少间隔0.1秒
        peaks_negative, _ = find_peaks(-smooth_x, distance=self.fps//10)
        
        peak_amplitude_pos = np.mean(smooth_x[peaks_positive]) if len(peaks_positive) > 0 else 0
        peak_amplitude_neg = np.mean(-smooth_x[peaks_negative]) if len(peaks_negative) > 0 else 0
        
        # 波形周期性分析
        periodicity = self.calculate_waveform_periodicity(smooth_x)
        
        print(f"   • 波形对称性: {symmetry:.3f} (1为完全对称)")
        print(f"   • 波形平滑度: {smoothness:.3f}")
        print(f"   • 正向峰值: {len(peaks_positive)}个 (平均幅度: {peak_amplitude_pos:.1f}px)")
        print(f"   • 负向峰值: {len(peaks_negative)}个 (平均幅度: {peak_amplitude_neg:.1f}px)")
        print(f"   • 波形周期性: {periodicity:.3f}")
        
        # 波形分类
        waveform_type = self.classify_waveform_type(symmetry, smoothness, periodicity)
        print(f"   • 波形类型: {waveform_type}")
        
        # 存储结果
        self.trajectory_analysis.update({
            '波形对称性': symmetry,
            '波形平滑度': smoothness,
            '正向峰值数': len(peaks_positive),
            '负向峰值数': len(peaks_negative),
            '波形周期性': periodicity,
            '波形类型': waveform_type
        })
    
    def calculate_waveform_symmetry(self, signal):
        """计算波形对称性"""
        # 计算信号的偏度，越接近0越对称
        from scipy.stats import skew
        skew_value = abs(skew(signal))
        symmetry = 1 / (1 + skew_value)  # 转换为0-1范围，1为完全对称
        return symmetry
    
    def calculate_waveform_smoothness(self, signal):
        """计算波形平滑度"""
        # 使用二阶差分评估平滑度
        if len(signal) < 3:
            return 0
        
        second_diff = np.diff(signal, n=2)
        roughness = np.var(second_diff)
        smoothness = 1 / (1 + roughness)  # 转换为平滑度指标
        return smoothness
    
    def calculate_waveform_periodicity(self, signal):
        """计算波形周期性"""
        # 使用自相关函数评估周期性
        if len(signal) < 10:
            return 0
        
        # 计算归一化自相关
        correlation = np.correlate(signal, signal, mode='full')
        correlation = correlation[correlation.size // 2:]
        correlation = correlation / correlation[0]  # 归一化
        
        # 寻找第一个显著的正相关峰值（排除lag=0）
        if len(correlation) > self.fps:  # 至少1秒的数据
            peaks, _ = find_peaks(correlation[1:min(len(correlation), self.fps*3)], 
                                height=0.3, distance=self.fps//10)
            
            if len(peaks) > 0:
                # 返回最强的周期性相关值
                return correlation[peaks[0] + 1]
            else:
                return 0
        else:
            return 0
    
    def classify_waveform_type(self, symmetry, smoothness, periodicity):
        """分类波形类型"""
        if periodicity > 0.6 and smoothness > 0.7:
            if symmetry > 0.8:
                return "规律正弦波型"
            else:
                return "规律非对称波型"
        elif periodicity > 0.4:
            if smoothness > 0.5:
                return "半规律平滑波型"
            else:
                return "半规律锯齿波型"
        elif smoothness > 0.7:
            return "不规则平滑波型"
        else:
            return "不规则噪声波型"
    
    def analyze_nystagmus_directionality(self):
        """分析眼震方向性特征"""
        print(f"\n🧭 眼震方向性分析:")
        
        vx = self.data['速度_X'].values
        actual_x = self.data['actualX'].values
        
        # 方向偏好分析
        positive_velocity_time = np.sum(vx > 0) / len(vx)
        negative_velocity_time = np.sum(vx < 0) / len(vx)
        stationary_time = np.sum(np.abs(vx) < 10) / len(vx)  # 速度<10px/s视为静止
        
        # 方向持续性分析
        direction_persistence = self.calculate_direction_persistence(vx)
        
        # 运动范围分析
        center_position = np.mean(actual_x)
        rightward_extent = np.max(actual_x) - center_position
        leftward_extent = center_position - np.min(actual_x)
        
        # 方向性强度
        directional_bias = abs(positive_velocity_time - negative_velocity_time)
        
        print(f"   • 方向时间分布:")
        print(f"     - 右向运动: {positive_velocity_time*100:.1f}%")
        print(f"     - 左向运动: {negative_velocity_time*100:.1f}%")
        print(f"     - 相对静止: {stationary_time*100:.1f}%")
        
        print(f"   • 运动范围:")
        print(f"     - 右向最大位移: {rightward_extent:.1f} 像素")
        print(f"     - 左向最大位移: {leftward_extent:.1f} 像素")
        print(f"     - 中心位置: {center_position:.1f} 像素")
        
        print(f"   • 方向特性:")
        print(f"     - 方向持续性: {direction_persistence:.3f}")
        print(f"     - 方向偏向性: {directional_bias:.3f}")
        
        # 方向模式分类
        direction_pattern = self.classify_direction_pattern(
            positive_velocity_time, negative_velocity_time, directional_bias, direction_persistence
        )
        print(f"   • 方向模式: {direction_pattern}")
        
        # 存储结果
        self.trajectory_analysis.update({
            '右向运动比例': positive_velocity_time,
            '左向运动比例': negative_velocity_time,
            '静止比例': stationary_time,
            '右向最大位移': rightward_extent,
            '左向最大位移': leftward_extent,
            '方向持续性': direction_persistence,
            '方向偏向性': directional_bias,
            '方向模式': direction_pattern
        })
    
    def calculate_direction_persistence(self, velocity):
        """计算方向持续性"""
        # 计算连续同方向运动的平均长度
        direction_changes = np.diff(np.sign(velocity))
        change_points = np.where(direction_changes != 0)[0]
        
        if len(change_points) < 2:
            return 1.0  # 如果没有方向变化，持续性为1
        
        # 计算各段的长度
        segment_lengths = np.diff(np.concatenate([[0], change_points, [len(velocity)-1]]))
        
        # 方向持续性 = 平均段长度 / 总长度
        persistence = np.mean(segment_lengths) / len(velocity)
        return min(1.0, persistence)
    
    def classify_direction_pattern(self, pos_ratio, neg_ratio, bias, persistence):
        """分类方向模式"""
        if bias > 0.3:  # 明显的方向偏好
            if pos_ratio > neg_ratio:
                return "右向偏好型"
            else:
                return "左向偏好型"
        elif persistence > 0.7:
            return "双向对称持续型"
        elif persistence > 0.3:
            return "双向对称间歇型"
        else:
            return "快速变向型"

    def print_comprehensive_trajectory_report(self):
        """打印全面的轨迹分析报告"""
        print("\n" + "="*100)
        print("🎯 X轴眼震轨迹全面分析报告")
        print("="*100)
        
        if not self.trajectory_analysis:
            print("❌ 轨迹分析数据不可用")
            return
        
        # 眼震基本分类
        print(f"\n📊 眼震基本特征:")
        print(f"   • 类型分类: {', '.join(self.trajectory_analysis.get('眼震类型', ['未知']))}")
        print(f"   • 运动模式: {self.trajectory_analysis.get('眼震模式', '未知')}")
        print(f"   • 模式描述: {self.trajectory_analysis.get('模式描述', '无描述')}")
        print(f"   • 强度等级: {self.trajectory_analysis.get('强度等级', '未知')} (评分: {self.trajectory_analysis.get('强度评分', 0):.1f}/10)")
        
        # 频率特征
        print(f"\n📡 频率特征:")
        if self.trajectory_analysis.get('主频率', 0) > 0:
            print(f"   • 主频率: {self.trajectory_analysis['主频率']:.3f} Hz")
            print(f"   • 频率分类: {self.trajectory_analysis.get('频率分类', '未分类')}")
            print(f"   • 频率稳定性: {self.trajectory_analysis.get('频率稳定性', 0):.3f}")
            print(f"   • 主导频带: {self.trajectory_analysis.get('主导频带', '未知')}")
            print(f"   • 谐波数量: {self.trajectory_analysis.get('谐波数量', 0)}个")
        else:
            print(f"   • 主频率: 无明显频率特征")
        
        # 运动强度
        print(f"\n💪 运动强度:")
        print(f"   • 位移范围: {self.trajectory_analysis.get('位移范围', 0):.1f} 像素")
        print(f"   • 位移RMS: {self.trajectory_analysis.get('位移RMS', 0):.1f} 像素")
        print(f"   • 平均速度: {self.trajectory_analysis.get('平均速度', 0):.1f} 像素/秒")
        print(f"   • 最大速度: {self.trajectory_analysis.get('最大速度', 0):.1f} 像素/秒")
        print(f"   • 平均加速度: {self.trajectory_analysis.get('平均加速度', 0):.1f} 像素/秒²")
        print(f"   • 信号复杂性: {self.trajectory_analysis.get('信号复杂性', 0):.3f}")
        print(f"   • 运动连续性: {self.trajectory_analysis.get('运动连续性', 0)*100:.1f}%")
        
        # 波形特征
        print(f"\n🌊 波形特征:")
        print(f"   • 波形类型: {self.trajectory_analysis.get('波形类型', '未知')}")
        print(f"   • 波形对称性: {self.trajectory_analysis.get('波形对称性', 0):.3f}")
        print(f"   • 波形平滑度: {self.trajectory_analysis.get('波形平滑度', 0):.3f}")
        print(f"   • 波形周期性: {self.trajectory_analysis.get('波形周期性', 0):.3f}")
        print(f"   • 正向峰值: {self.trajectory_analysis.get('正向峰值数', 0)}个")
        print(f"   • 负向峰值: {self.trajectory_analysis.get('负向峰值数', 0)}个")
        
        # 方向性特征
        print(f"\n🧭 方向性特征:")
        print(f"   • 方向模式: {self.trajectory_analysis.get('方向模式', '未知')}")
        print(f"   • 右向运动: {self.trajectory_analysis.get('右向运动比例', 0)*100:.1f}%")
        print(f"   • 左向运动: {self.trajectory_analysis.get('左向运动比例', 0)*100:.1f}%")
        print(f"   • 相对静止: {self.trajectory_analysis.get('静止比例', 0)*100:.1f}%")
        print(f"   • 方向持续性: {self.trajectory_analysis.get('方向持续性', 0):.3f}")
        print(f"   • 方向偏向性: {self.trajectory_analysis.get('方向偏向性', 0):.3f}")
        print(f"   • 右向最大位移: {self.trajectory_analysis.get('右向最大位移', 0):.1f} 像素")
        print(f"   • 左向最大位移: {self.trajectory_analysis.get('左向最大位移', 0):.1f} 像素")
        
        # 临床意义评估
        print(f"\n🏥 临床意义评估:")
        self.assess_clinical_significance()
        
    def assess_clinical_significance(self):
        """评估眼震的临床意义"""
        # 基于分析结果评估临床意义
        intensity_score = self.trajectory_analysis.get('强度评分', 0)
        main_freq = self.trajectory_analysis.get('主频率', 0)
        pattern = self.trajectory_analysis.get('眼震模式', '')
        symmetry = self.trajectory_analysis.get('波形对称性', 0)
        
        clinical_notes = []
        
        # 强度评估
        if intensity_score >= 7:
            clinical_notes.append("⚠️ 高强度眼震，可能影响视觉功能")
        elif intensity_score >= 4:
            clinical_notes.append("⚡ 中等强度眼震，需要关注")
        else:
            clinical_notes.append("✅ 低强度眼震，相对轻微")
        
        # 频率评估
        if 1 <= main_freq <= 5:
            clinical_notes.append("📡 频率在正常眼震范围内")
        elif main_freq > 8:
            clinical_notes.append("⚠️ 高频眼震，可能提示中枢性病变")
        elif main_freq > 0 and main_freq < 1:
            clinical_notes.append("🔄 低频眼震，可能提示前庭功能异常")
        
        # 模式评估
        if "冲动性" in pattern:
            clinical_notes.append("🎯 冲动性眼震模式，提示前庭系统异常")
        elif "摆动性" in pattern:
            clinical_notes.append("⚖️ 摆动性眼震模式，可能为先天性或获得性")
        
        # 对称性评估
        if symmetry < 0.5:
            clinical_notes.append("⚠️ 波形不对称，需要进一步评估")
        
        for note in clinical_notes:
            print(f"   {note}")
        
        if not clinical_notes:
            print("   📊 眼震特征在可接受范围内")

    # ======================
    # 原有的UKF预测分析功能保持不变
    # ======================
    
    def calculate_errors(self):
        """计算X轴误差指标（使用已有的误差数据）"""
        if self.data is None:
            print("❌ 没有数据可分析")
            return
        
        # 计算误差（如果没有的话）
        if 'predictionErrorX' not in self.data.columns:
            self.data['predictionErrorX'] = self.data['actualX'] - self.data['predictedX']
        
        # 使用新的列名
        self.data['误差_X'] = self.data['predictionErrorX']
        self.data['绝对误差_X'] = np.abs(self.data['误差_X'])
        
        # 如果有errorMagnitude，直接使用
        if 'errorMagnitude' in self.data.columns:
            self.data['总误差'] = self.data['errorMagnitude']
        
        # 计算相对误差（考虑到屏幕宽度）
        screen_width = 1920  # 假设1920像素宽度
        self.data['相对误差_X'] = (self.data['绝对误差_X'] / screen_width) * 100
        
        # 计算误差的移动平均（用于趋势分析）
        window_size = min(10, len(self.data) // 20)
        if window_size > 1:
            self.data['误差_移动平均'] = self.data['误差_X'].rolling(window=window_size, center=True).mean()
            self.data['绝对误差_移动平均'] = self.data['绝对误差_X'].rolling(window=window_size, center=True).mean()
        
        print("✅ X轴误差计算完成")
    
    def calculate_accuracy_metrics(self):
        """计算X轴准确性指标（修正NRMSE计算）"""
        if self.data is None:
            return
        
        metrics = {}
        
        # 基础误差指标
        metrics['平均绝对误差'] = self.data['绝对误差_X'].mean()
        metrics['中位数绝对误差'] = self.data['绝对误差_X'].median()
        metrics['最大绝对误差'] = self.data['绝对误差_X'].max()
        metrics['最小绝对误差'] = self.data['绝对误差_X'].min()
        metrics['误差标准差'] = self.data['绝对误差_X'].std()
        
        # 如果有总误差，也计算总误差的统计
        if 'errorMagnitude' in self.data.columns:
            metrics['平均总误差'] = self.data['errorMagnitude'].mean()
            metrics['最大总误差'] = self.data['errorMagnitude'].max()
        
        # 系统性偏差
        metrics['X轴系统偏差'] = self.data['误差_X'].mean()
        
        # 相关性和决定系数
        metrics['X轴相关性'] = self.data['actualX'].corr(self.data['predictedX'])
        metrics['X轴R²'] = r2_score(self.data['actualX'], self.data['predictedX'])
        
        # MSE和RMSE
        metrics['X轴MSE'] = mean_squared_error(self.data['actualX'], self.data['predictedX'])
        metrics['X轴RMSE'] = np.sqrt(metrics['X轴MSE'])
        metrics['X轴MAE'] = mean_absolute_error(self.data['actualX'], self.data['predictedX'])
        
        # 修正的NRMSE计算
        Yref = self.data['actualX'].values  # 真实值
        Y = self.data['predictedX'].values   # 预测值
        Yref_mean = np.mean(Yref)           # 真实值的均值
        
        # 计算分子：||Yref - Y||（预测误差的范数）
        numerator = np.linalg.norm(Yref - Y)
        
        # 计算分母：||Yref - mean(Yref)||（真实值与其均值的范数）
        denominator = np.linalg.norm(Yref - Yref_mean)
        
        # 计算修正的NRMSE
        if denominator > 0:
            metrics['X轴NRMSE_修正'] = 1 - (numerator / denominator)
        else:
            metrics['X轴NRMSE_修正'] = 0
        
        # 为了对比，保留原来的计算方法
        data_range = self.data['actualX'].max() - self.data['actualX'].min()
        if data_range > 0:
            metrics['X轴NRMSE_原始'] = metrics['X轴RMSE'] / data_range
        else:
            metrics['X轴NRMSE_原始'] = 0
        
        # 精度等级分布
        thresholds = [1, 2, 3, 5, 10, 15, 20, 30, 50, 100]
        for t in thresholds:
            metrics[f'{t}像素内'] = (self.data['绝对误差_X'] <= t).sum() / len(self.data) * 100
        
        # 计算视角误差（假设观看距离60cm，屏幕PPI=96）
        viewing_distance_cm = 60
        pixels_per_cm = 96 / 2.54  # 96 DPI转换
        visual_angle = np.degrees(2 * np.arctan(
            (self.data['绝对误差_X'] / pixels_per_cm) / (2 * viewing_distance_cm)
        ))
        metrics['平均视角误差'] = visual_angle.mean()
        metrics['最小视角误差'] = visual_angle.min()
        metrics['最大视角误差'] = visual_angle.max()
        
        # 计算UKF预测稳定性指标
        if len(self.data) > 1:
            pred_change = np.diff(self.data['predictedX'])
            metrics['UKF预测平滑度'] = np.abs(pred_change).mean()
            metrics['UKF预测抖动度'] = np.abs(pred_change).std()
        
        # 计算置信区间
        confidence_95 = 1.96 * metrics['误差标准差'] / np.sqrt(len(self.data))
        metrics['95%置信区间'] = f"±{confidence_95:.2f}"
        
        self.metrics = metrics
        return metrics
    
    def analyze_nystagmus_reduction(self):
        """以第一帧为中心分析眼震减缓效果（修正版）"""
        if len(self.data) < 2:
            print("数据不足，无法进行减缓分析")
            return
            
        print("\n" + "="*80)
        print("🎯 眼震减缓效果分析（以第一帧为参考中心，修正版）")
        print("="*80)
        
        # 1. 设置第一帧为参考中心点
        first_frame = self.data.iloc[0]
        self.reference_point = {
            'frameId': first_frame['frameId'],
            'actualX': first_frame['actualX'],
            'predictedX': first_frame['predictedX']
        }
        
        print(f"\n📍 参考中心点（第一帧）:")
        print(f"   帧ID: {self.reference_point['frameId']}")
        print(f"   真实X坐标: {self.reference_point['actualX']:.2f}")
        print(f"   预测X坐标: {self.reference_point['predictedX']:.2f}")
        
        # 2. 计算每帧相对于第一帧的位移
        self.data['真实位移_X'] = self.data['actualX'] - self.reference_point['actualX']
        self.data['预测位移_X'] = self.data['predictedX'] - self.reference_point['actualX']  # 都以真实值作为参考
        self.data['真实位移_绝对值'] = np.abs(self.data['真实位移_X'])
        self.data['预测位移_绝对值'] = np.abs(self.data['预测位移_X'])
        
        # 3. 修正的减缓区域计算
        # 公共区域（减缓区域）= 真实位移和预测位移的重叠部分
        self.data['公共区域_减缓'] = np.minimum(self.data['真实位移_绝对值'], self.data['预测位移_绝对值'])
        
        # 残余眼震 = 预测无法覆盖的真实眼震部分
        self.data['残余眼震'] = np.maximum(0, self.data['真实位移_绝对值'] - self.data['预测位移_绝对值'])
        
        # 异常增强 = 预测超出真实眼震的部分（这是不好的）
        self.data['异常增强'] = np.maximum(0, self.data['预测位移_绝对值'] - self.data['真实位移_绝对值'])
        
        # 总减缓量 = 原本眼震 - 残余眼震
        self.data['总减缓量'] = self.data['真实位移_绝对值'] - self.data['残余眼震']
        
        # 减缓效率 = 减缓量 / 原本眼震
        self.data['减缓效率'] = np.where(
            self.data['真实位移_绝对值'] > 0,
            (self.data['总减缓量'] / self.data['真实位移_绝对值']) * 100,
            0
        )
        
        # 4. 计算统计指标
        self.reduction_analysis = {}
        self.reduction_analysis['平均真实位移'] = self.data['真实位移_绝对值'].mean()
        self.reduction_analysis['平均预测位移'] = self.data['预测位移_绝对值'].mean()
        self.reduction_analysis['平均公共区域'] = self.data['公共区域_减缓'].mean()
        self.reduction_analysis['平均残余眼震'] = self.data['残余眼震'].mean()
        self.reduction_analysis['平均异常增强'] = self.data['异常增强'].mean()
        self.reduction_analysis['平均减缓量'] = self.data['总减缓量'].mean()
        self.reduction_analysis['平均减缓效率'] = self.data['减缓效率'].mean()
        
        # 最大值统计
        self.reduction_analysis['最大真实位移'] = self.data['真实位移_绝对值'].max()
        self.reduction_analysis['最大预测位移'] = self.data['预测位移_绝对值'].max()
        self.reduction_analysis['最大残余眼震'] = self.data['残余眼震'].max()
        self.reduction_analysis['最大异常增强'] = self.data['异常增强'].max()
        
        # 改善和恶化统计
        improved_frames = (self.data['残余眼震'] < self.data['真实位移_绝对值']).sum()
        worsened_frames = (self.data['异常增强'] > 0).sum()
        self.reduction_analysis['改善帧数'] = improved_frames
        self.reduction_analysis['恶化帧数'] = worsened_frames
        self.reduction_analysis['改善率'] = (improved_frames / len(self.data)) * 100
        self.reduction_analysis['恶化率'] = (worsened_frames / len(self.data)) * 100
        
        # 打印报告
        self._print_corrected_reduction_report()
        
    def _print_corrected_reduction_report(self):
        """打印修正的眼震减缓效果报告"""
        print(f"\n📊 修正的减缓效果分析:")
        print(f"   • 平均真实位移: {self.reduction_analysis['平均真实位移']:.2f} 像素")
        print(f"   • 平均预测位移: {self.reduction_analysis['平均预测位移']:.2f} 像素")
        print(f"   • 平均公共区域（有效减缓）: {self.reduction_analysis['平均公共区域']:.2f} 像素")
        print(f"   • 平均残余眼震: {self.reduction_analysis['平均残余眼震']:.2f} 像素")
        print(f"   • 平均异常增强: {self.reduction_analysis['平均异常增强']:.2f} 像素")
        print(f"   • 平均减缓效率: {self.reduction_analysis['平均减缓效率']:.1f}%")
        
        print(f"\n📈 最大值统计:")
        print(f"   • 最大真实位移: {self.reduction_analysis['最大真实位移']:.2f} 像素")
        print(f"   • 最大预测位移: {self.reduction_analysis['最大预测位移']:.2f} 像素")
        print(f"   • 最大残余眼震: {self.reduction_analysis['最大残余眼震']:.2f} 像素")
        print(f"   • 最大异常增强: {self.reduction_analysis['最大异常增强']:.2f} 像素")
        
        print(f"\n✅ 改善与恶化统计:")
        print(f"   • 改善帧数: {self.reduction_analysis['改善帧数']}/{len(self.data)}")
        print(f"   • 改善率: {self.reduction_analysis['改善率']:.1f}%")
        print(f"   • 恶化帧数: {self.reduction_analysis['恶化帧数']}/{len(self.data)}")
        print(f"   • 恶化率: {self.reduction_analysis['恶化率']:.1f}%")

    def print_analysis_report(self):
        """打印X轴UKF预测分析报告（调整NRMSE评级标准）"""
        if not hasattr(self, 'metrics'):
            self.calculate_accuracy_metrics()
        
        print("\n" + "="*70)
        print("🎯 X轴眼震UKF注视预测准确性分析报告（调整NRMSE评级）")
        print("="*70)
        
        # 总体性能评级（基于修正的NRMSE和其他综合指标）
        nrmse_corrected = self.metrics['X轴NRMSE_修正']
        avg_error = self.metrics['平均绝对误差']
        correlation = self.metrics['X轴相关性']
        r2 = self.metrics['X轴R²']
        precision_10px = self.metrics['10像素内']
        
        # 调整后的NRMSE评级标准（考虑眼动预测的实际困难度）
        if nrmse_corrected > 0.85:
            nrmse_grade = "卓越"
        elif nrmse_corrected > 0.75:
            nrmse_grade = "优秀"
        elif nrmse_corrected > 0.60:
            nrmse_grade = "良好"
        elif nrmse_corrected > 0.45:
            nrmse_grade = "可接受"
        else:
            nrmse_grade = "需改进"
        
        # 综合评级（考虑多个指标）
        score = 0
        if nrmse_corrected > 0.60: score += 1
        if correlation > 0.85: score += 1
        if r2 > 0.70: score += 1
        if precision_10px > 50: score += 1
        if avg_error < 20: score += 1
        
        if score >= 4:
            overall_grade = "卓越 ⭐⭐⭐⭐⭐+"
        elif score >= 3:
            overall_grade = "优秀 ⭐⭐⭐⭐⭐"
        elif score >= 2:
            overall_grade = "良好 ⭐⭐⭐⭐"
        else:
            overall_grade = "需改进 ⭐⭐⭐"
        
        print(f"\n🏆 X轴UKF预测综合评级: {overall_grade}")
        print(f"   修正NRMSE: {nrmse_corrected:.4f} ({nrmse_grade})")
        print(f"   平均误差: {avg_error:.1f} 像素 (约 {avg_error/37.8:.1f} mm)")
        print(f"   📊 综合评分: {score}/5 (NRMSE+相关性+R²+精度+误差)")
        
        print(f"\n📏 X轴误差统计:")
        print(f"   • 平均绝对误差: {self.metrics['平均绝对误差']:.1f} 像素")
        print(f"   • 中位数误差: {self.metrics['中位数绝对误差']:.1f} 像素")
        print(f"   • 最小误差: {self.metrics['最小绝对误差']:.1f} 像素")
        print(f"   • 最大误差: {self.metrics['最大绝对误差']:.1f} 像素")
        print(f"   • 误差波动: ±{self.metrics['误差标准差']:.1f} 像素")
        print(f"   • 95%置信区间: {self.metrics['95%置信区间']} 像素")
        print(f"   • RMSE: {self.metrics['X轴RMSE']:.1f} 像素")
        print(f"   • MAE: {self.metrics['X轴MAE']:.1f} 像素")
        
        print(f"\n📊 NRMSE对比分析:")
        print(f"   • 修正NRMSE: {self.metrics['X轴NRMSE_修正']:.4f} (公式: 1 - ||Yref-Y|| / ||Yref-mean(Yref)||)")
        print(f"   • 传统NRMSE(基于范围): {self.metrics['X轴NRMSE_原始']:.4f} (RMSE/数据范围)")
        
        # 调整后的NRMSE评级说明
        print(f"\n🎯 调整后的NRMSE评级标准（针对眼动预测）:")
        print(f"   • >0.85: 卓越 | 0.75-0.85: 优秀 | 0.60-0.75: 良好")
        print(f"   • 0.45-0.60: 可接受 | <0.45: 需改进")
        print(f"   • 您的结果: {nrmse_corrected:.4f} → {nrmse_grade}")
        
        if '平均总误差' in self.metrics:
            print(f"\n📐 总误差统计（含Y轴）:")
            print(f"   • 平均总误差: {self.metrics['平均总误差']:.1f} 像素")
            print(f"   • 最大总误差: {self.metrics['最大总误差']:.1f} 像素")
        
        print(f"\n👁️ 视角精度:")
        print(f"   • 平均视角误差: {self.metrics['平均视角误差']:.2f}°")
        print(f"   • 最小视角误差: {self.metrics['最小视角误差']:.2f}°")
        print(f"   • 最大视角误差: {self.metrics['最大视角误差']:.2f}°")
        
        print(f"\n🎯 X轴精度分布:")
        print("   ┌─────────────┬──────────┬────────────────────┐")
        print("   │ 误差范围    │ 百分比   │ 可视化             │")
        print("   ├─────────────┼──────────┼────────────────────┤")
        
        thresholds = [1, 2, 3, 5, 10, 15, 20, 30, 50]
        for t in thresholds:
            percent = self.metrics[f'{t}像素内']
            bar = '█' * int(percent / 2.5)
            status = ""
            if t <= 5 and percent > 25:
                status = " 🏆"
            elif t <= 10 and percent > 50:
                status = " ⭐"
            elif t <= 20 and percent > 80:
                status = " ✅"
            print(f"   │ ≤{t:2d} 像素   │ {percent:5.1f}%   │ {bar:<20} │{status}")
        
        print("   └─────────────┴──────────┴────────────────────┘")
        
        print(f"\n📊 X轴UKF预测质量:")
        print(f"   • 相关性: {self.metrics['X轴相关性']:.3f} {'🏆' if self.metrics['X轴相关性'] > 0.9 else '⭐' if self.metrics['X轴相关性'] > 0.8 else ''}")
        print(f"   • R²: {self.metrics['X轴R²']:.3f} {'🏆' if self.metrics['X轴R²'] > 0.8 else '⭐' if self.metrics['X轴R²'] > 0.7 else ''}")
        
        print(f"\n⚖️ X轴系统性偏差:")
        bias_x = self.metrics['X轴系统偏差']
        print(f"   • 偏差: {bias_x:+.1f} 像素 {'(偏右)' if bias_x > 0 else '(偏左)'}")
        
        if 'UKF预测平滑度' in self.metrics:
            print(f"\n🌊 X轴UKF稳定性指标:")
            print(f"   • 预测平滑度: {self.metrics['UKF预测平滑度']:.1f} 像素/帧")
            print(f"   • 预测抖动度: {self.metrics['UKF预测抖动度']:.1f} 像素")
        
        # 新增：性能亮点总结
        print(f"\n✨ 性能亮点总结:")
        highlights = []
        if self.metrics['X轴相关性'] > 0.9:
            highlights.append(f"🎯 强相关性 ({self.metrics['X轴相关性']:.3f})")
        if self.metrics['X轴R²'] > 0.8:
            highlights.append(f"📈 高解释度 (R²={self.metrics['X轴R²']:.3f})")
        if self.metrics['10像素内'] > 50:
            highlights.append(f"🎪 高精度 ({self.metrics['10像素内']:.1f}%在±10px内)")
        if self.metrics['平均绝对误差'] < 20:
            highlights.append(f"📍 低误差 (平均{self.metrics['平均绝对误差']:.1f}px)")
        
        for highlight in highlights:
            print(f"   • {highlight}")
        
        if len(highlights) >= 3:
            print(f"   🏆 总体表现：您的UKF系统表现优秀！")
        elif len(highlights) >= 2:
            print(f"   ⭐ 总体表现：您的UKF系统表现良好！")
        else:
            print(f"   📊 总体表现：您的UKF系统有改进空间。")

    # ==========================================
    # 可视化函数（保持原有功能并增强）
    # ==========================================
    
    def create_page1_tracking_and_error(self):
        """第1页：X轴跟踪效果和误差分析 (2个图表)"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 增强版X轴眼震UKF预测分析 - 第1页：跟踪效果与误差分析', fontsize=24, y=0.95)
        
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. X轴预测跟踪效果
        ax1 = fig.add_subplot(gs[0, 0])
        
        ax1.plot(self.data['frameId'], self.data['predictedX'], 'r-', linewidth=3, 
                label='UKF预测', alpha=0.8, zorder=1)
        ax1.plot(self.data['frameId'], self.data['actualX'], 'b--', linewidth=2.5, 
                label='真实值', alpha=0.9, zorder=2)
        
        # 如果有平滑数据，也显示平滑轨迹
        if '平滑_X' in self.data.columns:
            ax1.plot(self.data['frameId'], self.data['平滑_X'], 'g:', linewidth=2, 
                    label='平滑真实值', alpha=0.7, zorder=1)
        
        ax1.fill_between(self.data['frameId'], self.data['actualX'], self.data['predictedX'], 
                        alpha=0.2, color='gray', label='误差区域', zorder=0)
        
        ax1.set_xlabel('帧ID', fontsize=18)
        ax1.set_ylabel('X坐标 (像素)', fontsize=18)
        ax1.set_title('X轴UKF预测跟踪效果（含轨迹分析）', fontsize=20, pad=20)
        ax1.legend(fontsize=16, loc='upper right')
        ax1.grid(True, alpha=0.3)
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加轨迹特征信息
        if self.trajectory_analysis:
            trajectory_info = f'眼震类型: {", ".join(self.trajectory_analysis.get("眼震类型", ["未知"]))}\n'
            trajectory_info += f'主频率: {self.trajectory_analysis.get("主频率", 0):.2f} Hz\n'
            trajectory_info += f'强度等级: {self.trajectory_analysis.get("强度等级", "未知")}'
            ax1.text(0.02, 0.95, trajectory_info, 
                    transform=ax1.transAxes, fontsize=12, 
                    verticalalignment='top',
                    bbox=dict(boxstyle='round,pad=0.5', facecolor='lightcyan', alpha=0.8))
        
        # 2. X轴误差时序分析
        ax2 = fig.add_subplot(gs[0, 1])
        
        ax2.plot(self.data['frameId'], self.data['误差_X'], alpha=0.7, color='red', 
                linewidth=2, label='X轴误差')
        
        ax2.axhline(y=0, color='black', linestyle='-', linewidth=2, alpha=0.8)
        ax2.axhline(y=10, color='green', linestyle='--', linewidth=2, alpha=0.8, label='±10px')
        ax2.axhline(y=-10, color='green', linestyle='--', linewidth=2, alpha=0.8)
        ax2.axhline(y=20, color='orange', linestyle='--', linewidth=2, alpha=0.8, label='±20px')
        ax2.axhline(y=-20, color='orange', linestyle='--', linewidth=2, alpha=0.8)
        
        if '误差_移动平均' in self.data.columns:
            ax2.plot(self.data['frameId'], self.data['误差_移动平均'], color='darkred', 
                    linewidth=4, label='移动平均', alpha=0.9)
        
        ax2.set_xlabel('帧ID', fontsize=18)
        ax2.set_ylabel('X轴误差 (像素)', fontsize=18)
        ax2.set_title('X轴预测误差变化趋势', fontsize=20, pad=20)
        ax2.legend(loc='upper right', fontsize=14)
        ax2.grid(True, alpha=0.3)
        ax2.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加误差统计信息
        error_stats_text = f'平均误差: {self.metrics["平均绝对误差"]:.1f}px\n中位数: {self.metrics["中位数绝对误差"]:.1f}px\n标准差: {self.metrics["误差标准差"]:.1f}px'
        ax2.text(0.02, 0.95, error_stats_text, 
                transform=ax2.transAxes, fontsize=14, 
                verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='lightgreen', alpha=0.8))
        
        plt.tight_layout()
        plt.show()

    def create_page2_precision_analysis(self):
        """第2页：X轴精度分析 (2个图表)"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 增强版X轴眼震UKF预测分析 - 第2页：精度分析', fontsize=24, y=0.95)
        
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. 误差分布直方图
        ax1 = fig.add_subplot(gs[0, 0])
        
        bins = np.concatenate([
            np.arange(0, 10, 1),      
            np.arange(10, 30, 2),     
            np.arange(30, 60, 5),     
            np.arange(60, 100, 10),   
            [120, 150, 200]           
        ])
        
        n, bins, patches = ax1.hist(self.data['绝对误差_X'], bins=bins, alpha=0.8, 
                                   edgecolor='darkblue', linewidth=2)
        
        # 根据误差大小着色
        for i, patch in enumerate(patches):
            if bins[i] < 3:
                patch.set_facecolor('#27ae60')  
                patch.set_alpha(0.9)
            elif bins[i] < 5:
                patch.set_facecolor('#2ecc71')  
                patch.set_alpha(0.8)
            elif bins[i] < 10:
                patch.set_facecolor('#3498db')  
                patch.set_alpha(0.8)
            elif bins[i] < 20:
                patch.set_facecolor('#f39c12')  
                patch.set_alpha(0.8)
            elif bins[i] < 50:
                patch.set_facecolor('#e67e22')  
                patch.set_alpha(0.8)
            else:
                patch.set_facecolor('#e74c3c')  
                patch.set_alpha(0.8)
        
        ax1.axvline(self.data['绝对误差_X'].mean(), color='red', linestyle='--', 
                   linewidth=3, label=f'平均: {self.data["绝对误差_X"].mean():.1f}px')
        ax1.axvline(self.data['绝对误差_X'].median(), color='green', linestyle='--', 
                   linewidth=3, label=f'中位数: {self.data["绝对误差_X"].median():.1f}px')
        
        ax1.set_xlabel('绝对误差 (像素)', fontsize=18)
        ax1.set_ylabel('频次', fontsize=18)
        ax1.set_title('X轴误差分布直方图', fontsize=20, pad=20)
        ax1.legend(fontsize=16)
        ax1.grid(True, alpha=0.3, axis='y')
        ax1.set_xlim(0, min(100, self.data['绝对误差_X'].max() * 1.1))
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加修正NRMSE和轨迹信息
        stats_text = f'RMSE: {self.metrics["X轴RMSE"]:.1f}px\nMAE: {self.metrics["X轴MAE"]:.1f}px\n修正NRMSE: {self.metrics["X轴NRMSE_修正"]:.4f}'
        if self.trajectory_analysis.get('强度等级'):
            stats_text += f'\n眼震强度: {self.trajectory_analysis["强度等级"]}'
        ax1.text(0.7, 0.85, stats_text, transform=ax1.transAxes, 
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8),
                fontsize=14, verticalalignment='top')
        
        # 2. 精度等级饼图
        ax2 = fig.add_subplot(gs[0, 1])
        
        thresholds = [5, 10, 15, 20, 50, np.inf]
        labels = ['<5px\n(极致)', '5-10px\n(优秀)', '10-15px\n(良好)', 
                 '15-20px\n(合格)', '20-50px\n(较差)', '>50px\n(差)']
        colors_pie = ['#27ae60', '#2ecc71', '#3498db', '#52c41a', '#f39c12', '#e74c3c']
        
        counts = []
        for i in range(len(thresholds)):
            if i == 0:
                count = (self.data['绝对误差_X'] < thresholds[i]).sum()
            else:
                count = ((self.data['绝对误差_X'] >= thresholds[i-1]) & 
                        (self.data['绝对误差_X'] < thresholds[i])).sum()
            counts.append(count)
        
        non_zero = [(c, l, col) for c, l, col in zip(counts, labels, colors_pie) if c > 0]
        if non_zero:
            counts_nz, labels_nz, colors_nz = zip(*non_zero)
            
            wedges, texts, autotexts = ax2.pie(counts_nz, labels=labels_nz, colors=colors_nz, 
                                               autopct=lambda pct: f'{pct:.1f}%\n({int(pct*len(self.data)/100)})', 
                                               startangle=90, pctdistance=0.85, labeldistance=1.1)
            
            for text in texts:
                text.set_fontsize(16)
                text.set_weight('bold')
            for autotext in autotexts:
                autotext.set_color('white')
                autotext.set_weight('bold')
                autotext.set_fontsize(15)
        
        ax2.set_title('X轴精度等级分布', fontsize=20, pad=20)
        
        # 添加NRMSE评价和轨迹信息
        nrmse_corrected = self.metrics['X轴NRMSE_修正']
        nrmse_eval = ""
        if nrmse_corrected > 0.85:
            nrmse_eval = "卓越"
        elif nrmse_corrected > 0.75:
            nrmse_eval = "优秀"
        elif nrmse_corrected > 0.60:
            nrmse_eval = "良好"
        else:
            nrmse_eval = "需改进"
        
        eval_text = f'NRMSE评价: {nrmse_eval}'
        if self.trajectory_analysis.get('眼震模式'):
            eval_text += f'\n眼震模式: {self.trajectory_analysis["眼震模式"]}'
        
        ax2.text(0.5, -0.15, eval_text, 
                transform=ax2.transAxes, fontsize=16, 
                ha='center', weight='bold',
                bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.8))
        
        plt.tight_layout()
        plt.show()
    
    def create_page3_enhanced_nystagmus_analysis(self):
        """第3页：增强版眼震特性分析 (2个图表)"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 增强版X轴眼震UKF预测分析 - 第3页：深度眼震轨迹特性分析', fontsize=24, y=0.95)
        
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. X轴眼动速度和加速度分析
        ax1 = fig.add_subplot(gs[0, 0])
        
        # 双Y轴：速度和加速度
        ax1_twin = ax1.twinx()
        
        # 绘制速度
        line1 = ax1.plot(self.data['frameId'], self.data['X轴眼动速度'], 'purple', 
                        linewidth=2, alpha=0.8, label='眼动速度')
        
        # 绘制加速度（缩放以便显示）
        accel_scaled = self.data['X轴加速度'] / 10  # 缩放10倍以便显示
        line2 = ax1_twin.plot(self.data['frameId'], accel_scaled, 'orange', 
                             linewidth=2, alpha=0.8, label='加速度(/10)')
        
        # 添加速度阈值线
        ax1.axhline(y=100, color='green', linestyle='--', linewidth=2, alpha=0.6, label='低速阈值')
        ax1.axhline(y=300, color='orange', linestyle='--', linewidth=2, alpha=0.6, label='中速阈值')
        ax1.axhline(y=500, color='red', linestyle='--', linewidth=2, alpha=0.6, label='高速阈值')
        
        # 标记高速眼动点
        high_speed_points = self.data[self.data['X轴眼动速度'] > 500]
        if len(high_speed_points) > 0:
            ax1.scatter(high_speed_points['frameId'], high_speed_points['X轴眼动速度'], 
                       color='red', s=60, alpha=0.8, label='高速眼动', zorder=5)
        
        ax1.set_xlabel('帧ID', fontsize=18)
        ax1.set_ylabel('速度 (像素/秒)', fontsize=18, color='purple')
        ax1_twin.set_ylabel('加速度 (×10 像素/秒²)', fontsize=18, color='orange')
        ax1.set_title('X轴眼动动力学分析', fontsize=20, pad=20)
        
        # 合并图例
        lines1, labels1 = ax1.get_legend_handles_labels()
        lines2, labels2 = ax1_twin.get_legend_handles_labels()
        ax1.legend(lines1 + lines2, labels1 + labels2, fontsize=12, loc='upper right')
        
        ax1.grid(True, alpha=0.3)
        ax1.tick_params(axis='both', which='major', labelsize=16)
        ax1_twin.tick_params(axis='y', which='major', labelsize=16)
        
        # 添加轨迹特征信息
        if self.trajectory_analysis:
            trajectory_info = f'眼震模式: {self.trajectory_analysis.get("眼震模式", "未知")}\n'
            trajectory_info += f'平均速度: {self.trajectory_analysis.get("平均速度", 0):.1f} px/s\n'
            trajectory_info += f'强度评分: {self.trajectory_analysis.get("强度评分", 0):.1f}/10\n'
            trajectory_info += f'运动连续性: {self.trajectory_analysis.get("运动连续性", 0)*100:.1f}%'
            ax1.text(0.02, 0.95, trajectory_info, 
                    transform=ax1.transAxes, fontsize=12, 
                    verticalalignment='top',
                    bbox=dict(boxstyle='round,pad=0.5', facecolor='lightblue', alpha=0.8))
        
        # 2. X轴频谱和波形特征分析
        ax2 = fig.add_subplot(gs[0, 1])
        
        # 执行FFT分析
        if '平滑_X' in self.data.columns:
            signal = self.data['平滑_X'].values - np.mean(self.data['平滑_X'].values)
        else:
            signal = self.data['actualX'].values - np.mean(self.data['actualX'].values)
            
        fft_result = fft(signal)
        freqs = fftfreq(len(signal), 1/self.fps)
        
        # 只显示正频率部分
        positive_freqs = freqs[freqs > 0]
        fft_magnitude = np.abs(fft_result[freqs > 0])
        
        # 绘制频谱（限制在眼震相关频率范围）
        freq_mask = positive_freqs <= 20
        display_freqs = positive_freqs[freq_mask]
        display_magnitude = fft_magnitude[freq_mask]
        
        ax2.plot(display_freqs, display_magnitude, 'b-', linewidth=2, alpha=0.8, label='频谱')
        
        # 标记主频率和谐波
        if self.trajectory_analysis.get('主频率', 0) > 0:
            main_freq = self.trajectory_analysis['主频率']
            ax2.axvline(x=main_freq, color='red', linestyle='--', linewidth=3, 
                       label=f'主频率: {main_freq:.2f} Hz')
            
            # 标记谐波
            for i in range(2, 5):
                harmonic_freq = main_freq * i
                if harmonic_freq <= 20:
                    ax2.axvline(x=harmonic_freq, color='orange', linestyle=':', 
                               linewidth=2, alpha=0.7, label=f'{i}次谐波' if i == 2 else '')
        
        # 标记频带
        freq_bands = [(0, 1, 'lightblue', '超低频'), (1, 3, 'lightgreen', '低频'), 
                     (3, 7, 'lightyellow', '中频'), (7, 15, 'lightcoral', '高频')]
        
        for low, high, color, name in freq_bands:
            ax2.axvspan(low, high, alpha=0.2, color=color, label=name if low == 0 else '')
        
        ax2.set_xlabel('频率 (Hz)', fontsize=18)
        ax2.set_ylabel('幅度', fontsize=18)
        ax2.set_title('X轴眼震频谱分析（含频带划分）', fontsize=20, pad=20)
        ax2.set_xlim(0, 20)
        ax2.legend(fontsize=12, loc='upper right')
        ax2.grid(True, alpha=0.3)
        ax2.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加频率和波形特征信息
        if self.trajectory_analysis:
            freq_info = f'频率分类: {self.trajectory_analysis.get("频率分类", "未知")}\n'
            freq_info += f'频率稳定性: {self.trajectory_analysis.get("频率稳定性", 0):.3f}\n'
            freq_info += f'波形类型: {self.trajectory_analysis.get("波形类型", "未知")}\n'
            freq_info += f'波形对称性: {self.trajectory_analysis.get("波形对称性", 0):.3f}\n'
            freq_info += f'波形周期性: {self.trajectory_analysis.get("波形周期性", 0):.3f}'
            
            ax2.text(0.02, 0.98, freq_info, 
                    transform=ax2.transAxes, fontsize=11, 
                    verticalalignment='top',
                    bbox=dict(boxstyle='round,pad=0.5', facecolor='lightyellow', alpha=0.8))
        
        plt.tight_layout()
        plt.show()
    
    def create_page4_scatter_analysis(self):
        """第4页：X轴散点分析 (2个图表)"""
        fig = plt.figure(figsize=(24, 12))
        fig.suptitle('📊 增强版X轴眼震UKF预测分析 - 第4页：相关性与预测质量分析', fontsize=24, y=0.95)
        
        gs = fig.add_gridspec(1, 2, hspace=0.3, wspace=0.25)
        
        # 1. 真实值vs预测值散点图（增强版）
        ax1 = fig.add_subplot(gs[0, 0])
        
        # 根据误差大小着色散点
        colors = self.data['绝对误差_X']
        scatter = ax1.scatter(self.data['actualX'], self.data['predictedX'], 
                             c=colors, cmap='viridis_r', alpha=0.6, s=30)
        
        # 添加颜色条
        cbar = plt.colorbar(scatter, ax=ax1)
        cbar.set_label('绝对误差 (像素)', fontsize=14)
        
        # 添加理想预测线（y=x）
        min_val = min(self.data['actualX'].min(), self.data['predictedX'].min())
        max_val = max(self.data['actualX'].max(), self.data['predictedX'].max())
        ax1.plot([min_val, max_val], [min_val, max_val], 'r--', linewidth=3, 
                label='理想预测线', alpha=0.8)
        
        # 添加回归线
        z = np.polyfit(self.data['actualX'], self.data['predictedX'], 1)
        p = np.poly1d(z)
        ax1.plot([min_val, max_val], p([min_val, max_val]), 'g-', linewidth=3, 
                label=f'回归线: y={z[0]:.3f}x+{z[1]:.1f}', alpha=0.8)
        
        # 添加置信区间
        residuals = self.data['predictedX'] - p(self.data['actualX'])
        mse = np.mean(residuals**2)
        std_err = np.sqrt(mse)
        
        confidence_band = 1.96 * std_err  # 95%置信区间
        ax1.fill_between([min_val, max_val], 
                        p([min_val, max_val]) - confidence_band,
                        p([min_val, max_val]) + confidence_band,
                        alpha=0.2, color='green', label='95%置信区间')
        
        # 添加统计信息
        stats_text = f'相关系数: {self.metrics["X轴相关性"]:.3f}\nR²: {self.metrics["X轴R²"]:.3f}\n修正NRMSE: {self.metrics["X轴NRMSE_修正"]:.4f}'
        if self.trajectory_analysis.get('方向模式'):
            stats_text += f'\n方向模式: {self.trajectory_analysis["方向模式"]}'
        
        ax1.text(0.05, 0.95, stats_text, transform=ax1.transAxes, 
                fontsize=14, bbox=dict(boxstyle="round,pad=0.3", facecolor="lightblue"),
                verticalalignment='top')
        
        ax1.set_xlabel('真实X坐标 (像素)', fontsize=18)
        ax1.set_ylabel('预测X坐标 (像素)', fontsize=18)
        ax1.set_title('X轴真实值vs预测值散点图（色彩编码误差）', fontsize=20, pad=20)
        ax1.legend(fontsize=14, loc='lower right')
        ax1.grid(True, alpha=0.3)
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 2. 眼震方向性和位移分析
        ax2 = fig.add_subplot(gs[0, 1])
        
        # 计算相对于中心位置的位移
        center_pos = np.mean(self.data['actualX'])
        displacement_from_center = self.data['actualX'] - center_pos
        
        # 创建方向性散点图（时间 vs 位移）
        # 根据速度方向着色
        velocity_colors = np.where(self.data['速度_X'] > 0, 'red', 'blue')
        velocity_colors = np.where(np.abs(self.data['速度_X']) < 10, 'gray', velocity_colors)
        
        # 绘制散点，根据运动方向着色
        for direction, color, label in [('right', 'red', '右向运动'), 
                                       ('left', 'blue', '左向运动'), 
                                       ('static', 'gray', '相对静止')]:
            if direction == 'right':
                mask = self.data['速度_X'] > 10
            elif direction == 'left':
                mask = self.data['速度_X'] < -10
            else:
                mask = np.abs(self.data['速度_X']) <= 10
            
            if np.any(mask):
                ax2.scatter(self.data.loc[mask, 'frameId'], 
                           displacement_from_center[mask],
                           c=color, alpha=0.6, s=20, label=label)
        
        # 添加中心线
        ax2.axhline(y=0, color='black', linestyle='-', linewidth=2, alpha=0.8, label='中心位置')
        
        # 添加运动范围线
        if self.trajectory_analysis:
            rightward_extent = self.trajectory_analysis.get('右向最大位移', 0)
            leftward_extent = self.trajectory_analysis.get('左向最大位移', 0)
            ax2.axhline(y=rightward_extent, color='red', linestyle='--', 
                       linewidth=2, alpha=0.7, label=f'右向边界: +{rightward_extent:.1f}px')
            ax2.axhline(y=-leftward_extent, color='blue', linestyle='--', 
                       linewidth=2, alpha=0.7, label=f'左向边界: -{leftward_extent:.1f}px')
        
        ax2.set_xlabel('帧ID', fontsize=18)
        ax2.set_ylabel('相对中心位移 (像素)', fontsize=18)
        ax2.set_title('X轴眼震方向性和位移分析', fontsize=20, pad=20)
        ax2.legend(fontsize=12, loc='upper right')
        ax2.grid(True, alpha=0.3)
        ax2.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加方向性统计信息
        if self.trajectory_analysis:
            direction_info = f'右向运动: {self.trajectory_analysis.get("右向运动比例", 0)*100:.1f}%\n'
            direction_info += f'左向运动: {self.trajectory_analysis.get("左向运动比例", 0)*100:.1f}%\n'
            direction_info += f'相对静止: {self.trajectory_analysis.get("静止比例", 0)*100:.1f}%\n'
            direction_info += f'方向持续性: {self.trajectory_analysis.get("方向持续性", 0):.3f}\n'
            direction_info += f'方向偏向性: {self.trajectory_analysis.get("方向偏向性", 0):.3f}'
            
            ax2.text(0.02, 0.98, direction_info, 
                    transform=ax2.transAxes, fontsize=12, 
                    verticalalignment='top',
                    bbox=dict(boxstyle='round,pad=0.5', facecolor='lightcyan', alpha=0.8))
        
        plt.tight_layout()
        plt.show()
        
    def create_page5_corrected_reduction_analysis(self):
        """第5页：修正的眼震减缓效果分析"""
        fig = plt.figure(figsize=(24, 14))
        fig.suptitle('📊 增强版X轴眼震UKF预测分析 - 第5页：眼震减缓效果分析（修正版）', fontsize=24, y=0.96)
        
        gs = fig.add_gridspec(2, 1, hspace=0.35, height_ratios=[1, 1])
        
        # 1. 区域分解堆叠图
        ax1 = fig.add_subplot(gs[0, 0])
        
        # 绘制堆叠面积图
        ax1.fill_between(self.data['frameId'], 0, self.data['公共区域_减缓'], 
                        alpha=0.8, color='green', label='公共区域（有效减缓）')
        ax1.fill_between(self.data['frameId'], self.data['公共区域_减缓'], 
                        self.data['公共区域_减缓'] + self.data['残余眼震'], 
                        alpha=0.8, color='orange', label='残余眼震')
        ax1.fill_between(self.data['frameId'], self.data['真实位移_绝对值'], 
                        self.data['真实位移_绝对值'] + self.data['异常增强'], 
                        alpha=0.8, color='red', label='异常增强')
        
        # 添加边界线
        ax1.plot(self.data['frameId'], self.data['真实位移_绝对值'], 'b-', 
                linewidth=3, label='真实眼震边界', alpha=0.9)
        ax1.plot(self.data['frameId'], self.data['预测位移_绝对值'], 'r--', 
                linewidth=3, label='预测位移边界', alpha=0.9)
        
        ax1.set_xlabel('帧ID', fontsize=18)
        ax1.set_ylabel('位移幅度 (像素)', fontsize=18)
        ax1.set_title('眼震减缓区域分解图', fontsize=20, pad=20)
        ax1.legend(fontsize=16, loc='upper right')
        ax1.grid(True, alpha=0.3)
        ax1.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加统计信息
        stats_text = f'平均减缓效率: {self.reduction_analysis["平均减缓效率"]:.1f}%  |  '
        stats_text += f'平均残余眼震: {self.reduction_analysis["平均残余眼震"]:.1f}px  |  '
        stats_text += f'平均异常增强: {self.reduction_analysis["平均异常增强"]:.1f}px\n'
        stats_text += f'改善率: {self.reduction_analysis["改善率"]:.1f}%  |  '
        stats_text += f'恶化率: {self.reduction_analysis["恶化率"]:.1f}%  |  '
        stats_text += f'修正NRMSE: {self.metrics["X轴NRMSE_修正"]:.4f}'
        
        # 添加轨迹相关信息
        if self.trajectory_analysis.get('强度等级'):
            stats_text += f'\n眼震强度: {self.trajectory_analysis["强度等级"]} (评分: {self.trajectory_analysis.get("强度评分", 0):.1f}/10)'
        
        ax1.text(0.02, 0.98, stats_text, transform=ax1.transAxes, 
                fontsize=13, verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='lightblue', alpha=0.8))
        
        # 2. X轴运动轨迹对比（修正版）
        ax2 = fig.add_subplot(gs[1, 0])
        
        # 绘制真实轨迹和预测轨迹
        ax2.plot(self.data['frameId'], self.data['真实位移_X'], 'r-', 
                linewidth=3, alpha=0.8, label='真实眼震轨迹')
        ax2.plot(self.data['frameId'], self.data['预测位移_X'], 'b-', 
                linewidth=3, alpha=0.8, label='UKF预测轨迹')
        
        # 添加零线
        ax2.axhline(y=0, color='black', linestyle='-', linewidth=2, alpha=0.8)
        
        # 填充公共区域（减缓区域）
        for i in range(len(self.data)):
            frame_id = self.data.iloc[i]['frameId']
            real_pos = self.data.iloc[i]['真实位移_X']
            pred_pos = self.data.iloc[i]['预测位移_X']
            
            # 计算公共区域范围
            if real_pos >= 0 and pred_pos >= 0:  # 都在正向
                common_range = min(real_pos, pred_pos)
                ax2.fill_between([frame_id-0.5, frame_id+0.5], [0, 0], [common_range, common_range], 
                               alpha=0.3, color='green')
            elif real_pos <= 0 and pred_pos <= 0:  # 都在负向
                common_range = max(real_pos, pred_pos)
                ax2.fill_between([frame_id-0.5, frame_id+0.5], [0, 0], [common_range, common_range], 
                               alpha=0.3, color='green')
        
        ax2.set_xlabel('帧ID', fontsize=18)
        ax2.set_ylabel('相对第一帧的X位移 (像素)', fontsize=18)
        ax2.set_title('X轴眼震轨迹对比（绿色区域为有效减缓区域）', fontsize=20, pad=20)
        ax2.legend(fontsize=16)
        ax2.grid(True, alpha=0.3)
        ax2.tick_params(axis='both', which='major', labelsize=16)
        
        # 添加轨迹统计信息
        trajectory_stats = f'真实轨迹: 最大位移±{self.data["真实位移_绝对值"].max():.1f}px, 平均{self.data["真实位移_绝对值"].mean():.1f}px\n'
        trajectory_stats += f'预测轨迹: 最大位移±{self.data["预测位移_绝对值"].max():.1f}px, 平均{self.data["预测位移_绝对值"].mean():.1f}px\n'
        trajectory_stats += f'公共区域: 平均{self.reduction_analysis["平均公共区域"]:.1f}px, 残余眼震: 平均{self.reduction_analysis["平均残余眼震"]:.1f}px'
        
        # 添加轨迹特征信息
        if self.trajectory_analysis.get('波形类型'):
            trajectory_stats += f'\n波形类型: {self.trajectory_analysis["波形类型"]}'
        if self.trajectory_analysis.get('方向模式'):
            trajectory_stats += f', 方向模式: {self.trajectory_analysis["方向模式"]}'
            
        ax2.text(0.02, 0.98, trajectory_stats, transform=ax2.transAxes, 
                fontsize=11, verticalalignment='top',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='lightyellow', alpha=0.8))
        
        plt.tight_layout()
        plt.show()

    def create_page6_trajectory_summary(self):
        """第6页：轨迹特征总结页面（新增）"""
        fig = plt.figure(figsize=(24, 14))
        fig.suptitle('📊 增强版X轴眼震UKF预测分析 - 第6页：眼震轨迹特征全面总结', fontsize=24, y=0.96)
        
        gs = fig.add_gridspec(2, 2, hspace=0.35, wspace=0.25)
        
        # 1. 眼震特征雷达图
        ax1 = fig.add_subplot(gs[0, 0], projection='polar')
        
        # 准备雷达图数据
        categories = ['强度评分', '频率稳定性', '波形对称性', '波形周期性', 
                     '方向持续性', '运动连续性', '信号复杂性']
        
        values = [
            self.trajectory_analysis.get('强度评分', 0) / 10,  # 归一化到0-1
            self.trajectory_analysis.get('频率稳定性', 0),
            self.trajectory_analysis.get('波形对称性', 0),
            self.trajectory_analysis.get('波形周期性', 0),
            self.trajectory_analysis.get('方向持续性', 0),
            self.trajectory_analysis.get('运动连续性', 0),
            min(1, self.trajectory_analysis.get('信号复杂性', 0))  # 限制在0-1范围
        ]
        
        # 补全雷达图（闭合）
        values += values[:1]
        angles = np.linspace(0, 2 * np.pi, len(categories), endpoint=False).tolist()
        angles += angles[:1]
        
        # 绘制雷达图
        ax1.plot(angles, values, 'o-', linewidth=3, color='blue', alpha=0.7)
        ax1.fill(angles, values, alpha=0.25, color='blue')
        
        # 设置标签
        ax1.set_xticks(angles[:-1])
        ax1.set_xticklabels(categories, fontsize=12)
        ax1.set_ylim(0, 1)
        ax1.set_yticks([0.2, 0.4, 0.6, 0.8, 1.0])
        ax1.set_yticklabels(['0.2', '0.4', '0.6', '0.8', '1.0'], fontsize=10)
        ax1.set_title('眼震特征雷达图', fontsize=16, pad=20)
        ax1.grid(True)
        
        # 2. 频率分布饼图
        ax2 = fig.add_subplot(gs[0, 1])
        
        if self.trajectory_analysis.get('频带分布'):
            # 使用计算出的频带分布
            band_data = self.trajectory_analysis['频带分布']
            # 只显示占比超过5%的频带
            significant_bands = {k: v for k, v in band_data.items() if v > 5}
            
            if significant_bands:
                labels = list(significant_bands.keys())
                sizes = list(significant_bands.values())
                colors = ['#ff9999', '#66b3ff', '#99ff99', '#ffcc99', '#ff99cc']
                
                wedges, texts, autotexts = ax2.pie(sizes, labels=labels, colors=colors[:len(labels)], 
                                                   autopct='%1.1f%%', startangle=90)
                
                for text in texts:
                    text.set_fontsize(11)
                for autotext in autotexts:
                    autotext.set_fontsize(10)
                    autotext.set_color('white')
                    autotext.set_weight('bold')
        else:
            # 如果没有频带分布数据，显示主频率信息
            ax2.text(0.5, 0.5, f'主频率\n{self.trajectory_analysis.get("主频率", 0):.2f} Hz\n\n{self.trajectory_analysis.get("频率分类", "未知")}', 
                    ha='center', va='center', fontsize=16, 
                    bbox=dict(boxstyle="round,pad=0.3", facecolor="lightblue"))
        
        ax2.set_title('频带能量分布', fontsize=16, pad=20)
        
        # 3. 方向性分析柱状图
        ax3 = fig.add_subplot(gs[1, 0])
        
        direction_data = [
            self.trajectory_analysis.get('右向运动比例', 0) * 100,
            self.trajectory_analysis.get('左向运动比例', 0) * 100,
            self.trajectory_analysis.get('静止比例', 0) * 100
        ]
        direction_labels = ['右向运动', '左向运动', '相对静止']
        colors = ['red', 'blue', 'gray']
        
        bars = ax3.bar(direction_labels, direction_data, color=colors, alpha=0.7)
        
        # 添加数值标签
        for bar, value in zip(bars, direction_data):
            height = bar.get_height()
            ax3.text(bar.get_x() + bar.get_width()/2., height + 1,
                    f'{value:.1f}%', ha='center', va='bottom', fontsize=12, fontweight='bold')
        
        ax3.set_ylabel('时间比例 (%)', fontsize=14)
        ax3.set_title('运动方向分布', fontsize=16, pad=20)
        ax3.set_ylim(0, max(direction_data) * 1.2)
        ax3.grid(True, alpha=0.3, axis='y')
        
        # 添加方向模式信息
        direction_pattern = self.trajectory_analysis.get('方向模式', '未知')
        ax3.text(0.5, 0.95, f'方向模式: {direction_pattern}', 
                transform=ax3.transAxes, ha='center', va='top',
                bbox=dict(boxstyle="round,pad=0.3", facecolor="lightyellow"),
                fontsize=12, fontweight='bold')
        
        # 4. 综合评估文本总结
        ax4 = fig.add_subplot(gs[1, 1])
        ax4.axis('off')  # 隐藏坐标轴
        
        # 构建综合评估文本
        summary_text = "🏥 眼震轨迹综合评估报告\n"
        summary_text += "=" * 35 + "\n\n"
        
        # 基本分类
        nystagmus_types = self.trajectory_analysis.get('眼震类型', ['未知'])
        summary_text += f"📊 眼震分类: {', '.join(nystagmus_types)}\n"
        summary_text += f"🎯 运动模式: {self.trajectory_analysis.get('眼震模式', '未知')}\n"
        summary_text += f"💪 强度等级: {self.trajectory_analysis.get('强度等级', '未知')}\n"
        summary_text += f"    评分: {self.trajectory_analysis.get('强度评分', 0):.1f}/10\n\n"
        
        # 频率特征
        if self.trajectory_analysis.get('主频率', 0) > 0:
            summary_text += f"📡 主频率: {self.trajectory_analysis['主频率']:.2f} Hz\n"
            summary_text += f"📈 频率分类: {self.trajectory_analysis.get('频率分类', '未知')}\n"
            summary_text += f"🎯 频率稳定性: {self.trajectory_analysis.get('频率稳定性', 0):.2f}\n\n"
        else:
            summary_text += f"📡 频率特征: 无明显周期性\n\n"
        
        # 波形特征
        summary_text += f"🌊 波形特征:\n"
        summary_text += f"    类型: {self.trajectory_analysis.get('波形类型', '未知')}\n"
        summary_text += f"    对称性: {self.trajectory_analysis.get('波形对称性', 0):.2f}\n"
        summary_text += f"    周期性: {self.trajectory_analysis.get('波形周期性', 0):.2f}\n\n"
        
        # 方向性特征
        summary_text += f"🧭 方向性特征:\n"
        summary_text += f"    模式: {self.trajectory_analysis.get('方向模式', '未知')}\n"
        summary_text += f"    持续性: {self.trajectory_analysis.get('方向持续性', 0):.2f}\n"
        summary_text += f"    偏向性: {self.trajectory_analysis.get('方向偏向性', 0):.2f}\n\n"
        
        # 运动质量
        summary_text += f"⚡ 运动质量:\n"
        summary_text += f"    连续性: {self.trajectory_analysis.get('运动连续性', 0)*100:.1f}%\n"
        summary_text += f"    复杂性: {self.trajectory_analysis.get('信号复杂性', 0):.3f}\n"
        summary_text += f"    平均速度: {self.trajectory_analysis.get('平均速度', 0):.1f} px/s\n\n"
        
        # 临床建议
        summary_text += f"🏥 临床建议:\n"
        
        # 根据分析结果给出建议
        intensity_score = self.trajectory_analysis.get('强度评分', 0)
        main_freq = self.trajectory_analysis.get('主频率', 0)
        
        if intensity_score >= 7:
            summary_text += f"⚠️  高强度眼震，建议详细检查\n"
        elif intensity_score >= 4:
            summary_text += f"📋 中等强度，建议定期观察\n"
        else:
            summary_text += f"✅ 轻度眼震，继续监测\n"
        
        if 1 <= main_freq <= 5:
            summary_text += f"📈 频率正常范围\n"
        elif main_freq > 8:
            summary_text += f"⚠️  高频特征，需进一步评估\n"
        
        # 显示文本
        ax4.text(0.05, 0.95, summary_text, transform=ax4.transAxes, 
                fontsize=11, verticalalignment='top', horizontalalignment='left',
                bbox=dict(boxstyle="round,pad=0.5", facecolor="lightcyan", alpha=0.8),
                family='monospace')
        
        plt.tight_layout()
        plt.show()

    def run_enhanced_analysis_with_trajectory(self):
        """运行包含深度轨迹分析的完整X轴眼震UKF预测分析流程"""
        if self.data is None:
            return
        
        print("\n" + "="*90)
        print("🚀 增强版X轴眼震UKF预测分析系统 v4.0（含深度轨迹特性分析）")
        print("="*90)
        
        # 启用matplotlib的交互模式
        plt.ion()
        
        # 1. 深度眼震轨迹特性分析（新增）
        self.analyze_nystagmus_trajectory_characteristics()
        
        # 2. 计算误差
        self.calculate_errors()
        
        # 3. 计算准确性指标
        self.calculate_accuracy_metrics()
        
        # 4. 分析眼震减缓效果（修正版）
        self.analyze_nystagmus_reduction()
        
        # 5. 打印轨迹分析报告
        self.print_comprehensive_trajectory_report()
        
        # 6. 打印UKF预测分析报告
        self.print_analysis_report()
        
        # 7. 创建6页可视化分析
        print(f"\n📊 正在生成6页增强版X轴专用可视化分析...")
        print("   🖱️ 所有图表支持鼠标滚轮缩放和拖动平移")
        print("   ✨ 新增：深度眼震轨迹特性分析")
        print("   🔬 新增：眼震类型、模式、频率、波形、方向性全面分析")
        print("   📈 新增：临床意义评估")
        
        print("   第1页：X轴跟踪效果与误差分析（含轨迹信息）")
        self.create_page1_tracking_and_error()
        
        print("   第2页：X轴精度分析（含轨迹特征）")
        self.create_page2_precision_analysis()
        
        print("   第3页：深度眼震轨迹特性分析（动力学+频谱+波形）")
        self.create_page3_enhanced_nystagmus_analysis()
        
        print("   第4页：相关性与方向性分析（散点+方向性）")
        self.create_page4_scatter_analysis()
        
        print("   第5页：眼震减缓效果分析（修正版+轨迹特征）")
        self.create_page5_corrected_reduction_analysis()
        
        print("   第6页：眼震轨迹特征全面总结（雷达图+综合评估）")
        self.create_page6_trajectory_summary()
        
        print("\n" + "="*90)
        print("✅ 增强版X轴眼震UKF预测分析完成！")
        print("   🎯 专注于X轴数据深度分析")
        print("   📊 6页专业可视化展示")
        print("   🔬 全面的眼震轨迹特性分析：")
        print("      • 眼震类型识别（幅度、速度、规律性分类）")
        print("      • 眼震模式识别（摆动性、冲动性、混合性）")
        print("      • 增强版频率分析（主频率、谐波、频带分布）")
        print("      • 眼震强度评估（多维度强度指标）")
        print("      • 波形特征分析（对称性、平滑度、周期性）")
        print("      • 方向性分析（方向偏好、持续性、运动范围）")
        print("   📈 全面的UKF预测精度评估（含NRMSE）")
        print("   🎯 修正版眼震减缓效果分析")
        print("   🏥 临床意义评估和建议")
        print("   📋 综合轨迹特征总结报告")
        print("="*90)


# 使用示例
if __name__ == "__main__":
    print("👁️ 增强版X轴眼震UKF预测准确性分析工具 v4.0（含深度轨迹分析）")
    print("="*90)
    print("✨ 新增特性:")
    print("   🔬 深度眼震轨迹特性分析")
    print("   📊 眼震类型自动识别（幅度/速度/规律性分类）")
    print("   🌊 眼震模式识别（摆动性/冲动性/混合性/漂移性）")
    print("   📡 增强版频率分析（主频率/谐波/频带分布/稳定性）")
    print("   💪 多维度强度评估（位移/速度/加速度/复杂性/连续性）")
    print("   🌊 波形特征分析（对称性/平滑度/周期性/类型分类）")
    print("   🧭 方向性特征分析（方向偏好/持续性/运动范围）")
    print("   🏥 临床意义评估和建议")
    print("   📋 6页专业可视化（新增轨迹特征总结页）")
    print("   🎯 雷达图展示眼震特征全貌")
    print("   📈 综合轨迹特征报告")
    print("="*90)
    print("💡 保留原有功能:")
    print("   🎯 UKF预测准确性分析")
    print("   📊 NRMSE（归一化RMSE）分析")
    print("   🔄 修正版眼震减缓效果分析")
    print("   🖱️ 所有图表支持鼠标交互")
    print("="*90)
    
    # 创建增强版X轴分析器
    analyzer = EnhancedXAxisNystagmusAnalyzer('prediction_only_data.csv', fps=60)
    
    # 运行包含深度轨迹分析的完整分析
    if analyzer.data is not None:
        analyzer.run_enhanced_analysis_with_trajectory()
    else:
        print("❌ 无法加载数据文件，请检查文件路径")
        print("   确保文件包含必要的列: frameId, actualX, predictedX")
    
    # 保持图形窗口打开
    input("\n按回车键退出...")