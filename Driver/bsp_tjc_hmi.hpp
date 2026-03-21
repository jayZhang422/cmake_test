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
    // 底层 TX 发送接口
    // ==========================================
    
    /**
     * @brief 格式化发送指令（内部自动追加 0xFF 0xFF 0xFF 结束符并加锁）
     */
    void SendCmd(const char* fmt, ...);

    /**
     * @brief 发送纯数据流（不带结束符，常用于连续波形数据的传输）
     */
    void SendRawData(const uint8_t* data, uint16_t len);

    /**
     * @brief 发送纯结束符 (0xFF 0xFF 0xFF)
     */
    void SendEndFrame(void);

    /**
     * @brief 显式获取与释放发送锁
     * @note 用于应用层需要发送复合指令组合（例如发送头 -> 延时 -> 发数据 -> 结束符）
     * 以防止被其他线程的屏幕更新打断。
     */
    void LockTx(void);
    void UnlockTx(void);

    // ==========================================
    // 底层 RX 接收解析接口
    // ==========================================

    /**
     * @brief 串口屏标准返回帧回调函数指针 (解析 0x65 按键报文)
     * @param page  页面ID
     * @param cmp   控件ID
     * @param event 0x01为按下，0x00为松开
     */
    typedef void (*RxEventCb_t)(uint8_t page, uint8_t cmp, uint8_t event);

        // 在 bsp_tjc_hmi.hpp 的接收解析机制区域添加：
        typedef void (*StringEventCb_t)(const char* str);
        typedef void (*NumEventCb_t)(int32_t val);
        void SetStringCallback(StringEventCb_t cb);
        void SetNumberCallback(NumEventCb_t cb);
    /**
     * @brief 注册接收回调函数，将解析结果抛给应用层处理
     */
    void SetRxCallback(RxEventCb_t cb);

    /**
     * @brief 串口接收解析轮询任务 (协议层状态机)
     * @note 请在一个独立的 ThreadX 线程的 while(1) 循环中调用此函数
     */
    void RxTaskLoop(void);

}

#endif // BSP_TJC_HMI_HPP