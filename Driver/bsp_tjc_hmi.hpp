#ifndef BSP_TJC_HMI_HPP
#define BSP_TJC_HMI_HPP

#include "bsp_usart.h"
#include <stdint.h>

namespace TjcHmi {

    /**
     * @brief 初始化串口屏绑定的串口
     */
    void Init(bsp_com_id_e port);

    // ==========================================
    // 发送与赋值指令
    // ==========================================
    void SendCmd(const char* fmt, ...);
    void SetValue(const char* obj_name, int32_t val);
    void SetText(const char* obj_name, const char* text);
    void SetFloat(const char* obj_name, float val, uint8_t decimals = 2);

    /**
     * @brief 颜色设置 (对应 python 的 .pco 指令 [cite: 6, 7])
     * @param obj_name 控件名，例如 "page4.t8"
     * @param color_565 RGB565颜色值
     */
    void SetColor(const char* obj_name, uint16_t color_565);

    // ==========================================
    // 波形控件指令 (对应 python 的 HMI_Wave 系列函数 [cite: 1, 2])
    // ==========================================
    
    /**
     * @brief 清空波形数据 (cle 指令 [cite: 2])
     */
    void ClearWave(uint8_t component_id, uint8_t channel);

    /**
     * @brief 动态添加单个波形数据 (add 指令 [cite: 1])
     */
    void AddWave(uint8_t component_id, uint8_t channel, uint8_t val);

    /**
     * @brief 静态快速下发批量波形数据 (addt 指令 [cite: 2])
     * @note 内部处理了底层数据分包与时序
     */
    void AddWaveFast(uint8_t component_id, uint8_t channel, const uint8_t* data, uint16_t len);


    // ==========================================
    // 接收解析机制 (对应 python 的 parse_frame 逻辑 [cite: 12, 13])
    // ==========================================

    /**
     * @brief 触摸事件回调函数指针定义
     * @param page  页面ID
     * @param cmp   控件ID
     * @param event 0x01为按下，0x00为松开
     */
    typedef void (*TouchEventCb_t)(uint8_t page, uint8_t cmp, uint8_t event);

    /**
     * @brief 注册触摸回调函数
     */
    void SetTouchCallback(TouchEventCb_t cb);

    /**
     * @brief 串口接收解析轮询任务
     * @note 请在一个独立的 ThreadX 线程的 while(1) 循环中调用此函数
     */
    void RxTaskLoop(void);

}

#endif // BSP_TJC_HMI_HPP