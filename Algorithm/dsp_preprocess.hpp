#pragma once

#include "arm_math_types.h"
#include "dsp/basic_math_functions.h"
#include "dsp/statistics_functions.h"
#include <arm_math.h>
#include <stdint.h>
#include <stdbool.h>

namespace EIS {

// 信号质量状态枚举
typedef enum {
    SIGNAL_OK = 0,
    SIGNAL_CLIPPED,      // 信号削顶或触底 (增益过大)
    SIGNAL_TOO_WEAK,     // 信号太弱 (需要提高增益)
    SIGNAL_INVALID_PARAM // 传入参数非法
} SignalQuality_e;

class DspPreprocess {
public:
    /**
     * @brief 算子 1：ADC 原始数据转换为物理电压，并进行硬件边界检测
     * @param p_raw_in   [输入] 12-bit ADC DMA 原始数据
     * @param p_volts_out[输出] 物理电压值缓冲
     * @param length     [输入] 数据长度
     * @param vref       [输入] ADC 参考电压 (如 3.3V)
     * @param pga_gain   [输入] 当前硬件 PGA 的放大倍数
     * @return bool      如果检测到削顶 (接近0或接近4095)，返回 true
     */
    static bool ConvertRawToVoltage(const uint16_t* p_raw_in, float32_t* p_volts_out, uint32_t length, float vref, float pga_gain) {
        if (!p_raw_in || !p_volts_out || length == 0 || pga_gain <= 0.0f) return false;

        bool is_clipped = false;
        // 映射系数：将 ADC 读数直接折算为前端电池接口处的真实电压
        const float32_t scale_factor = (vref / 4096.0f) / pga_gain;
        
        // 12-bit ADC 的安全物理边界 (留 5 个 LSB 裕量)
        const uint16_t CLIP_HIGH = 4090;
        const uint16_t CLIP_LOW  = 5;

        for (uint32_t i = 0; i < length; i++) {
            uint16_t raw = p_raw_in[i];
            if (raw >= CLIP_HIGH || raw <= CLIP_LOW) {
                is_clipped = true; // 仅置位标志，依然完成后续转换以供排查问题
            }
            p_volts_out[i] = (float32_t)raw * scale_factor;
        }
        return is_clipped;
    }

    /**
     * @brief 算子 2：零相移去直流 (Block DC Blocking)
     * @param p_data     [输入/输出] 需要去直流的浮点数组 (原地操作)
     * @param length     [输入] 数据长度
     * @param p_dc_bias  [输出] 提取出的直流偏置电压 (即电池当前的端电压)
     */
    static void RemoveDcOffset(float32_t* p_data, uint32_t length, float32_t* p_dc_bias) {
        if (!p_data || !p_dc_bias || length == 0) return;

        // 1. 利用 CMSIS-DSP 极速计算均值 (直流分量)
        arm_mean_f32(p_data, length, p_dc_bias);

        // 2. 利用 CMSIS-DSP 极速偏移: Data[i] = Data[i] - mean (变为纯交流分量)
        arm_offset_f32(p_data, -(*p_dc_bias), p_data, length);
    }

    /**
     * @brief 算子 3：计算交流信号的有效值 (RMS)
     * @param p_data     [输入] 已经去完直流的纯交流浮点数组
     * @param length     [输入] 数据长度
     * @return float32_t 交流有效值
     */
    static float32_t CalculateRMS(const float32_t* p_data, uint32_t length) {
        if (!p_data || length == 0) return 0.0f;

        float32_t rms_val = 0.0f;
        arm_rms_f32(p_data, length, &rms_val);
        return rms_val;
    }

    /**
     * @brief 算子 4：综合信号质量评估 (供 AGC 控制器调用)
     * @param is_clipped [输入] 算子1得到的削顶标志
     * @param ac_rms     [输入] 算子3得到的交流有效值
     * @param pga_gain   [输入] 当前的增益倍数
     * @return SignalQuality_e 状态指示
     */
    static SignalQuality_e EvaluateSignalQuality(bool is_clipped, float32_t ac_rms, float pga_gain) {
        if (pga_gain <= 0.0f) return SIGNAL_INVALID_PARAM;

        if (is_clipped) {
            return SIGNAL_CLIPPED; // 必须降低增益
        } 
        
        // 假设折算到电池端的交流有效值小于 5mV (0.005f)，认为信噪比极差
        // 这个阈值可根据实际 AFE 板子的底噪进行调整
        if (ac_rms < (0.005f / pga_gain)) {
            return SIGNAL_TOO_WEAK; // 需要提高增益
        }

        return SIGNAL_OK;
    }
};

} // namespace EIS