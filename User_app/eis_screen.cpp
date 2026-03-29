#include "eis_screen.hpp"
#include "tx_api.h"
#include "uart_screen.hpp"
#include <cstdint>

#include <cmath>
#include <cstdlib>


#define SIM_POINTS 100 // 模拟数据点数，根据你的屏幕控件宽度调整

namespace AppHmi {
    // 模拟数据缓存区
    float mock_z_real[SIM_POINTS];
    float mock_z_imag[SIM_POINTS];
    float mock_bode_mag[SIM_POINTS];
    float mock_bode_phase[SIM_POINTS];

    // 生成带有模拟误差的 EIS 数据
   // 生成带有模拟误差的 EIS 数据 (单片机安全版)
    void Generate_Mock_EIS_Data() {
        float Rs = 3.1f;   
        float Rct = 4.2f;  
        float Cdl = 0.08f; 
        float W = 1.5f;    
        float L = 1.2e-5f; 

        for (size_t i = 0; i < SIM_POINTS; i++) {
            // 【修改 1】全部换成 powf (单精度)
            float freq = 1000.0f * powf(10.0f, -4.0f * (float)i / (SIM_POINTS - 1));
            float omega = 2.0f * PI * freq;

            // 【修改 2】全部换成 powf 和 sqrtf
            float denominator = 1.0f + powf(omega * Rct * Cdl, 2.0f);
            float real_part = Rs + Rct / denominator + W / sqrtf(omega);
            float imag_part = -(omega * Rct * Rct * Cdl) / denominator - W / sqrtf(omega) + omega * L;

            // 【修改 3】弃用 rand()！用简单的取余运算结合索引制造“伪随机”噪声，完全不依赖 Heap
            // i % 7 会产生 0~6，减 3 变成 -3~3，乘以 0.05 就是约 ±0.15 的噪声
            float noise_real = ((float)(i % 7) - 3.0f) * 0.05f;
            float noise_imag = ((float)(i % 5) - 2.0f) * 0.05f;

            mock_z_real[i] = real_part + noise_real;
            mock_z_imag[i] = -(imag_part + noise_imag);

            // 【修改 4】换成 sqrtf 和 atan2f
            mock_bode_mag[i] = sqrtf(powf(mock_z_real[i], 2.0f) + powf(mock_z_imag[i], 2.0f));
            mock_bode_phase[i] = atan2f(mock_z_imag[i], mock_z_real[i]) * 180.0f / PI;
        }
    }
}    
AppHmi::Prostaus staus;

    

extern"C" void Battery_screen::init(void)
{
        TjcHmi::Init(COM_TJC) ;
        AppHmi::Init() ;
        AppHmi::Update_Status(AppHmi::Prostaus::Unstart);
}

extern "C" void Battery_screen::update(void) {
    Get();
    clear();

    // =====================================
    // 状态 1：处理按下 Start 的瞬间
    // =====================================
    if (true == AppHmi::GetStartStaus()) {
        AppHmi::setStartStuas(false);
        AppHmi::Update_Status(AppHmi::Prostaus::Doing); // 亮黄色
        staus = AppHmi::Prostaus::Doing;
        AppHmi::Generate_Mock_EIS_Data();
        
        // 【关键修复】直接 return！
        // 让当前这轮循环结束，给底层的串口驱动时间把 "亮黄色" 的指令发出去
        return; 
    }

    // =====================================
    // 状态 2：处理正在测量中 (Doing)
    // =====================================
    if (staus == AppHmi::Prostaus::Doing) {
        
        // 【关键修复】确认你的 Tick 频率。假设是 1000Hz，8000 是 8 秒。
        // 如果你的板子是 100Hz，改成 tx_thread_sleep(800); 才是 8 秒！
        tx_thread_sleep(2000); 

        AppHmi::Update_Status(AppHmi::Prostaus::Finsh); // 亮红色
        staus = AppHmi::Prostaus::Finsh;
        
        return; // 同样 return，保证亮红色的指令发出去
    }

    // =====================================
    // 状态 3：处理测量完成 (Finsh)
    // =====================================
    if (staus == AppHmi::Prostaus::Finsh) {
        // 判断页面并发送数据
        if (get_page == AppHmi::PAGE_2_NYQUIST) {
            send(NYQUIST, AppHmi::mock_z_real, 0); 
            send(NYQUIST, AppHmi::mock_z_imag, 1);
        }
        else if (get_page == AppHmi::PAGE_3_BODE) {
            send(BODE, AppHmi::mock_bode_mag, 0);
            send(BODE, AppHmi::mock_bode_phase, 1);
        }

        
        tx_thread_sleep(500);                 // 选项 B：每半秒刷新一次波形（配合你那个动态刷新功能）
    }
}
extern"C" void Battery_screen::clear(void)
{
    if(AppHmi::PAGE_2_NYQUIST != get_page && AppHmi::PAGE_3_BODE != get_page)
    {
        get_status = AppHmi::CruveStatus::CLEAR ;
    }


}
extern"C" void Battery_screen::Get(void)
{
        get_brand = AppHmi::GetBrand() ;
        get_Capacity =AppHmi::GetCapacity() ;
        get_Cells =AppHmi::GetCells() ;
        get_status = AppHmi::GetCurveStatus();
        get_page = AppHmi::GetCurrentPage() ;
        get_start = AppHmi::GetStartStaus();
}