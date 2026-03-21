#ifndef APP_HMI_LOGIC_HPP
#define APP_HMI_LOGIC_HPP

#include <stdint.h>
#include <stddef.h>   
#include "bsp_tjc_hmi.hpp"
namespace AppHmi {

    // ==========================================
    // 1. 页面 ID 定义 (根据 ckp.txt 规划) 
    // ==========================================
    constexpr uint8_t PAGE_0_HOME     = 0; // 主页
    constexpr uint8_t PAGE_1_SETTING  = 1; // 电池设置页
    constexpr uint8_t PAGE_2_NYQUIST  = 2; // 尼奎斯特图
    constexpr uint8_t PAGE_3_BODE     = 3; // 波特图
    constexpr uint8_t PAGE_4_RESULT   = 4; // 结果展示页

    // ==========================================
    // 2. 控件 ID 定义 (用于接收按键解析)
    // 注意：这里的 ID 必须与你串口屏工程中的实际控件 ID 对应
    // ==========================================
    namespace CmpId {
        // Page 0 控件 ID 
        constexpr uint8_t P0_BTN_BATT_TYPE = 1; // b0
        constexpr uint8_t P0_BTN_NYQUIST   = 2; // b1
        constexpr uint8_t P0_BTN_BODE      = 3; // b2
        constexpr uint8_t P0_BTN_HEALTH    = 4; // b3

        constexpr uint8_t P1_BTN_BRAND     = 5; // b1 (品牌按钮，ID为5)
        constexpr uint8_t P1_NUM_CELLS     = 1; // n0 (节数，ID为1)
        constexpr uint8_t P1_NUM_CAPACITY  = 2; // n1 (容量，ID为2)
    }

    // ==========================================
    // 3. 颜色常量定义 (RGB565 格式)
    // ==========================================
    constexpr uint16_t COLOR_DEFAULT = 0x0000;  // 黑色
    constexpr uint16_t COLOR_GREEN   = 0x07E0;  // 绿色 (正常/优秀)
    constexpr uint16_t COLOR_YELLOW  = 0xFFE0;  // 黄色 (偏小/良好)
    constexpr uint16_t COLOR_RED     = 0xF800;  // 红色 (偏大/劣质)
    constexpr uint16_t COLOR_GRAY    = 0x8410;  // 灰色 (未激活状态)

    // ==========================================
    // 4. 业务状态枚举
    // ==========================================
    enum class MeasureState {
        LOW,    // 偏小 
        NORMAL, // 正常 
        HIGH    // 偏大 
    };

    enum class HealthLevel {
        EXCELLENT, // 优秀 (t7) 
        GOOD,      // 良好 (t6) 
        FAIR,      // 一般 (t5) 
        POOR,      // 较差 (t4) 
        INFERIOR   // 劣质 (t15) 
    };

    enum class BrandDetecet {
        Tattu,//格氏
        Infinity,//花牌
        BosLi,//博世
    };

    enum class CruveStatus {
        STILL,
        DYNAMIC,
        CLEAR,
     };

    // ==========================================
    // 5. 对外暴露的 API
    // ==========================================
    
    /**
     * @brief 业务层初始化，负责把回调注册到底层驱动
     */
    void Init();

    // ---- Page 4 的主动更新接口 ---- 
    void Update_InternalResistance(float value, MeasureState state);
    void Update_TransferImpedance(float value, MeasureState state);
    void Update_HealthStatus(HealthLevel level);
    BrandDetecet GetBrand(void);
    int32_t GetCells(void);
    int32_t GetCapacity(void);
    uint8_t GetCurrentPage(void);
    CruveStatus GetCurveStatus (void);
    void Clear_Curve(uint8_t curve_id, uint8_t channel);
    void Add_Curve_Point(uint8_t curve_id, uint8_t channel, uint8_t val);
    template <size_t N>
    void Fast_Send_Float_Array(uint8_t curve_id, uint8_t channel, const float (&data)[N]) {
        if (N == 0) return;
        float min_v, max_v;
        size_t i;
        if (N % 2 != 0) { min_v = max_v = data[0]; i = 1; }
        else {
            if (data[0] < data[1]) { min_v = data[0]; max_v = data[1]; }
            else { min_v = data[1]; max_v = data[0]; }
            i = 2;
        }
        while (i < N) {
            float l_min, l_max;
            if (data[i] < data[i+1]) { l_min = data[i]; l_max = data[i+1]; }
            else { l_min = data[i+1]; l_max = data[i]; }
            if (l_min < min_v) min_v = l_min;
            if (l_max > max_v) max_v = l_max;
            i += 2;
        }
        float range = max_v - min_v;
        static uint8_t raw[1024];
        size_t pts = (N > 1024) ? 1024 : N;
        for (size_t j = 0; j < pts; ++j) {
            raw[j] = (range > 0.0001f) ? (uint8_t)((data[j] - min_v) / range * 255.0f) : 128;
        }
        TjcHmi::SendCmd("addt %u,%u,%u", curve_id, channel, (uint16_t)pts);
        TjcHmi::SendRawData(raw, (uint16_t)pts);
    }
}

#endif // APP_HMI_LOGIC_HPP