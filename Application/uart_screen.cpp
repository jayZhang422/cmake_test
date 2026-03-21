#include "uart_screen.hpp"
#include "bsp_tjc_hmi.hpp" // 引入我们刚刚写的底层纯驱动
#include <cstdio>
#include <cstring>

namespace AppHmi {

    // ==========================================
    // 内部辅助函数：重置高亮颜色为灰色
    // ==========================================
    static void Reset_Page4_Colors() {
        // 重置内阻状态颜色 
        TjcHmi::SendCmd("page4.t14.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t13.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t12.pco=%u", COLOR_GRAY);
        
        // 重置转移阻抗状态颜色 
        TjcHmi::SendCmd("page4.t11.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t9.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t8.pco=%u", COLOR_GRAY);
        
        // 重置健康度颜色 
        TjcHmi::SendCmd("page4.t7.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t6.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t5.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t4.pco=%u", COLOR_GRAY);
        TjcHmi::SendCmd("page4.t15.pco=%u", COLOR_GRAY);
    }

    // ==========================================
    // Page 0 事件处理 (读取为主) 
    // ==========================================
    static void Handle_Page0(uint8_t cmp, uint8_t event) {
        if (event != 0x01) return; // 只处理按下事件

        switch (cmp) {
            case CmpId::P0_BTN_BATT_TYPE:
                // 触发：读取或切换电池类型设定 
                break;
            case CmpId::P0_BTN_NYQUIST:
                // 触发：跳转到尼奎斯特图或开始测量 
                break;
            case CmpId::P0_BTN_BODE:
                // 触发：跳转到波特图 
                break;
            case CmpId::P0_BTN_HEALTH:
                // 触发：进行健康度评估 
                break;
            default: break;
        }
    }

    // ==========================================
    // Page 1 事件处理 (读取为主) 
    // ==========================================
    static void Handle_Page1(uint8_t cmp, uint8_t event) {
        if (event != 0x01) return; 

        switch (cmp) {
            case CmpId::P1_BTN_BRAND:
                // 触发：读取电池品牌 (花牌，格氏，博世) 
                break;
            case CmpId::P1_NUM_CELLS:
                // 触发：读取节数 
                break;
            case CmpId::P1_NUM_CAPACITY:
                // 触发：读取容量 
                break;
            default: break;
        }
    }

    // ==========================================
    // 核心事件路由总线 (挂载到底层驱动的回调)
    // ==========================================
    static void Router_Callback(uint8_t page, uint8_t cmp, uint8_t event) {
        switch (page) {
            case PAGE_0_HOME:    Handle_Page0(cmp, event); break;
            case PAGE_1_SETTING: Handle_Page1(cmp, event); break;
            case PAGE_2_NYQUIST: /* 处理 page2 动作 */ break;
            case PAGE_3_BODE:    /* 处理 page3 动作 */ break;
            case PAGE_4_RESULT:  /* 处理 page4 返回动作等 */ break;
            default: break;
        }
    }

   static BrandDetecet g_brand  = BrandDetecet::Tattu;

    static void String_Callback(const char* str) {
        if (strcmp(str, "Tattu") == 0)
        {
            g_brand =BrandDetecet::Tattu ;
        }
        else if (strcmp(str, "Infinity") == 0) 
        {
            g_brand = BrandDetecet::Infinity ;
        }
        else if (strcmp(str, "BosLi-po") == 0)
        {
            g_brand = BrandDetecet::BosLi ;
        }
    }
    
    BrandDetecet GetBrand(void)
    {
        return  g_brand;
    }

    // ==========================================
    // 暴露的初始化接口
    // ==========================================
    void Init() {
        // 将路由函数注册到底层监控平台
        TjcHmi::SetRxCallback(Router_Callback);
        TjcHmi::SetStringCallback(String_Callback);
    }

    // ==========================================
    // 暴露的业务下发接口 (Page 4) 
    // ==========================================
    void Update_InternalResistance(float value, MeasureState state) {
        // 1. 写数值 (t2) 
        TjcHmi::SendCmd("page4.t2.txt=\"%.2f mR\"", value);
        
        // 2. 更新高亮状态 (t14偏小, t13正常, t12偏大) 
        TjcHmi::SendCmd("page4.t14.pco=%u", state == MeasureState::LOW ? COLOR_YELLOW : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t13.pco=%u", state == MeasureState::NORMAL ? COLOR_GREEN : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t12.pco=%u", state == MeasureState::HIGH ? COLOR_RED : COLOR_GRAY);
    }

    void Update_TransferImpedance(float value, MeasureState state) {
        // 1. 写数值 (t3) 
        TjcHmi::SendCmd("page4.t3.txt=\"%.2f mR\"", value);
        
        // 2. 更新高亮状态 (t11偏小, t9正常, t8偏大) 
        TjcHmi::SendCmd("page4.t11.pco=%u", state == MeasureState::LOW ? COLOR_YELLOW : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t9.pco=%u", state == MeasureState::NORMAL ? COLOR_GREEN : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t8.pco=%u", state == MeasureState::HIGH ? COLOR_RED : COLOR_GRAY);
    }

    void Update_HealthStatus(HealthLevel level) {
        // 互斥高亮健康度 (t7, t6, t5, t4, t15) 
        TjcHmi::SendCmd("page4.t7.pco=%u",  level == HealthLevel::EXCELLENT ? COLOR_GREEN  : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t6.pco=%u",  level == HealthLevel::GOOD      ? COLOR_GREEN  : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t5.pco=%u",  level == HealthLevel::FAIR      ? COLOR_YELLOW : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t4.pco=%u",  level == HealthLevel::POOR      ? COLOR_RED    : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t15.pco=%u", level == HealthLevel::INFERIOR  ? COLOR_RED    : COLOR_GRAY);
    }

}