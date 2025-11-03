import pygame
import random
import time
import math
import numpy as np
import os
import sys

class EnhancedNystagmusStimulus:
    """基于最新文献优化的眼震刺激系统"""
    
    def __init__(self):
        """初始化增强的眼震刺激参数"""
        # 基于文献的最优参数设置
        self.viewing_distance_cm = 60  # 标准观看距离
        self.screen_ppi = 96  # 屏幕PPI
        
        # 空间频率参数（基于文献0.08-1.6 c/deg）
        self.spatial_frequencies = [0.08, 0.2, 0.4, 0.8, 1.2, 1.6]  # cycles/degree
        self.current_sf_index = 2  # 默认0.4 c/deg
        
        # 速度参数（基于文献21-135°/s）
        self.velocity_levels = [21, 45, 60, 90, 135]  # degrees/second
        self.current_velocity_index = 2  # 默认60°/s
        
        # 对比度参数（基于文献8.2%-55.5%）
        self.contrast_levels = [0.1, 0.2, 0.4, 0.6, 0.8, 1.0]
        self.current_contrast_index = 4  # 默认80%
        
        # 刺激类型
        self.stimulus_types = ['square_wave', 'sinusoidal', 'random_dots', 'checkerboard']
        self.current_stimulus_type = 'sinusoidal'
        
        # 方向控制（增强版）
        self.directions = {
            'horizontal_right': (1, 0, "水平向右"),
            'horizontal_left': (-1, 0, "水平向左"),
            'vertical_up': (0, -1, "垂直向上"),
            'vertical_down': (0, 1, "垂直向下"),
            'diagonal_ur': (0.707, -0.707, "对角右上"),
            'diagonal_dl': (-0.707, 0.707, "对角左下"),
            'circular_cw': ('circular', 1, "顺时针旋转"),
            'circular_ccw': ('circular', -1, "逆时针旋转")
        }
        self.direction_sequence = list(self.directions.keys())
        self.current_direction_index = 0
        
        # 眼震节律参数（优化版）
        self.slow_phase_duration_range = (0.8, 2.0)  # 慢相持续时间
        self.fast_phase_duration_range = (0.05, 0.2)  # 快相持续时间
        self.current_phase = "slow"
        self.phase_timer = 0
        self.slow_phase_duration = random.uniform(*self.slow_phase_duration_range)
        self.fast_phase_duration = random.uniform(*self.fast_phase_duration_range)
        
        # 适应性参数（基于文献）
        self.adaptation_phases = {
            'initial': (0, 30),      # 初始响应期
            'adaptation': (30, 180), # 适应期 
            'stable': (180, 600)     # 稳定期
        }
        self.adaptation_factor = 1.0
        
        # 自动程序控制
        self.auto_sequence_mode = True
        self.sequence_timer = 0
        self.sequence_duration = 45  # 每个方向45秒
        
        # 全视野刺激参数
        self.full_field_mode = True
        self.dot_density = 0.3
        self.checkerboard_size = 32
        
        # 阈值检测模式
        self.threshold_mode = False
        self.threshold_detector = ThresholdDetector()
        
        # 记录参数
        self.start_time = None
        self.total_cycles = 0
        self.okn_detected = False

    def calculate_stripe_width(self):
        """基于当前空间频率计算条纹宽度"""
        spatial_freq = self.spatial_frequencies[self.current_sf_index]
        
        # 视角计算
        visual_angle_deg = 1 / spatial_freq  # 每个周期的视角
        visual_angle_rad = math.radians(visual_angle_deg)
        
        # 转换为像素
        stripe_width_cm = self.viewing_distance_cm * math.tan(visual_angle_rad / 2)
        stripe_width_pixels = stripe_width_cm * (self.screen_ppi / 2.54)
        
        return max(4, int(stripe_width_pixels))  # 最小4像素
    
    def calculate_velocity_pixels_per_second(self):
        """将角速度转换为像素/秒"""
        angular_velocity = self.velocity_levels[self.current_velocity_index]
        
        # 角速度转换为线速度
        angular_rad = math.radians(angular_velocity)
        linear_velocity_cm = self.viewing_distance_cm * math.tan(angular_rad)
        velocity_pixels = linear_velocity_cm * (self.screen_ppi / 2.54)
        
        return velocity_pixels
    
    def get_current_colors(self):
        """获取当前对比度的颜色"""
        contrast = self.contrast_levels[self.current_contrast_index]
        
        white_val = min(255, int(128 + 127 * contrast))
        black_val = max(0, int(128 - 127 * contrast))
        
        return (white_val, white_val, white_val), (black_val, black_val, black_val)
    
    def get_current_direction_vector(self):
        """获取当前方向矢量"""
        direction_key = self.direction_sequence[self.current_direction_index]
        return self.directions[direction_key]
    
    def update_adaptation(self, elapsed_time):
        """更新适应性参数"""
        # 确定当前适应相位
        current_phase = 'extended'
        for phase, (start, end) in self.adaptation_phases.items():
            if start <= elapsed_time < end:
                current_phase = phase
                break
        
        # 根据相位调整适应因子
        if current_phase == 'initial':
            self.adaptation_factor = 1.0
        elif current_phase == 'adaptation':
            # 适应期：逐渐降低响应
            progress = (elapsed_time - 30) / 150  # 0-1
            self.adaptation_factor = 1.0 - 0.3 * progress  # 降低到70%
        elif current_phase == 'stable':
            self.adaptation_factor = 0.7
        else:
            # 偶尔恢复（模拟注意力重新集中）
            if random.random() < 0.05:
                self.adaptation_factor = min(1.0, self.adaptation_factor + 0.1)
    
    def update_automatic_sequence(self, dt):
        """自动序列控制"""
        if not self.auto_sequence_mode:
            return
            
        self.sequence_timer += dt
        
        if self.sequence_timer >= self.sequence_duration:
            # 切换到下一个方向
            self.current_direction_index = (self.current_direction_index + 1) % len(self.direction_sequence)
            self.sequence_timer = 0
            
            direction_name = self.directions[self.direction_sequence[self.current_direction_index]][2]
            print(f"自动切换方向: {direction_name}")
            
            # 每完成一轮后增加难度
            if self.current_direction_index == 0:
                self.increase_difficulty()
    
    def increase_difficulty(self):
        """增加测试难度"""
        # 增加空间频率
        if self.current_sf_index < len(self.spatial_frequencies) - 1:
            self.current_sf_index += 1
            print(f"增加空间频率到: {self.spatial_frequencies[self.current_sf_index]:.2f} c/deg")
        
        # 或降低对比度
        elif self.current_contrast_index > 0:
            self.current_contrast_index -= 1
            print(f"降低对比度到: {self.contrast_levels[self.current_contrast_index]*100:.0f}%")
    
    def update_nystagmus_rhythm(self, dt):
        """更新眼震节律"""
        self.phase_timer += dt
        
        if self.current_phase == "slow":
            if self.phase_timer >= self.slow_phase_duration:
                # 切换到快相
                self.current_phase = "fast"
                self.phase_timer = 0
                self.fast_phase_duration = random.uniform(*self.fast_phase_duration_range)
                self.total_cycles += 1
        
        elif self.current_phase == "fast":
            if self.phase_timer >= self.fast_phase_duration:
                # 切换到慢相
                self.current_phase = "slow"
                self.phase_timer = 0
                self.slow_phase_duration = random.uniform(*self.slow_phase_duration_range)
    
    def get_current_speed(self):
        """获取当前速度"""
        base_speed = self.calculate_velocity_pixels_per_second()
        
        if self.current_phase == "slow":
            return base_speed * self.adaptation_factor
        else:
            # 快相速度是慢相的3-5倍
            return base_speed * 4 * self.adaptation_factor

