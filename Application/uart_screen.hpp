#ifndef APP_HMI_LOGIC_HPP
#define APP_HMI_LOGIC_HPP

#include <stdint.h>

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

        // Page 1 控件 ID 
        constexpr uint8_t P1_BTN_BRAND     = 1; // b1
        constexpr uint8_t P1_NUM_CELLS     = 2; // n0
        constexpr uint8_t P1_NUM_CAPACITY  = 3; // n1
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
}

#endif // APP_HMI_LOGIC_HPP