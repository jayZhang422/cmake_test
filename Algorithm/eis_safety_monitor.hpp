#pragma once

#include <stdint.h>
#include <stdbool.h>


#include "bsp_algorithm.hpp"     // 包含 ImpedanceResult
#include "dsp_preprocess.hpp"    // 包含 PreprocessResult

namespace EIS {

// 安全状态枚举
typedef enum {
    SAFETY_OK = 0,
    SAFETY_ERR_OVER_VOLTAGE,      // 电池过压 (或接反)
    SAFETY_ERR_OVER_CURRENT,      // 激励短路或电流失控
    SAFETY_ERR_PROBE_DISCONNECT,  // 探针脱落 (接触不良)
    SAFETY_ERR_MATH_NAN           // 算法崩溃 (出现除以0或溢出)
} SafetyStatus_e;

class SafetyMonitor {
public:
    struct ThresholdConfig {
        float MaxBatteryVoltage_V;  // 允许的最大电池静置电压 (如 4.5V)
        float MaxAcCurrent_A;       // 允许的最大交流激励电流有效值 (如 1.0A)
        float MaxImpedance_Ohm;     // 判定为脱落的极值阻抗 (如 10.0欧姆)
    };

    /**
     * @brief 初始化安全阈值
     */
    static void Init(const ThresholdConfig& config) {
        _cfg = config;
    }

    /**
     * @brief 算子 1：时域快速检查 (在模块 1 预处理之后立刻调用)
     * @param dc_voltage  当前通道测得的直流偏置电压
     * @param ac_current  当前电流通道测得的有效值
     * @return SafetyStatus_e 
     */
    static SafetyStatus_e CheckTimeDomain(float dc_voltage, float ac_current) {
        // 1. 检查过压或严重的反接 (假设硬件不能承受负压过大)
        // 绝对值判断，防止电池接反时烧坏 ADC 缓冲级
        float abs_v = (dc_voltage < 0.0f) ? -dc_voltage : dc_voltage;
        if (abs_v > _cfg.MaxBatteryVoltage_V) {
            return SAFETY_ERR_OVER_VOLTAGE;
        }

        // 2. 检查电流是否失控 (过流保护)
        if (ac_current > _cfg.MaxAcCurrent_A) {
            return SAFETY_ERR_OVER_CURRENT;
        }

        return SAFETY_OK;
    }

    /**
     * @brief 算子 2：频域脱落检查 (在模块 0 阻抗解算完成后调用)
     * @param p_z_result  解算出的复数阻抗
     * @return SafetyStatus_e 
     */
    static SafetyStatus_e CheckImpedance(const ImpedanceResult* p_z_result) {
        if (!p_z_result) return SAFETY_ERR_MATH_NAN;

        // 1. 检查是否存在非数字(NaN)或无穷大(Inf)，防止后续运算让整个 RTOS 任务死机
        // IEEE754 浮点数特性：如果不是数字，它自己不等于自己
        if (p_z_result->Magnitude != p_z_result->Magnitude || 
            p_z_result->R_real != p_z_result->R_real) {
            return SAFETY_ERR_MATH_NAN;
        }

        // 2. 探针脱落 / 开路检测
        // 如果阻抗模值远远超出电池的物理极限，说明夹子掉了
        if (p_z_result->Magnitude > _cfg.MaxImpedance_Ohm) {
            return SAFETY_ERR_PROBE_DISCONNECT;
        }

        return SAFETY_OK;
    }

private:
    static ThresholdConfig _cfg;
};

// 静态成员变量定义 (别忘了在对应的 .cpp 文件中分配它)
// SafetyMonitor::ThresholdConfig SafetyMonitor::_cfg = {5.0f, 1.0f, 10.0f};

} // namespace EIS