class ThresholdDetector:
    """阈值检测器"""
    
    def __init__(self):
        self.detection_history = []
        self.current_threshold_sf = 0.4
        self.current_threshold_contrast = 0.8
        self.step_size_sf = 0.1
        self.step_size_contrast = 0.1
    
    def update(self, okn_detected):
        """更新阈值检测"""
        self.detection_history.append(okn_detected)
        
        if len(self.detection_history) >= 3:
            recent_detections = self.detection_history[-3:]
            
            if all(recent_detections):
                # 连续检测到，增加难度
                self.current_threshold_sf += self.step_size_sf
                self.current_threshold_contrast -= self.step_size_contrast
            elif not any(recent_detections):
                # 连续未检测到，降低难度
                self.current_threshold_sf -= self.step_size_sf
                self.current_threshold_contrast += self.step_size_contrast
            
            # 限制范围
            self.current_threshold_sf = np.clip(self.current_threshold_sf, 0.08, 1.6)
            self.current_threshold_contrast = np.clip(self.current_threshold_contrast, 0.1, 1.0)

class StimulusRenderer:
    """刺激渲染器"""
    
    @staticmethod
    def render_square_wave(screen, params, offset):
        """方波刺激"""
        width, height = screen.get_size()
        stripe_width = params.calculate_stripe_width()
        white_color, black_color = params.get_current_colors()
        
        direction = params.get_current_direction_vector()
        
        if direction[0] != 'circular':
            # 线性运动
            dx, dy = direction[0], direction[1]
            
            if abs(dx) > abs(dy):  # 主要是水平运动
                start_x = -stripe_width + (offset % (stripe_width * 2))
                x = start_x
                stripe_count = 0
                
                while x < width + stripe_width:
                    color = white_color if stripe_count % 2 == 0 else black_color
                    pygame.draw.rect(screen, color, (x, 0, stripe_width, height))
                    x += stripe_width
                    stripe_count += 1
            else:  # 主要是垂直运动
                start_y = -stripe_width + (offset % (stripe_width * 2))
                y = start_y
                stripe_count = 0
                
                while y < height + stripe_width:
                    color = white_color if stripe_count % 2 == 0 else black_color
                    pygame.draw.rect(screen, color, (0, y, width, stripe_width))
                    y += stripe_width
                    stripe_count += 1
        else:
            # 圆形旋转
            StimulusRenderer.render_circular_pattern(screen, params, offset)
    
    @staticmethod
    def render_sinusoidal(screen, params, offset):
        """正弦波刺激（文献推荐）"""
        width, height = screen.get_size()
        spatial_freq = params.spatial_frequencies[params.current_sf_index]
        white_color, black_color = params.get_current_colors()
        
        direction = params.get_current_direction_vector()
        
        if direction[0] != 'circular':
            dx, dy = direction[0], direction[1]
            
            if abs(dx) > abs(dy):  # 水平正弦波
                for x in range(width):
                    phase = 2 * math.pi * spatial_freq * (x + offset) / 100
                    brightness = 128 + 127 * params.contrast_levels[params.current_contrast_index] * math.sin(phase)
                    brightness = int(np.clip(brightness, 0, 255))
                    color = (brightness, brightness, brightness)
                    pygame.draw.line(screen, color, (x, 0), (x, height))
            else:  # 垂直正弦波
                for y in range(height):
                    phase = 2 * math.pi * spatial_freq * (y + offset) / 100
                    brightness = 128 + 127 * params.contrast_levels[params.current_contrast_index] * math.sin(phase)
                    brightness = int(np.clip(brightness, 0, 255))
                    color = (brightness, brightness, brightness)
                    pygame.draw.line(screen, color, (0, y), (width, y))
    
    @staticmethod
    def render_random_dots(screen, params, offset):
        """随机点图案（文献推荐的最佳刺激）"""
        width, height = screen.get_size()
        white_color, black_color = params.get_current_colors()
        
        dot_size = 3
        spacing = 8
        
        direction = params.get_current_direction_vector()
        dx, dy = direction[0], direction[1]
        
        random.seed(42)  # 固定种子确保一致性
        
        for i in range(-spacing, width + spacing, spacing):
            for j in range(-spacing, height + spacing, spacing):
                if random.random() < params.dot_density:
                    # 根据offset移动点
                    x = i + offset * dx
                    y = j + offset * dy
                    
                    # 边界处理
                    x = x % (width + 2 * spacing) - spacing
                    y = y % (height + 2 * spacing) - spacing
                    
                    color = white_color if (i + j) % (2 * spacing) == 0 else black_color
                    if 0 <= x <= width and 0 <= y <= height:
                        pygame.draw.circle(screen, color, (int(x), int(y)), dot_size)
    
    @staticmethod
    def render_checkerboard(screen, params, offset):
        """棋盘格刺激"""
        width, height = screen.get_size()
        check_size = params.checkerboard_size
        white_color, black_color = params.get_current_colors()
        
        direction = params.get_current_direction_vector()
        dx, dy = direction[0], direction[1]
        
        offset_x = int(offset * dx) % (check_size * 2)
        offset_y = int(offset * dy) % (check_size * 2)
        
        for x in range(-check_size, width + check_size, check_size):
            for y in range(-check_size, height + check_size, check_size):
                check_x = x + offset_x
                check_y = y + offset_y
                
                # 确定颜色
                color_index = ((check_x // check_size) + (check_y // check_size)) % 2
                color = white_color if color_index == 0 else black_color
                
                pygame.draw.rect(screen, color, (check_x, check_y, check_size, check_size))
    
    @staticmethod
    def render_circular_pattern(screen, params, offset):
        """圆形旋转图案"""
        width, height = screen.get_size()
        center_x, center_y = width // 2, height // 2
        white_color, black_color = params.get_current_colors()
        
        direction = params.get_current_direction_vector()
        rotation_direction = direction[1]  # 1 for CW, -1 for CCW
        
        # 绘制径向条纹
        num_sectors = 16
        sector_angle = 2 * math.pi / num_sectors
        
        for i in range(num_sectors):
            start_angle = i * sector_angle + offset * rotation_direction * 0.01
            end_angle = start_angle + sector_angle
            
            color = white_color if i % 2 == 0 else black_color
            
            # 绘制扇形
            points = [(center_x, center_y)]
            for angle in np.linspace(start_angle, end_angle, 20):
                x = center_x + min(width, height) // 2 * math.cos(angle)
                y = center_y + min(width, height) // 2 * math.sin(angle)
                points.append((x, y))
            
            if len(points) > 2:
                pygame.draw.polygon(screen, color, points)

def enhanced_nystagmus_test():
    """增强版眼震测试主函数"""
    
    # 初始化pygame
    pygame.init()
    width, height = 1920, 1080
    screen = pygame.display.set_mode((width, height), pygame.FULLSCREEN)
    pygame.display.set_caption("增强版眼震刺激系统 v2.0")
    clock = pygame.time.Clock()
    
    # 初始化刺激参数
    params = EnhancedNystagmusStimulus()
    params.start_time = time.time()
    
    # 初始化渲染器
    renderer = StimulusRenderer()
    
    print("🚀 增强版眼震刺激系统启动!")
    print("="*50)
    print("🎮 控制说明:")
    print("ESC - 退出程序")
    print("SPACE - 手动触发快相")
    print("1-4 - 切换刺激类型 (方波/正弦波/随机点/棋盘格)")
    print("A - 自动/手动序列模式")
    print("T - 阈值检测模式")
    print("← → - 调节空间频率")
    print("↑ ↓ - 调节速度")
    print("[ ] - 调节对比度")
    print("R - 手动切换方向")
    print("F - 全屏/窗口模式")
    
    # 主循环变量
    running = True
    offset = 0
    manual_fast_phase = False
    fullscreen = True
    
    while running:
        dt = clock.tick(60) / 1000.0  # 60 FPS
        elapsed_time = time.time() - params.start_time
        
        # 事件处理
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_SPACE:
                    manual_fast_phase = True
                    print("手动触发快相")
                
                # 刺激类型切换
                elif event.key == pygame.K_1:
                    params.current_stimulus_type = 'square_wave'
                    print("切换到: 方波刺激")
                elif event.key == pygame.K_2:
                    params.current_stimulus_type = 'sinusoidal'
                    print("切换到: 正弦波刺激")
                elif event.key == pygame.K_3:
                    params.current_stimulus_type = 'random_dots'
                    print("切换到: 随机点刺激")
                elif event.key == pygame.K_4:
                    params.current_stimulus_type = 'checkerboard'
                    print("切换到: 棋盘格刺激")
                
                # 参数调节
                elif event.key == pygame.K_LEFT:
                    if params.current_sf_index > 0:
                        params.current_sf_index -= 1
                        sf = params.spatial_frequencies[params.current_sf_index]
                        print(f"空间频率: {sf:.2f} c/deg")
                elif event.key == pygame.K_RIGHT:
                    if params.current_sf_index < len(params.spatial_frequencies) - 1:
                        params.current_sf_index += 1
                        sf = params.spatial_frequencies[params.current_sf_index]
                        print(f"空间频率: {sf:.2f} c/deg")
                
                elif event.key == pygame.K_UP:
                    if params.current_velocity_index < len(params.velocity_levels) - 1:
                        params.current_velocity_index += 1
                        vel = params.velocity_levels[params.current_velocity_index]
                        print(f"角速度: {vel}°/s")
                elif event.key == pygame.K_DOWN:
                    if params.current_velocity_index > 0:
                        params.current_velocity_index -= 1
                        vel = params.velocity_levels[params.current_velocity_index]
                        print(f"角速度: {vel}°/s")
                
                elif event.key == pygame.K_LEFTBRACKET:
                    if params.current_contrast_index > 0:
                        params.current_contrast_index -= 1
                        contrast = params.contrast_levels[params.current_contrast_index]
                        print(f"对比度: {contrast*100:.0f}%")
                elif event.key == pygame.K_RIGHTBRACKET:
                    if params.current_contrast_index < len(params.contrast_levels) - 1:
                        params.current_contrast_index += 1
                        contrast = params.contrast_levels[params.current_contrast_index]
                        print(f"对比度: {contrast*100:.0f}%")
                
                # 模式切换
                elif event.key == pygame.K_a:
                    params.auto_sequence_mode = not params.auto_sequence_mode
                    mode = "自动序列" if params.auto_sequence_mode else "手动"
                    print(f"模式: {mode}")
                elif event.key == pygame.K_t:
                    params.threshold_mode = not params.threshold_mode
                    mode = "阈值检测" if params.threshold_mode else "标准测试"
                    print(f"切换到: {mode}")
                elif event.key == pygame.K_r:
                    if not params.auto_sequence_mode:
                        params.current_direction_index = (params.current_direction_index + 1) % len(params.direction_sequence)
                        direction_name = params.get_current_direction_vector()[2]
                        print(f"手动切换方向: {direction_name}")
                elif event.key == pygame.K_f:
                    fullscreen = not fullscreen
                    if fullscreen:
                        screen = pygame.display.set_mode((width, height), pygame.FULLSCREEN)
                    else:
                        screen = pygame.display.set_mode((width, height))
        
        # 更新刺激参数
        params.update_adaptation(elapsed_time)
        params.update_automatic_sequence(dt)
        params.update_nystagmus_rhythm(dt)
        
        # 处理手动快相触发
        if manual_fast_phase:
            params.current_phase = "fast"
            params.phase_timer = 0
            manual_fast_phase = False
        
        # 更新位置偏移
        current_speed = params.get_current_speed()
        direction = params.get_current_direction_vector()
        
        if direction[0] != 'circular':
            dx, dy = direction[0], direction[1]
            offset += current_speed * dt
        else:
            # 圆形旋转
            offset += current_speed * direction[1] * dt * 0.1
        
        # 渲染刺激
        screen.fill((128, 128, 128))  # 中性灰背景
        
        # 选择渲染方法
        if params.current_stimulus_type == 'square_wave':
            renderer.render_square_wave(screen, params, offset)
        elif params.current_stimulus_type == 'sinusoidal':
            renderer.render_sinusoidal(screen, params, offset)
        elif params.current_stimulus_type == 'random_dots':
            renderer.render_random_dots(screen, params, offset)
        elif params.current_stimulus_type == 'checkerboard':
            renderer.render_checkerboard(screen, params, offset)
        
        # 绘制中心注视点
        center_x, center_y = width // 2, height // 2
        pygame.draw.circle(screen, (255, 0, 0), (center_x, center_y), 3)
        pygame.draw.line(screen, (255, 0, 0), (center_x - 8, center_y), (center_x + 8, center_y), 2)
        pygame.draw.line(screen, (255, 0, 0), (center_x, center_y - 8), (center_x, center_y + 8), 2)
        
        pygame.display.flip()
    
    pygame.quit()
    
    # 输出测试总结
    total_time = time.time() - params.start_time
    print("\n" + "="*60)
    print("🏁 增强版眼震刺激测试完成!")
    print("="*60)
    print(f"📊 测试统计:")
    print(f"   • 总运行时间: {total_time:.1f} 秒")
    print(f"   • 眼震总周期数: {params.total_cycles}")
    print(f"   • 平均周期频率: {params.total_cycles/total_time:.2f} Hz")
    print(f"   • 最终适应因子: {params.adaptation_factor:.2f}")
    
    print(f"\n📋 最终参数:")
    print(f"   • 空间频率: {params.spatial_frequencies[params.current_sf_index]:.2f} c/deg")
    print(f"   • 角速度: {params.velocity_levels[params.current_velocity_index]}°/s")
    print(f"   • 对比度: {params.contrast_levels[params.current_contrast_index]*100:.0f}%")
    print(f"   • 刺激类型: {params.current_stimulus_type}")
    
    return True

# 主程序入口
if __name__ == "__main__":
    print("🔬 增强版眼震刺激系统 v2.0 (无文本版)")
    print("="*50)
    
    try:
        enhanced_nystagmus_test()
    except KeyboardInterrupt:
        print("\n程序已取消")
    except Exception as e:
        print(f"\n程序错误: {e}")
        import traceback
        traceback.print_exc()