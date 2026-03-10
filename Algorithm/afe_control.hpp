#pragma once

#include <stdint.h>
#include <stdbool.h>

// 假设这是从你的 dsp_preprocess.hpp 包含过来的
// #include "dsp_preprocess.hpp" 
namespace EIS {

// 声明外部传入的质量枚举 (与模块一解耦对应)
// 若已在 dsp_preprocess.hpp 中定义，可直接引用
#ifndef EIS_SIGNAL_QUALITY_DEFINED
#define EIS_SIGNAL_QUALITY_DEFINED
typedef enum {
    SIGNAL_OK = 0,
    SIGNAL_CLIPPED,      // 信号削顶或触底
    SIGNAL_TOO_WEAK,     // 信号太弱
    SIGNAL_INVALID_PARAM 
} SignalQuality_e;
#endif

// AGC 处理结果状态
typedef enum {
    AGC_STATE_STABLE = 0,     // 稳定态：增益合适，且硬件已稳定，允许计算阻抗
    AGC_STATE_BLANKING,       // 屏蔽态：运放正在建立稳态，必须丢弃当前数据块
    AGC_STATE_JUST_CHANGED,   // 触发态：刚刚改变了增益要求，底层需立即操作 GPIO 更新 PGA
    AGC_STATE_LIMIT_WARNING   // 极限态：信号过载或过弱，但硬件增益档位已到头，无法再调
} AgcState_e;


// ==========================================
// 算子 1：AGC 状态机 (防震荡 & 屏蔽期管理)
// ==========================================
class AgcStateMachine {
public:
    /**
     * @brief 初始化 AGC 状态机
     * @param min_idx         最小增益档位索引 (如 0 代表 Gain=1)
     * @param max_idx         最大增益档位索引 (如 7 代表 Gain=128)
     * @param start_idx       初始增益档位索引
     * @param blanking_frames 每次切换增益后，需要强制丢弃的 DMA 数据块数量
     */
    void Init(uint8_t min_idx, uint8_t max_idx, uint8_t start_idx, uint16_t blanking_frames) {
        _min_gain_idx = min_idx;
        _max_gain_idx = max_idx;
        
        // 范围安全约束
        _current_idx = (start_idx > max_idx) ? max_idx : start_idx;
        _current_idx = (_current_idx < min_idx) ? min_idx : _current_idx;
        
        _blanking_frames_cfg = blanking_frames;
        _blanking_counter = 0;
    }

    /**
     * @brief 驱动 AGC 状态机 (在每个 DMA 传输完成回调的任务中调用)
     * @param quality      [输入] 模块一算出的当前数据块质量
     * @param p_out_idx    [输出] 当前应该使用的增益档位索引
     * @return AgcState_e  当前系统状态 (指示上层是否可以解算阻抗)
     */
    AgcState_e Process(SignalQuality_e quality, uint8_t* p_out_idx) {
        if (!p_out_idx) return AGC_STATE_LIMIT_WARNING;
        
        // 1. 如果处于屏蔽期，无视任何信号输入 (因为此时 ADC 采样的是运放毛刺)
        if (_blanking_counter > 0) {
            _blanking_counter--;
            *p_out_idx = _current_idx;
            return AGC_STATE_BLANKING;
        }

        // 2. 状态判定与档位切换
        AgcState_e result_state = AGC_STATE_STABLE;

        switch (quality) {
            case SIGNAL_CLIPPED:
                // 削顶，必须降低增益 (减小索引)
                if (_current_idx > _min_gain_idx) {
                    _current_idx--;
                    _blanking_counter = _blanking_frames_cfg; // 重新触发屏蔽期
                    result_state = AGC_STATE_JUST_CHANGED;
                } else {
                    result_state = AGC_STATE_LIMIT_WARNING; // 已降到最低，依然削顶 (需降低激励电流)
                }
                break;

            case SIGNAL_TOO_WEAK:
                // 信号弱，提高增益 (增大索引)
                if (_current_idx < _max_gain_idx) {
                    _current_idx++;
                    _blanking_counter = _blanking_frames_cfg; // 重新触发屏蔽期
                    result_state = AGC_STATE_JUST_CHANGED;
                } else {
                    result_state = AGC_STATE_LIMIT_WARNING; // 已放到最大，依然微弱
                }
                break;

            case SIGNAL_OK:
            default:
                // 信号完美，保持现状
                result_state = AGC_STATE_STABLE;
                break;
        }

        *p_out_idx = _current_idx;
        return result_state;
    }

    // 强制复位状态 (例如探针脱落后重新开始时调用)
    void ResetState() {
        _blanking_counter = 0;
    }

private:
    uint8_t  _min_gain_idx;
    uint8_t  _max_gain_idx;
    uint8_t  _current_idx;
    uint16_t _blanking_frames_cfg;
    uint16_t _blanking_counter;
};


// ==========================================
// 算子 2：初始测量配置推断器 (根据电池容量)
// ==========================================
class ExcitationEstimator {
public:
    struct InitialConfig {
        float    SuggestedCurrentAmps; // 建议的交流激励电流 (安培)
        uint8_t  SuggestedGainIndex;   // 建议的初始 PGA 档位索引
    };

    /**
     * @brief 根据串口屏输入的电池容量，推断初始参数
     * @note  基于经验：容量越大，内阻越小，需要的激励电流越大，初始增益可能需要更大。
     */
    static InitialConfig Estimate(float battery_capacity_ah) {
        InitialConfig cfg;

        if (battery_capacity_ah < 5.0f) {
            // 小容量电池 (如 18650, 手机电池)
            // 内阻通常在 20mR ~ 100mR 级别
            cfg.SuggestedCurrentAmps = 0.05f; // 50mA 扰动
            cfg.SuggestedGainIndex = 1;       // 假设档位1对应较小增益(如x2)
        } 
        else if (battery_capacity_ah < 50.0f) {
            // 中等容量 (如 轻型电动车电池)
            // 内阻通常在 5mR ~ 20mR
            cfg.SuggestedCurrentAmps = 0.1f;  // 100mA 扰动
            cfg.SuggestedGainIndex = 2;       // 中等增益
        } 
        else {
            // 大容量储能电池/动力电池 (如 100Ah 磷酸铁锂)
            // 内阻极小，通常在 0.2mR ~ 1mR
            // 如果不加大电流，电压响应会被 ADC 噪声淹没
            cfg.SuggestedCurrentAmps = 0.5f;  // 500mA 扰动
            cfg.SuggestedGainIndex = 3;       // 假设档位3对应大增益(如x16或x32)
        }

        return cfg;
    }
};

} // namespace EIS