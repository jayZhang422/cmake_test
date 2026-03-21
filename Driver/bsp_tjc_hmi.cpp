#include "bsp_tjc_hmi.hpp"
#include "tx_api.h"
#include <cstdio>
#include <cstdarg>

namespace TjcHmi {

    #define TJC_BUF_SIZE 128
    
    static bsp_com_id_e g_uart_port;
    
    // 整个 HMI 层共享的发送缓冲区，受到 g_hmi_mutex 保护
    static uint8_t      g_tx_buf[TJC_BUF_SIZE];
    
    // HMI 协议事务锁（区别于 bsp_usart.c 中的硬件传输锁）
    static TX_MUTEX     g_hmi_mutex;
    static bool         g_is_init = false;

    // 回调函数全局指针
    static RxEventCb_t  g_rx_cb = nullptr;
    static StringEventCb_t g_str_cb = nullptr;
    static NumEventCb_t    g_num_cb =nullptr;

    static const uint8_t g_end_frame[3] = {0xFF, 0xFF, 0xFF};

    void Init(bsp_com_id_e port) {
        if (g_is_init) return;
        g_uart_port = port;
        
        // 创建事务锁，支持优先级继承防反转
        tx_mutex_create(&g_hmi_mutex, (CHAR*)"TJC_MUTEX", TX_INHERIT);
        
        // 调用底层的初始化 (底层内部会创建 tx_mutex 和 rx_mutex)
        BSP_Serial_Init(port);
        g_is_init = true;
    }

    void LockTx(void) {
        if (g_is_init) tx_mutex_get(&g_hmi_mutex, TX_WAIT_FOREVER);
    }

    void UnlockTx(void) {
        if (g_is_init) tx_mutex_put(&g_hmi_mutex);
    }

    void SendCmd(const char* fmt, ...) {
        if (!g_is_init || (nullptr == fmt)) return;
        
        // 获取 HMI 事务锁：
        // 1. 保护全局的 g_tx_buf 不被其他线程篡改
        // 2. 保护屏幕接收指令后的 2 tick 消化时间
        LockTx();

        va_list args;
        va_start(args, fmt);
        int len = vsnprintf((char*)g_tx_buf, TJC_BUF_SIZE - 3, fmt, args);
        va_end(args);

        if (len > 0) {
            if (len >= (TJC_BUF_SIZE - 3)) {
                len = TJC_BUF_SIZE - 4;
            }
            // 自动追加结束符
            g_tx_buf[len]     = 0xFF;
            g_tx_buf[len + 1] = 0xFF;
            g_tx_buf[len + 2] = 0xFF;
            
            // 此时调用 BSP 会触发底层 tx_mutex 的获取和释放，这是安全的嵌套逻辑
            BSP_Serial_Send(g_uart_port, g_tx_buf, (uint32_t)(len + 3));
            
            // 保持 HMI 锁，休眠 2 tick 给屏幕喘息时间，防止其他 HMI 线程趁虚而入
            tx_thread_sleep(2); 
        }

        UnlockTx();
    }

    void SendRawData(const uint8_t* data, uint16_t len) {
        if (!g_is_init || data == nullptr || len == 0) return;
        
        LockTx();
        BSP_Serial_Send(g_uart_port, (uint8_t*)data, len);
        UnlockTx();
    }

    void SendEndFrame(void) {
        if (!g_is_init) return;
        
        LockTx();
        BSP_Serial_Send(g_uart_port, (uint8_t*)g_end_frame, 3);
        UnlockTx();
    }

    void SetRxCallback(RxEventCb_t cb) {
        g_rx_cb = cb;
    }

    void SetStringCallback(StringEventCb_t cb) {
    g_str_cb = cb;
    }

    void SetNumberCallback(NumEventCb_t cb)
    {
        g_num_cb = cb ;
    }
   void RxTaskLoop(void) {
    if (!g_is_init) return;

    uint8_t rx_byte;
    static uint8_t state = 0;
    static uint8_t frame[7];
    static uint8_t ff_count = 0;

    // 新增：专门接 0D 0A 结尾的数据（能接字符串，也能接刚才的数字）
    static uint8_t raw_buf[32];
    static uint8_t raw_idx = 0;
    uint16_t budget = 64U;

    // 时序敏感逻辑：单次循环限制处理量，防止接收突发时长时间占用线程。
    while ((budget-- > 0U) && (FSP_SUCCESS == BSP_Serial_ReadByteTry(g_uart_port, &rx_byte))) {
        if (state == 0) {
            if (rx_byte == 0x65) {
                // 1. 发现触摸按键帧头 0x65
                frame[0] = rx_byte;
                state = 1;
                ff_count = 0;
                raw_idx = 0; // 清空杂乱数据
            } 
            else {
                // 2. 不是 0x65，通通放进缓冲罐子观察
                if (raw_idx < sizeof(raw_buf)) {
                    raw_buf[raw_idx++] = rx_byte;
                }

                // 3. 检查是不是以 0D 0A 结尾了？
                if (raw_idx >= 2 && raw_buf[raw_idx - 2] == 0x0D && raw_buf[raw_idx - 1] == 0x0A) {
                    
                    // 情况A：刚好收到了 4 个字节 (比如 78 05 0D 0A)，这就是你刚才测出来的数字！
                    if (raw_idx == 4) {
                        int32_t val = raw_buf[0] | (raw_buf[1] << 8); // 小端模式还原成整数
                        if (g_num_cb) g_num_cb(val); // 报告给业务层
                    }
                    // 情况B：长度大于 4 个字节，说明是屏幕发来的品牌英文字符串 (比如 Tattu\r\n)
                    else if (raw_idx > 2) {
                        raw_buf[raw_idx - 2] = '\0'; // 抹掉 0D 0A，变成 C 语言字符串
                        if (g_str_cb) g_str_cb((char*)raw_buf); // 报告给业务层
                    }

                    raw_idx = 0; // 处理完，清空罐子接下一个
                }
            }
        }
        else {
            // 这里保留原有的 0x65 触摸协议解析代码
            switch(state) {
                case 1: case 2: case 3:
                    frame[state] = rx_byte;
                    state++;
                    break;
                case 4: case 5: case 6:
                    if (rx_byte == 0xFF) {
                        ff_count++;
                        if (ff_count == 3) {
                            if (g_rx_cb) g_rx_cb(frame[1], frame[2], frame[3]);
                            state = 0; 
                        }
                    } else { state = 0; }
                    break;
                default: state = 0; break;
            }
        }
    }
}
}
