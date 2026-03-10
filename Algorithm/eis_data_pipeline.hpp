#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "tx_api.h"

namespace EIS {

// 定义每次 DMA 搬运的数据块大小 (需结合扫频管理器的 IntegerCycleOptimizer 动态修改，这里设最大值)
constexpr uint32_t MAX_DMA_BUFFER_SIZE = 2048;

// ThreadX 事件标志位定义
constexpr ULONG EVENT_DMA_PING_READY = (1 << 0);
constexpr ULONG EVENT_DMA_PONG_READY = (1 << 1);
constexpr ULONG EVENT_SWEEP_STOP     = (1 << 2); // 紧急停止标志

class DataPipeline {
public:
    /**
     * @brief 初始化数据流水线 (在 RTOS 启动前或任务初始化时调用)
     */
    static void Init() {
        // 创建 ThreadX 事件标志组
        tx_event_flags_create(&_dma_events, (CHAR*)"EIS_DMA_Events");
        _current_buffer_is_ping = true;
    }

    /**
     * @brief 获取可供 ADC/DMA 写入的下一个缓冲区地址
     * @return uint16_t* 指向内部静态缓冲区的指针
     */
    static uint16_t* GetNextWriteBuffer() {
        return _current_buffer_is_ping ? _ping_buffer : _pong_buffer;
    }

    /**
     * @brief [中断上下文中调用] 通知数据流，DMA 已填满当前缓冲区
     * @note 必须在底层的 dmac_common_isr 或 adc 扫频结束中断中调用
     */
    static void NotifyDmaCompleteFromISR() {
        if (_current_buffer_is_ping) {
            // Ping 填满了，告诉 RTOS 任务去处理 Ping，同时将下一次的写入目标切到 Pong
            tx_event_flags_set(&_dma_events, EVENT_DMA_PING_READY, TX_OR);
            _current_buffer_is_ping = false; 
        } else {
            // Pong 填满了，告诉 RTOS 任务去处理 Pong，同时将下一次的写入目标切到 Ping
            tx_event_flags_set(&_dma_events, EVENT_DMA_PONG_READY, TX_OR);
            _current_buffer_is_ping = true;
        }
    }

    /**
     * @brief [RTOS 任务中调用] 阻塞等待下一块数据就绪
     * @param pp_ready_buffer [输出] 返回装满数据的缓冲区指针
     * @param timeout_ticks   超时时间
     * @return bool           是否成功获取到数据 (false 可能意味着超时或被紧急停止)
     */
    static bool WaitForDataBlock(const uint16_t** pp_ready_buffer, ULONG timeout_ticks) {
        if (!pp_ready_buffer) return false;

        ULONG actual_events = 0;
        // 阻塞等待 Ping 或 Pong 就绪，或者接收到停止命令
        UINT status = tx_event_flags_get(&_dma_events, 
                                         EVENT_DMA_PING_READY | EVENT_DMA_PONG_READY | EVENT_SWEEP_STOP, 
                                         TX_OR_CLEAR, // 获取后自动清除标志位
                                         &actual_events, 
                                         timeout_ticks);

        if (status != TX_SUCCESS) {
            return false; // 超时
        }

        if (actual_events & EVENT_SWEEP_STOP) {
            return false; // 收到紧急停止信号
        }

        // 根据唤醒的事件，将对应的缓冲区指针交给算法层
        if (actual_events & EVENT_DMA_PING_READY) {
            *pp_ready_buffer = _ping_buffer;
            return true;
        }

        if (actual_events & EVENT_DMA_PONG_READY) {
            *pp_ready_buffer = _pong_buffer;
            return true;
        }

        return false;
    }

    /**
     * @brief 紧急终止流水线 (供安全模块调用)
     */
    static void StopPipeline() {
        tx_event_flags_set(&_dma_events, EVENT_SWEEP_STOP, TX_OR);
    }

private:
    // ThreadX 事件组
    static TX_EVENT_FLAGS_GROUP _dma_events;
    
    // 乒乓标志位 ( true = DMA 正在写 Ping, CPU 处理 Pong )
    static bool _current_buffer_is_ping;

    // 严苛的 4 字节对齐，保证 CPU FPU 访问速度与 DMA 总线效率
    alignas(4) static uint16_t _ping_buffer[MAX_DMA_BUFFER_SIZE];
    alignas(4) static uint16_t _pong_buffer[MAX_DMA_BUFFER_SIZE];

    
};


} // namespace EIS