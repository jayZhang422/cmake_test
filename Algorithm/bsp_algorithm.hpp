#pragma once

#include "arm_math_types.h"
#include "dsp/complex_math_functions.h"
#include "dsp/transform_functions.h"
#include <arm_math.h>
#include <cassert>
#include <cmath>


#define BSP_ALG_SIN 0
#define BSP_ALG_COS PI/2
namespace EIS {



template<uint16_t FFT_SIZE>

class StaticFFT
{
    public:
        StaticFFT() {
        
       arm_status status = arm_rfft_fast_init_f32(&fft_handler, FFT_SIZE);

       if(status != ARM_MATH_SUCCESS)
       {
            assert(0);

       }
    }

    void Execute(float *input_data,float *output_mag)
    {
        arm_rfft_fast_f32(&fft_handler, input_data, _buffer, 0);

        arm_cmplx_mag_f32(_buffer,output_mag, FFT_SIZE/2) ;
    }
     

    private:

    arm_rfft_fast_instance_f32 fft_handler ;


    alignas(4) float _buffer[FFT_SIZE];
};

// ==========================================
// 1. 信号发生器 (用于生成 Sin/Cos 参考表)
// ==========================================
class EisRefGenerator {
public:
    // 生成参考波形
    // buffer: 存储数组
    // size: 数组长度
    // frequency: 目标频率 (Hz)
    // sample_rate: ADC采样率 (Hz)
    // phase_offset:BSP_ALG_SIN   BSP_ALG_COS (0=Sin, PI/2=Cos)
    static void Generate(float* buffer, uint32_t size, float frequency, float sample_rate, float phase_offset) {
        float32_t phase_inc = 2.0f * PI * frequency / sample_rate;
        float32_t current_phase = phase_offset;

        for (uint32_t i = 0; i < size; i++) {
            buffer[i] = arm_sin_f32(current_phase); // CMSIS-DSP 快速正弦
           //相位累加生成离散点
            current_phase += phase_inc;
            
            // 防止相位溢出，保持在 0-2PI 
            if (current_phase >= 2.0f * PI) {
                current_phase -= 2.0f * PI;
            }
        }
    }
};

// ==========================================
// 2. 核心锁相放大器 
// ==========================================
//微弱信号放大
class DigitalLockIn {
public:
    // 计算实部或虚部
    // signal: ADC采集的原始数据
    // reference: 标准正弦或余弦波
    // length: 数据长度
    // 返回值: 该分量的幅值 (电压或电流)
    static float Demodulate(const float* signal, const float* reference, uint32_t length) {
        float32_t dot_product_result;
        
       
        // 相当于：sum += signal[i] * reference[i]
        arm_dot_prod_f32(signal, reference, length, &dot_product_result);

        // 【数学修正】
        // 点乘结果是累加和，长度越长数值越大。
        // 对于正弦波相关运算，幅值 = (2 * Sum) / N
        return (2.0f * dot_product_result) / (float)length;
    }
};

// ==========================================
// 3. 阻抗解算器 (复数运算)
// ==========================================
struct ImpedanceResult {
    float R_real;     // 实部 (Z') -> 对应欧姆内阻
    float R_imag;     // 虚部 (Z'') -> 对应容抗/感抗
    float Magnitude;  // 总阻抗模值 |Z|
    float Phase_deg;  // 相位角 (度)
};

class ImpedanceSolver {
public:
    /**
     * @brief 计算阻抗 Z = V / I
     * @param Vr 电压实部
     * @param Vi 电压虚部
     * @param Ir 电流实部
     * @param Ii 电流虚部
     * @param p_out [输出] 结果结构体指针 (传入 &variable)
     */
    static void Calculate(float Vr, float Vi, float Ir, float Ii, ImpedanceResult* p_out) {
        // 0. 安全检查：防止传入空指针导致死机
        if (p_out == nullptr) {
            return;
        }

        // 1. 计算分母 (电流模长的平方: |I|^2)
        float denominator = Ir * Ir + Ii * Ii;

        // 2. 防止除以0的保护
        // 如果电流太小，除法会溢出，直接返回 0
        if (denominator < 1e-9f) {
            p_out->R_real = 0.0f;
            p_out->R_imag = 0.0f;
            p_out->Magnitude = 0.0f;
            p_out->Phase_deg = 0.0f;
            return;
        }

        // 3. 复数除法 Z = V / I
        // 公式推导: (Vr+jVi) / (Ir+jIi) * (Ir-jIi)/(Ir-jIi)
        
        // 实部: (Vr*Ir + Vi*Ii) / Denom
        p_out->R_real = (Vr * Ir + Vi * Ii) / denominator;
        
        // 虚部: (Vi*Ir - Vr*Ii) / Denom
        p_out->R_imag = (Vi * Ir - Vr * Ii) / denominator;

        // 4. 计算模值 |Z| = sqrt(Real^2 + Imag^2)
        // 使用 DSP 库的硬件开方函数，速度更快
        arm_sqrt_f32(p_out->R_real * p_out->R_real + p_out->R_imag * p_out->R_imag, &p_out->Magnitude);

        // 5. 计算相位 (Atan2 返回弧度，转角度)
        // atan2f(y, x) -> atan2f(Imag, Real)
        p_out->Phase_deg = atan2f(p_out->R_imag, p_out->R_real) * (180.0f / PI);
    }
};

// 专门用来装载 DAC 配置的结构体
struct DacWaveConfig {
    float offset;    // 换算后的 DAC 偏置值
    float amplitude; // 换算后的 DAC 振幅值
};

class DacConfigHelper {
public:
    /**
     * @brief 人性化的 DAC 波形参数设置辅助函数
     * @param center_voltage_mv 中心偏置电压 (单位: 毫伏 mV)。建议设为 1650 (即 1.65V)
     * @param peak_amplitude_mv 正弦波的峰值振幅 (单位: 毫伏 mV)。比如 1000mV 代表波形上下各摆动 1V (峰峰值 2V)
     * @param vref_mv           系统 DAC 的参考硬件电压 (通常为 3300 mV)
     * @return DacWaveConfig    算好的可以直接塞给 DAC_Waveform 的参数
     */
    static DacWaveConfig Calculate(float center_voltage_mv = 1650.0f, 
                                   float peak_amplitude_mv = 1000.0f, 
                                   float vref_mv = 3300.0f) 
    {
        DacWaveConfig config;
        // 12位 DAC 的最大值是 4095.0f
        const float max_dac_val = 4095.0f;
        const float safe_center_mv = 1650.0f;
        const float safe_amp_mv = 0.0f;
        const float safe_vref_mv = 3300.0f;

        // 输入兜底：阻断 NaN/Inf 和非法 Vref，避免异常值传播到 DAC。
        if (!std::isfinite(center_voltage_mv)) {
            center_voltage_mv = safe_center_mv;
        }
        if (!std::isfinite(peak_amplitude_mv)) {
            peak_amplitude_mv = safe_amp_mv;
        }
        if (!std::isfinite(vref_mv) || (vref_mv <= 0.0f)) {
            vref_mv = safe_vref_mv;
        }

        // 1. 将人类易读的毫伏(mV)换算为机器所需的 DAC 原始数值
        float offset_dac = (center_voltage_mv / vref_mv) * max_dac_val;
        float amplitude_dac = (peak_amplitude_mv / vref_mv) * max_dac_val;

        // 二次兜底：处理极端输入导致的中间非有限值。
        if (!std::isfinite(offset_dac)) {
            offset_dac = (safe_center_mv / safe_vref_mv) * max_dac_val;
        }
        if (!std::isfinite(amplitude_dac)) {
            amplitude_dac = 0.0f;
        }

        // 2. 极值与乱填保护 (防止瞎传负数或超限)
        if (offset_dac > max_dac_val) offset_dac = max_dac_val;
        if (offset_dac < 0.0f) offset_dac = 0.0f;
        if (amplitude_dac < 0.0f) amplitude_dac = 0.0f;

        // 3. 终极防削峰保护：如果请求的振幅过大，导致波峰顶破 3.3V 或波谷击穿 0V，则自动压缩振幅
        // 保护上限：偏置 + 振幅 不能超过 4095
        if (offset_dac + amplitude_dac > max_dac_val) {
            amplitude_dac = max_dac_val - offset_dac; 
        }
        // 保护下限：偏置 - 振幅 不能小于 0
        if (offset_dac - amplitude_dac < 0.0f) {
            amplitude_dac = offset_dac; 
        }

        config.offset = offset_dac;
        config.amplitude = amplitude_dac;

        return config;
    }
};

}

//C++ 当作 “带有命名空间、模板和权限管理的更好用的 C 语言” 在用。
// 这种写法在底层保留了 C 语言级别的指针操作和 ARM DSP 硬件加速
//同时上层也更容易调用api