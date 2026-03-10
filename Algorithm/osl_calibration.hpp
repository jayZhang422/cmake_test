#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

namespace EIS {

// 校准点数据结构
struct CalibPoint {
    float Freq_Hz;       // 校准频率
    float Short_Real;    // 短路校准实部 (欧姆)
    float Short_Imag;    // 短路校准虚部 (欧姆)
    float Open_Real;     // 开路校准实部 (欧姆)
    float Open_Imag;     // 开路校准虚部 (欧姆)
};

class OslCalibration {
public:
    /**
     * @brief 初始化校准矩阵
     * @param p_calib_table  事先在 Flash 中存好的校准表指针
     * @param table_size     校准表的频点数量
     */
    void Init(const CalibPoint* p_calib_table, uint16_t table_size) {
        _p_table = p_calib_table;
        _table_size = table_size;
    }

    /**
     * @brief 应用 OSL 补偿 (核心算子)
     * @param target_freq  当前实际测量的频率
     * @param p_real       [输入/输出] 测量得到的实部 -> 补偿后的实部
     * @param p_imag       [输入/输出] 测量得到的虚部 -> 补偿后的虚部
     */
    void ApplyCompensation(float target_freq, float* p_real, float* p_imag) {
        if (!_p_table || _table_size == 0 || !p_real || !p_imag) return;

        CalibPoint interp_calib;
        // 1. 查找并插值计算出当前频率下的开路和短路寄生阻抗
        Interpolate(target_freq, &interp_calib);

        // 2. 执行短路补偿: Z_diff = Z_meas - Z_short
        float diff_R = *p_real - interp_calib.Short_Real;
        float diff_X = *p_imag - interp_calib.Short_Imag;

        // 3. 执行开路补偿: Z_actual = Z_diff / (1 - Z_diff / Z_open)
        // 电池通常阻抗极低，如果开路没校准好或者为0，只做短路补偿
        float open_mag_sq = interp_calib.Open_Real * interp_calib.Open_Real + 
                            interp_calib.Open_Imag * interp_calib.Open_Imag;
                            
        if (open_mag_sq > 1e-6f) { // 开路阻抗有效且不为0
            // 3.1 计算 Z_diff / Z_open (复数除法)
            // (A+jB)/(C+jD) = (AC+BD)/(C^2+D^2) + j(BC-AD)/(C^2+D^2)
            float term_R = (diff_R * interp_calib.Open_Real + diff_X * interp_calib.Open_Imag) / open_mag_sq;
            float term_X = (diff_X * interp_calib.Open_Real - diff_R * interp_calib.Open_Imag) / open_mag_sq;

            // 3.2 计算分母 1 - (Z_diff / Z_open)
            float den_R = 1.0f - term_R;
            float den_X = -term_X;

            // 3.3 计算最终的除法: Z_actual = Z_diff / 分母
            float den_mag_sq = den_R * den_R + den_X * den_X;
            if (den_mag_sq > 1e-9f) { // 防止除 0 崩溃
                *p_real = (diff_R * den_R + diff_X * den_X) / den_mag_sq;
                *p_imag = (diff_X * den_R - diff_R * den_X) / den_mag_sq;
                return;
            }
        }

        // 如果开路未校准或异常，回退到仅短路补偿
        *p_real = diff_R;
        *p_imag = diff_X;
    }

private:
    const CalibPoint* _p_table = nullptr;
    uint16_t          _table_size = 0;

    // 一维线性插值算法
    void Interpolate(float freq, CalibPoint* out) {
        // 边界处理：低于最小频点或只有一个频点
        if (freq <= _p_table[0].Freq_Hz || _table_size == 1) {
            *out = _p_table[0];
            return;
        }
        // 边界处理：高于最大频点
        if (freq >= _p_table[_table_size - 1].Freq_Hz) {
            *out = _p_table[_table_size - 1];
            return;
        }

        // 二分法或顺序查找区间 (因为频点一般不超过 100 个，顺序查找配合缓存也很快)
        for (uint16_t i = 0; i < _table_size - 1; i++) {
            if (freq >= _p_table[i].Freq_Hz && freq <= _p_table[i+1].Freq_Hz) {
                float f0 = _p_table[i].Freq_Hz;
                float f1 = _p_table[i+1].Freq_Hz;
                float ratio = (freq - f0) / (f1 - f0); // 插值权重

                out->Freq_Hz    = freq;
                out->Short_Real = _p_table[i].Short_Real + ratio * (_p_table[i+1].Short_Real - _p_table[i].Short_Real);
                out->Short_Imag = _p_table[i].Short_Imag + ratio * (_p_table[i+1].Short_Imag - _p_table[i].Short_Imag);
                out->Open_Real  = _p_table[i].Open_Real  + ratio * (_p_table[i+1].Open_Real  - _p_table[i].Open_Real);
                out->Open_Imag  = _p_table[i].Open_Imag  + ratio * (_p_table[i+1].Open_Imag  - _p_table[i].Open_Imag);
                return;
            }
        }
    }
};

} // namespace EIS