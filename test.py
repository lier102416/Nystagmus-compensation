import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from matplotlib.patches import Circle
import warnings
warnings.filterwarnings('ignore')

# 设置中文字体支持
plt.rcParams['font.sans-serif'] = ['SimHei', 'DejaVu Sans']
plt.rcParams['axes.unicode_minus'] = False

class GazeTrajectoryAnalyzer:
    def __init__(self, csv_file_path):
        """初始化分析器并加载数据"""
        self.data = pd.read_csv(csv_file_path)
        self.prepare_data()
    
    def prepare_data(self):
        """数据预处理"""
        # 计算各种误差
        self.data['distance_error'] = np.sqrt(
            (self.data['pGazeX'] - self.data['GazeX'])**2 + 
            (self.data['pGazeY'] - self.data['GazeY'])**2
        )
        self.data['error_x'] = self.data['pGazeX'] - self.data['GazeX']
        self.data['error_y'] = self.data['pGazeY'] - self.data['GazeY']
        
        # 计算移动速度（相邻点之间的距离）
        if len(self.data) > 1:
            real_speed = np.sqrt(np.diff(self.data['GazeX'])**2 + np.diff(self.data['GazeY'])**2)
            pred_speed = np.sqrt(np.diff(self.data['pGazeX'])**2 + np.diff(self.data['pGazeY'])**2)
            
            # 在数组前面添加0，保持与原数据长度一致
            self.data['real_speed'] = np.concatenate([[0], real_speed])
            self.data['pred_speed'] = np.concatenate([[0], pred_speed])
        else:
            self.data['real_speed'] = 0
            self.data['pred_speed'] = 0
        
        print(f"数据总数: {len(self.data)}")
        print(f"平均距离误差: {self.data['distance_error'].mean():.4f}")
        print(f"最大距离误差: {self.data['distance_error'].max():.4f}")
    
    def plot_time_trajectory_analysis(self):
        """时间轨迹分析 - 主要图表"""
        # 调整为2x2布局，移除二维空间轨迹图
        fig = plt.figure(figsize=(16, 10))
        
        # 1. 时间轨迹图 - X坐标
        ax1 = plt.subplot(2, 2, 1)
        plt.plot(self.data['index'], self.data['GazeX'], 
                'b-', linewidth=2, alpha=0.8, label='真实X轨迹', marker='o', markersize=3)
        plt.plot(self.data['index'], self.data['pGazeX'], 
                'r--', linewidth=2, alpha=0.8, label='预测X轨迹', marker='s', markersize=3)
        plt.xlabel('时间点(索引)')
        plt.ylabel('X坐标')
        plt.title('X坐标时间轨迹对比', pad=15)
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # 2. 时间轨迹图 - Y坐标
        ax2 = plt.subplot(2, 2, 2)
        plt.plot(self.data['index'], self.data['GazeY'], 
                'b-', linewidth=2, alpha=0.8, label='真实Y轨迹', marker='o', markersize=3)
        plt.plot(self.data['index'], self.data['pGazeY'], 
                'r--', linewidth=2, alpha=0.8, label='预测Y轨迹', marker='s', markersize=3)
        plt.xlabel('时间点(索引)')
        plt.ylabel('Y坐标')
        plt.title('Y坐标时间轨迹对比', pad=15)
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # 3. 距离误差时间序列
        ax3 = plt.subplot(2, 2, 3)
        plt.plot(self.data['index'], self.data['distance_error'], 
                'g-', linewidth=2, alpha=0.8, marker='o', markersize=2)
        plt.fill_between(self.data['index'], self.data['distance_error'], 
                        alpha=0.3, color='green')
        plt.xlabel('时间点(索引)')
        plt.ylabel('距离误差')
        plt.title('距离误差随时间变化', pad=15)
        plt.grid(True, alpha=0.3)
        
        # 4. X和Y方向误差时间序列
        ax4 = plt.subplot(2, 2, 4)
        plt.plot(self.data['index'], self.data['error_x'], 
                'orange', linewidth=2, alpha=0.8, label='X方向误差', marker='o', markersize=2)
        plt.plot(self.data['index'], self.data['error_y'], 
                'purple', linewidth=2, alpha=0.8, label='Y方向误差', marker='s', markersize=2)
        plt.axhline(y=0, color='black', linestyle='-', alpha=0.5)
        plt.xlabel('时间点(索引)')
        plt.ylabel('误差值')
        plt.title('X/Y方向误差随时间变化', pad=15)
        plt.legend()
        plt.grid(True, alpha=0.3)
        
        # 调整子图间距
        plt.subplots_adjust(left=0.08, bottom=0.08, right=0.95, top=0.92, 
                           wspace=0.35, hspace=0.35)
        
        plt.savefig('gaze_trajectory_analysis.png', dpi=300, bbox_inches='tight')
        plt.show()
    
    def plot_error_analysis(self):
        """误差分析图表"""
        fig, axes = plt.subplots(1, 2, figsize=(16, 6))
        
        # 1. 误差分布直方图
        ax1 = axes[0]
        ax1.hist(self.data['distance_error'], bins=30, alpha=0.7, color='skyblue', 
                edgecolor='black', density=True)
        ax1.axvline(self.data['distance_error'].mean(), color='red', 
                   linestyle='--', linewidth=2, label=f'均值: {self.data["distance_error"].mean():.3f}')
        ax1.axvline(self.data['distance_error'].median(), color='green', 
                   linestyle='--', linewidth=2, label=f'中位数: {self.data["distance_error"].median():.3f}')
        ax1.set_xlabel('距离误差')
        ax1.set_ylabel('密度')
        ax1.set_title('距离误差分布', pad=15)
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # 2. 误差随时间变化的趋势分析
        ax2 = axes[1]
        # 计算移动平均
        window_size = max(5, len(self.data) // 20)
        if len(self.data) >= window_size:
            moving_avg = self.data['distance_error'].rolling(window=window_size, center=True).mean()
            moving_std = self.data['distance_error'].rolling(window=window_size, center=True).std()
            
            ax2.plot(self.data['index'], self.data['distance_error'], 
                    alpha=0.3, color='gray', label='原始误差')
            ax2.plot(self.data['index'], moving_avg, 
                    color='red', linewidth=3, label=f'移动平均(窗口={window_size})')
            ax2.fill_between(self.data['index'], 
                           moving_avg - moving_std, moving_avg + moving_std,
                           alpha=0.2, color='red', label='±1标准差')
        else:
            ax2.plot(self.data['index'], self.data['distance_error'], 
                    color='red', linewidth=2, marker='o')
        
        ax2.set_xlabel('时间点(索引)')
        ax2.set_ylabel('距离误差')
        ax2.set_title('误差趋势分析', pad=15)
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # 调整子图间距
        plt.subplots_adjust(left=0.08, bottom=0.15, right=0.95, top=0.88, 
                           wspace=0.3)
        
        plt.savefig('gaze_error_analysis.png', dpi=300, bbox_inches='tight')
        plt.show()
    
    def plot_speed_analysis(self):
        """移动速度分析"""
        fig, axes = plt.subplots(1, 2, figsize=(16, 6))
        
        # 1. 移动速度对比
        ax1 = axes[0]
        ax1.plot(self.data['index'], self.data['real_speed'], 
                'b-', linewidth=2, alpha=0.8, label='真实移动速度', marker='o', markersize=2)
        ax1.plot(self.data['index'], self.data['pred_speed'], 
                'r--', linewidth=2, alpha=0.8, label='预测移动速度', marker='s', markersize=2)
        ax1.set_xlabel('时间点(索引)')
        ax1.set_ylabel('移动速度')
        ax1.set_title('移动速度时间对比', pad=15)
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # 2. 速度误差分析
        ax2 = axes[1]
        speed_error = self.data['pred_speed'] - self.data['real_speed']
        ax2.plot(self.data['index'], speed_error, 
                'purple', linewidth=2, alpha=0.8, marker='o', markersize=2)
        ax2.axhline(y=0, color='black', linestyle='-', alpha=0.5)
        ax2.fill_between(self.data['index'], speed_error, 
                        alpha=0.3, color='purple')
        ax2.set_xlabel('时间点(索引)')
        ax2.set_ylabel('速度误差')
        ax2.set_title('速度预测误差随时间变化', pad=15)
        ax2.grid(True, alpha=0.3)
        
        # 调整子图间距
        plt.subplots_adjust(left=0.08, bottom=0.15, right=0.95, top=0.88, 
                           wspace=0.3)
        
        plt.savefig('gaze_speed_analysis.png', dpi=300, bbox_inches='tight')
        plt.show()
    
    def generate_summary_report(self):
        """生成分析报告"""
        print("="*70)
        print("🎯 注视点轨迹与误差分析报告")
        print("="*70)
        
        print(f"\n📊 数据概况:")
        print(f"   总数据点数: {len(self.data)}")
        print(f"   时间范围: {self.data['index'].min()} - {self.data['index'].max()}")
        
        print(f"\n📍 空间范围:")
        print(f"   真实注视点X范围: {self.data['GazeX'].min():.2f} - {self.data['GazeX'].max():.2f}")
        print(f"   真实注视点Y范围: {self.data['GazeY'].min():.2f} - {self.data['GazeY'].max():.2f}")
        print(f"   预测注视点X范围: {self.data['pGazeX'].min():.2f} - {self.data['pGazeX'].max():.2f}")
        print(f"   预测注视点Y范围: {self.data['pGazeY'].min():.2f} - {self.data['pGazeY'].max():.2f}")
        
        print(f"\n📏 距离误差分析:")
        print(f"   平均距离误差: {self.data['distance_error'].mean():.4f}")
        print(f"   距离误差标准差: {self.data['distance_error'].std():.4f}")
        print(f"   最大距离误差: {self.data['distance_error'].max():.4f}")
        print(f"   最小距离误差: {self.data['distance_error'].min():.4f}")
        print(f"   中位数距离误差: {self.data['distance_error'].median():.4f}")
        
        print(f"\n📐 方向误差分析:")
        print(f"   X方向平均误差: {self.data['error_x'].mean():.4f}")
        print(f"   Y方向平均误差: {self.data['error_y'].mean():.4f}")
        print(f"   X方向误差标准差: {self.data['error_x'].std():.4f}")
        print(f"   Y方向误差标准差: {self.data['error_y'].std():.4f}")
        
        print(f"\n🎯 精度评估:")
        accuracy_metrics = [1.0, 2.0, 3.0, 5.0, 10.0]
        for threshold in accuracy_metrics:
            accuracy = (self.data['distance_error'] <= threshold).mean() * 100
            print(f"   {threshold}像素内准确率: {accuracy:.2f}%")
        
        print(f"\n🚀 运动特征:")
        if len(self.data) > 1:
            total_real_distance = self.data['real_speed'].sum()
            total_pred_distance = self.data['pred_speed'].sum()
            print(f"   真实轨迹总长度: {total_real_distance:.2f}")
            print(f"   预测轨迹总长度: {total_pred_distance:.2f}")
            print(f"   轨迹长度误差: {abs(total_real_distance - total_pred_distance):.2f}")
            
            avg_real_speed = self.data['real_speed'].mean()
            avg_pred_speed = self.data['pred_speed'].mean()
            print(f"   平均真实移动速度: {avg_real_speed:.4f}")
            print(f"   平均预测移动速度: {avg_pred_speed:.4f}")
        
        print(f"\n📈 数据质量评估:")
        correlation_x = np.corrcoef(self.data['GazeX'], self.data['pGazeX'])[0, 1]
        correlation_y = np.corrcoef(self.data['GazeY'], self.data['pGazeY'])[0, 1]
        print(f"   X坐标相关系数: {correlation_x:.4f}")
        print(f"   Y坐标相关系数: {correlation_y:.4f}")
        
        if hasattr(self.data, 'predictionError'):
            pred_error_corr = self.data['predictionError'].corr(self.data['distance_error'])
            print(f"   预测误差与距离误差相关性: {pred_error_corr:.4f}")

# 使用示例
if __name__ == "__main__":
    # 替换为您的CSV文件路径
    csv_file_path = "collected_measurement_data.csv"
    
    try:
        # 创建分析器实例
        analyzer = GazeTrajectoryAnalyzer(csv_file_path)
        
        # 生成主要轨迹分析图表
        analyzer.plot_time_trajectory_analysis()
        
        # 生成误差分析图表
        # analyzer.plot_error_analysis()
        
        # 生成速度分析图表
        # analyzer.plot_speed_analysis()
        
        # 生成分析报告
        analyzer.generate_summary_report()
        
        print(f"\n✅ 分析完成！图片已保存为:")
        print("   - gaze_trajectory_analysis.png (主要轨迹分析)")
        print("   - gaze_error_analysis.png (误差分析)")
        print("   - gaze_speed_analysis.png (速度分析)")
        
    except FileNotFoundError:
        print(f"❌ 错误: 找不到文件 '{csv_file_path}'")
        print("请确保CSV文件存在并且路径正确")
    except Exception as e:
        print(f"❌ 分析过程中出现错误: {str(e)}")
        import traceback
        traceback.print_exc()