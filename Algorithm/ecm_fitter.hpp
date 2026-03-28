#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "bsp_algorithm.hpp" 


/*
 * =====================================================================================
 * @brief 算法原理：基于非线性最小二乘法 (NLLS) 的 Randles 等效电路拟合
 * * 1. 物理模型：将电池抽象为基础 Randles 电路 (Rs 串联 [Rct 并联 Cdl])。
 * 理论复阻抗公式：$$Z(\omega) = R_s + \frac{R_{ct}}{1 + j\omega R_{ct} C_{dl}}$$
 * * 2. 核心思想：实际扫频得到的阻抗数据点 (31个频点) 远多于待求参数 (Rs, Rct, Cdl 仅3个)，
 * 这是一个典型的“超定方程组”问题，无法直接代数求解。
 * * 3. 求解策略：本算法采用【带自适应衰减的梯度下降法】进行非线性最小二乘寻优。
 * 通过不断微调 Rs、Rct、Cdl 的值，使得所有频点下【模型预测阻抗】与【实际测量阻抗】
 * 在复平面上的欧氏距离平方和（MSE均方误差）降到最低，从而提取出最贴近真实物理状态的参数。
 * =====================================================================================
 */

namespace EIS {

struct EcmParams {
    float Rs;   
    float Rct;  
    float Cdl;  
    float MSE;  
};

class EcmFitter {
public:
    static bool FitRandlesModel(const float* freqs_hz, const ImpedanceResult* z_data, uint16_t data_len, uint16_t max_iters, EcmParams* p_out) {
        if (!freqs_hz || !z_data || !p_out || data_len < 3) return false;

        EstimateInitialGuess(freqs_hz, z_data, data_len, p_out);

        //基于初始猜测值动态分配学习率，解决 Rs 和 Cdl 量级相差巨大的“量纲灾难”
        float lr_Rs  = p_out->Rs  * 0.1f;
        float lr_Rct = p_out->Rct * 0.1f;
        float lr_Cdl = p_out->Cdl * 0.1f;

        float current_mse = CalculateMSE(freqs_hz, z_data, data_len, p_out);

        for (uint16_t iter = 0; iter < max_iters; iter++) {
            //  使用中心差分法求导，精度远高于前向差分
            float grad_Rs  = ComputeNumericalGradient(freqs_hz, z_data, data_len, *p_out, 0);
            float grad_Rct = ComputeNumericalGradient(freqs_hz, z_data, data_len, *p_out, 1);
            float grad_Cdl = ComputeNumericalGradient(freqs_hz, z_data, data_len, *p_out, 2);

            EcmParams next_params = *p_out;
            next_params.Rs  -= lr_Rs  * grad_Rs;
            next_params.Rct -= lr_Rct * grad_Rct;
            next_params.Cdl -= lr_Cdl * grad_Cdl;

            // 物理边界硬约束
            if (next_params.Rs < 1e-4f)  next_params.Rs = 1e-4f;
            if (next_params.Rct < 1e-4f) next_params.Rct = 1e-4f;
            if (next_params.Cdl < 1e-6f) next_params.Cdl = 1e-6f;

            float next_mse = CalculateMSE(freqs_hz, z_data, data_len, &next_params);
            
            if (next_mse < current_mse) {
                *p_out = next_params;
                
                // 相对误差收敛判定
                if (fabsf(current_mse - next_mse) < (1e-5f * current_mse)) {
                    p_out->MSE = next_mse;
                    return true; 
                }
                current_mse = next_mse;
                
                // 连续成功时，适当放大步长加速收敛
                lr_Rs *= 1.1f; lr_Rct *= 1.1f; lr_Cdl *= 1.1f; 
            } else {
                // 发生震荡，衰减学习率重试
                lr_Rs *= 0.5f; lr_Rct *= 0.5f; lr_Cdl *= 0.5f;
            }
        }

        p_out->MSE = current_mse;
        return true; 
    }

private:
    static void EstimateInitialGuess(const float* freqs_hz, const ImpedanceResult* z_data, uint16_t data_len, EcmParams* p_out) {
        // Rs: 高频区实轴截距
        p_out->Rs = z_data[data_len - 1].R_real; 

        // Rct: 中低频半圆跨度
        float max_real = z_data[0].R_real;
        for(uint16_t i=0; i<data_len; i++) {
            if(z_data[i].R_real > max_real) max_real = z_data[i].R_real;
        }
        p_out->Rct = max_real - p_out->Rs;
        if (p_out->Rct <= 1e-3f) p_out->Rct = 1e-3f; 

        // Cdl: 寻找虚部极小值 (容抗峰值) 计算电容
        float min_imag = 0.0f; 
        float f_at_min_imag = freqs_hz[0];
        for(uint16_t i=0; i<data_len; i++) {
            if(z_data[i].R_imag < min_imag) { 
                min_imag = z_data[i].R_imag;
                f_at_min_imag = freqs_hz[i];
            }
        }
        
        if (f_at_min_imag > 0.0f) {
            p_out->Cdl = 1.0f / (2.0f * PI * f_at_min_imag * p_out->Rct);
        } else {
            p_out->Cdl = 1.0f; 
        }
    }

    static float CalculateMSE(const float* freqs_hz, const ImpedanceResult* z_data, uint16_t data_len, const EcmParams* p) {
        float sum_sq_err = 0.0f;
        for (uint16_t i = 0; i < data_len; i++) {
            // 预计算公共项，减少 FPU 乘法周期
            float w = 2.0f * PI * freqs_hz[i];
            float w_RC = w * p->Rct * p->Cdl;
            float den = 1.0f + w_RC * w_RC;
            
            float z_real_pred = p->Rs + (p->Rct / den);
            float z_imag_pred = - (w * p->Rct * p->Rct * p->Cdl) / den;

            float err_r = z_real_pred - z_data[i].R_real;
            float err_i = z_imag_pred - z_data[i].R_imag;
            
            sum_sq_err += (err_r * err_r + err_i * err_i);
        }
        return sum_sq_err / (float)data_len;
    }

    static float ComputeNumericalGradient(const float* freqs_hz, const ImpedanceResult* z_data, uint16_t data_len, EcmParams p, uint8_t param_idx) {
        // 相对摄动量。防止当参数极大或极小时，固定的 epsilon 被 float32 舍入误差吞没
        float val = 0.0f;
        if (param_idx == 0) val = p.Rs;
        else if (param_idx == 1) val = p.Rct;
        else if (param_idx == 2) val = p.Cdl;

        float epsilon = fmaxf(val * 1e-4f, 1e-7f); 

        // 中心差分法: f(x+h) - f(x-h) / 2h
        EcmParams p_plus = p;
        EcmParams p_minus = p;

        if (param_idx == 0)      { p_plus.Rs += epsilon;  p_minus.Rs -= epsilon; }
        else if (param_idx == 1) { p_plus.Rct += epsilon; p_minus.Rct -= epsilon; }
        else if (param_idx == 2) { p_plus.Cdl += epsilon; p_minus.Cdl -= epsilon; }

        float mse_plus = CalculateMSE(freqs_hz, z_data, data_len, &p_plus);
        float mse_minus = CalculateMSE(freqs_hz, z_data, data_len, &p_minus);

        return (mse_plus - mse_minus) / (2.0f * epsilon);
    }
};

} // namespace EIS