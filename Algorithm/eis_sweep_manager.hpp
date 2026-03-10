#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h> // 需要使用 powf 进行初始化运算

namespace EIS {

// ==========================================
// 算子 1：对数扫频序列生成器 (Log-Sweep Generator)
// ==========================================
class SweepGenerator {
public:
    /**
     * @brief 初始化扫频参数 (在扫频开始前调用一次)
     * @param f_start 起始频率 (Hz)
     * @param f_end   截止频率 (Hz)
     * @param steps   总频点数
     */
    void Init(float f_start, float f_end, uint16_t steps) {
        if (steps == 0 || f_start <= 0.0f || f_end <= 0.0f) return;

        _current_freq = f_start;
        _total_steps = steps;
        _current_step = 0;

        // 核心优化：预计算对数步进乘数 K
        // f_n = f_start * K^n
        // 这样在每次变频时，只需要做一次硬件浮点乘法 (耗时 < 100ns)
        if (steps > 1) {
            _multiplier = powf(f_end / f_start, 1.0f / (float)(steps - 1));
        } else {
            _multiplier = 1.0f;
        }
    }

    /**
     * @brief 获取下一个频点
     * @param p_freq_out [输出] 下一个目标频率
     * @return bool 是否扫频结束 (true=还有数据, false=已扫完)
     */
    bool GetNext(float* p_freq_out) {
        if (!p_freq_out || _current_step >= _total_steps) {
            return false; 
        }

        *p_freq_out = _current_freq;
        
        // 极速递推下一个频率
        _current_freq *= _multiplier;
        _current_step++;
        
        return true;
    }

    // 获取扫频进度百分比 (用于 UI 更新)
    uint8_t GetProgressPct() const {
        if (_total_steps == 0) return 0;
        return (uint8_t)((_current_step * 100) / _total_steps);
    }

private:
    float    _current_freq = 0.0f;
    float    _multiplier   = 1.0f;
    uint16_t _total_steps  = 0;
    uint16_t _current_step = 0;
};

// ==========================================
// 算子 2：整周期采样优化器 (Integer Cycle Optimizer)
// ==========================================
class IntegerCycleOptimizer {
public:
    /**
     * @brief 计算最优采样点数，防止频谱泄露
     * @param target_freq   [输入] 期望的信号频率 (Hz)
     * @param sample_rate   [输入] 系统的 ADC 采样率 (Hz)
     * @param max_buffer_sz [输入] 你的系统最大可用 DMA 缓冲长度 (例如 2048)
     * @param p_optimal_n   [输出] 建议的 DMA 采样点数
     * @param p_actual_freq [输出] 修正后的极精确激励频率
     */
    static void Optimize(float target_freq, float sample_rate, uint32_t max_buffer_sz, 
                         uint32_t* p_optimal_n, float* p_actual_freq) 
    {
        if (target_freq <= 0.0f || sample_rate <= 0.0f || max_buffer_sz == 0) return;

        // 1. 计算在最大缓冲区内，理论上能装下多少个完整的信号周期
        float theoretical_cycles = (float)max_buffer_sz * target_freq / sample_rate;
        
        // 2. 向下取整，确保装下的是绝对整数个周期 (核心约束)
        uint32_t k_cycles = (uint32_t)theoretical_cycles;
        
        // 低频保护：如果 1 个周期都装不下，必须强制采 1 个周期（此时可能会超 max_buffer_sz，由外部去降低 fs）
        if (k_cycles == 0) k_cycles = 1; 

        // 3. 反推实现 k_cycles 个整数周期所需要的实际采样点数 N
        *p_optimal_n = (uint32_t)roundf((float)k_cycles * sample_rate / target_freq);
        
        // 限幅保护，防止越界
        if (*p_optimal_n > max_buffer_sz) {
            *p_optimal_n = max_buffer_sz;
        }

        // 4. 数学修正：为了完美契合点数 N，DAC 实际发出的物理频率应做微调
        // f_actual = k * fs / N
        *p_actual_freq = (float)k_cycles * sample_rate / (float)(*p_optimal_n);
    }
};

// ==========================================
// 算子 3：稳态建立时间计算器 (Settling Time Calculator)
// ==========================================
class SettlingTimeCalculator {
public:
    /**
     * @brief 计算变频后需要盲等待的毫秒数
     * @param freq_hz      当前频率
     * @param wait_cycles  期望等待的周期数 (例如等待 3 个周期让滤波器稳定)
     * @param min_wait_ms  硬件绝对最小等待时间 (例如 PGA 切换的毛刺恢复需 5ms)
     * @return uint32_t    应传入 RTOS_Delay() 的毫秒数
     */
    static uint32_t CalculateDelayMs(float freq_hz, float wait_cycles = 3.0f, uint32_t min_wait_ms = 5) {
        if (freq_hz <= 0.0f) return min_wait_ms;

        // T = 1 / f
        // 耗时(ms) = 周期数 * 1000 / 频率
        uint32_t theoretical_wait_ms = (uint32_t)roundf((wait_cycles * 1000.0f) / freq_hz);

        // 返回理论时间和物理底线时间中更大的那个
        return (theoretical_wait_ms > min_wait_ms) ? theoretical_wait_ms : min_wait_ms;
    }
};

} // namespace EIS