#include "uart_screen.hpp"
#include "bsp_tjc_hmi.hpp" // 引入我们刚刚写的底层纯驱动
#include <cstdio>
#include <cstring>
#include <stdint.h>

namespace AppHmi {

    static int32_t g_cells = 0;     // 保存电池节数
    static int32_t g_capacity = 0;  // 保存电池容量
    static uint8_t g_active_page  = 0;
    static BrandDetecet g_brand  = BrandDetecet::Tattu;
    static CruveStatus g_is_curveDrawing = CruveStatus::CLEAR;
    static bool start = false ;
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
            case CmpId::P1_BTN_START:
                start = true ;
                break;
                
            default: break;
        }
        
    }

    static void Handle_Page4(uint8_t cmp, uint8_t event) {
       if (event != 0x00) return; 

        switch (cmp) {
            case 3: 
                // Reset_Page4_Colors(); // 离开前，把所有高亮颜色重置为灰色
                break;
            default: break;
        }
        
    }

    // ==========================================
    // 核心事件路由总线 (挂载到底层驱动的回调)
    // ==========================================
    static void Router_Callback(uint8_t page, uint8_t cmp, uint8_t event) {
        if (g_active_page != page) 
        {
            // g_is_curveDrawing = CruveStatus::CLEAR; 
        }
        g_active_page = page ;
        switch (page) {
            case PAGE_0_HOME:    Handle_Page0(cmp, event); break;
            case PAGE_1_SETTING: Handle_Page1(cmp, event); break;
            case PAGE_2_NYQUIST: /* 处理 page2 动作 */ break;
            case PAGE_3_BODE:    /* 处理 page3 动作 */ break;
            case PAGE_4_RESULT:  Handle_Page4(cmp, event); break;
            default: break;
        }
    }

    static void Number_Callback(int32_t val) {
        if (val < 100) {
            g_cells = val;
            
        } else {
            g_capacity = val;
            
        }
    }
  
  
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
        else if(strcmp(str,"4") ==0 )
        {
            g_is_curveDrawing = CruveStatus::DYNAMIC; //动态
        }
        else if(strcmp(str,"5") ==0 )
        {
            g_is_curveDrawing = CruveStatus::STILL; //静态
        }
        else if(strcmp(str,"6") == 0)
        {
            g_is_curveDrawing = CruveStatus::CLEAR; //清空
        }
    }
    CruveStatus GetCurveStatus (void)
    {
        return g_is_curveDrawing ;
    }
    BrandDetecet GetBrand(void)
    {
        return  g_brand;
    }
    int32_t GetCells(void) 
    {
        return g_cells;
    }

    int32_t GetCapacity(void) 
    {
        return g_capacity;
    }
    uint8_t GetCurrentPage(void)
    {
        return g_active_page ;
    }
    bool GetStartStaus(void)
    {
        return start ;
    }
    void setStartStuas(bool s)
    {
        start = s ;
    }
    // ==========================================
    // 暴露的初始化接口
    // ==========================================
    void Init() {
        // 将路由函数注册到底层监控平台
        TjcHmi::SetRxCallback(Router_Callback);
        TjcHmi::SetStringCallback(String_Callback);
        TjcHmi::SetNumberCallback(Number_Callback);
    }

    // ==========================================
    // 暴露的业务下发接口 (Page 4) 
    // ==========================================
void Update_InternalResistance(float value, MeasureState state) {
        // 1. 将浮点数拆分为整数和小数部分 (放大100倍取两位小数)
        int int_part = (int)value; 
        int dec_part = (int)((value - int_part) * 100.0f); 
        if(dec_part < 0) dec_part = -dec_part; // 防止出现 30.-25 的负数异常

        // 用 %d.%02d 替代 %.2f 发送
        TjcHmi::SendCmd("page4.t2.txt=\"%d.%02d mR\"", int_part, dec_part);
        
        // 2. 更新高亮状态 (t14偏小, t13正常, t12偏大) 
        TjcHmi::SendCmd("page4.t14.pco=%u", state == MeasureState::LOW ? COLOR_YELLOW : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t13.pco=%u", state == MeasureState::NORMAL ? COLOR_GREEN : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t12.pco=%u", state == MeasureState::HIGH ? COLOR_RED : COLOR_GRAY);
    }

    void Update_TransferImpedance(float value, MeasureState state) {
        // 1. 将浮点数拆分为整数和小数部分
        int int_part = (int)value; 
        int dec_part = (int)((value - int_part) * 100.0f); 
        if(dec_part < 0) dec_part = -dec_part;

        // 用 %d.%02d 替代 %.2f 发送
        TjcHmi::SendCmd("page4.t3.txt=\"%d.%02d mR\"", int_part, dec_part);
        
        // 2. 更新高亮状态 (t11偏小, t9正常, t8偏大) 
        TjcHmi::SendCmd("page4.t11.pco=%u", state == MeasureState::LOW ? COLOR_YELLOW : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t9.pco=%u", state == MeasureState::NORMAL ? COLOR_GREEN : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t8.pco=%u", state == MeasureState::HIGH ? COLOR_RED : COLOR_GRAY);
    }

    void Update_HealthStatus(HealthLevel level) {
        
       
        TjcHmi::SendCmd("page4.t6.pco=%u",  level == HealthLevel::GOOD      ? COLOR_GREEN  : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t5.pco=%u",  level == HealthLevel::FAIR      ? COLOR_YELLOW : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t4.pco=%u",  level == HealthLevel::POOR      ? COLOR_RED    : COLOR_GRAY);
        
    }
  
    void Update_Status (Prostaus staus )
    {
        TjcHmi::SendCmd("page4.t7.pco=%u",  staus == Prostaus::Unstart      ? COLOR_GREEN  : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t15.pco=%u",  staus == Prostaus::Doing      ? COLOR_YELLOW : COLOR_GRAY);
        TjcHmi::SendCmd("page4.t10.pco=%u",  staus == Prostaus::Finsh      ? COLOR_RED    : COLOR_GRAY);
    }
    void Clear_Curve(uint8_t curve_id, uint8_t channel) {
        TjcHmi::SendCmd("cle %u,%u", curve_id, channel);
    }

    // 2. 动态画一个点 (val 必须是 0~255 之间的整数)
    void Add_Curve_Point(uint8_t curve_id, uint8_t channel, uint8_t val) {
        TjcHmi::SendCmd("add %u,%u,%u", curve_id, channel, val);
    }
     
}