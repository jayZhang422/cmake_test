#include "bsp_tjc_hmi.hpp"
#include "tx_api.h"
#include <cstdio>
#include <cstdarg>

namespace TjcHmi {

    #define TJC_BUF_SIZE 128
    
    static bsp_com_id_e g_uart_port;
    static uint8_t      g_tx_buf[TJC_BUF_SIZE];
    static TX_MUTEX     g_hmi_mutex;
    static bool         g_is_init = false;

    // 回调函数全局指针
    static TouchEventCb_t g_touch_cb = nullptr;

    static void SendRawBuffer(int valid_len) {
        if (valid_len <= 0 || valid_len >= (TJC_BUF_SIZE - 3)) return;
        g_tx_buf[valid_len]     = 0xFF;
        g_tx_buf[valid_len + 1] = 0xFF;
        g_tx_buf[valid_len + 2] = 0xFF;
        BSP_Serial_Send(g_uart_port, g_tx_buf, (uint32_t)(valid_len + 3));
        tx_thread_sleep(2); 
    }

    void Init(bsp_com_id_e port) {
        if (g_is_init) return;
        g_uart_port = port;
        tx_mutex_create(&g_hmi_mutex, (CHAR*)"TJC_MUTEX", TX_INHERIT);
        BSP_Serial_Init(port);
        g_is_init = true;
    }

    void SendCmd(const char* fmt, ...) {
        if (!g_is_init || (nullptr == fmt)) return;
        if (TX_SUCCESS != tx_mutex_get(&g_hmi_mutex, TX_WAIT_FOREVER)) return;

        va_list args;
        va_start(args, fmt);
        int len = vsnprintf((char*)g_tx_buf, TJC_BUF_SIZE - 3, fmt, args);
        va_end(args);

        if (len > 0) {
            if (len >= (TJC_BUF_SIZE - 3)) {
                len = TJC_BUF_SIZE - 4;
            }
            SendRawBuffer(len);
        }

        tx_mutex_put(&g_hmi_mutex);
    }

    void SetValue(const char* obj_name, int32_t val) {
        if (nullptr == obj_name) return;
        SendCmd("%s.val=%ld", obj_name, (long)val);
    }

    void SetText(const char* obj_name, const char* text) {
        if ((nullptr == obj_name) || (nullptr == text)) return;
        SendCmd("%s.txt=\"%s\"", obj_name, text);
    }

    void SetFloat(const char* obj_name, float val, uint8_t decimals) {
        if (nullptr == obj_name) return;
        if (decimals > 6U) decimals = 6U;
        bool neg = (val < 0.0f);
        float abs_val = neg ? -val : val;

        uint32_t scale = 1U;
        for (uint8_t i = 0U; i < decimals; ++i)
        {
            scale *= 10U;
        }

        int32_t i_part = (int32_t)abs_val;
        int32_t d_part = 0;

        if (decimals > 0U)
        {
            float frac = abs_val - (float)i_part;
            d_part = (int32_t)(frac * (float)scale + 0.5f);
            if ((uint32_t)d_part >= scale)
            {
                i_part += 1;
                d_part = 0;
            }
        }

        if (decimals == 0U)
        {
            if (neg) SendCmd("%s.txt=\"-%ld\"", obj_name, (long)i_part);
            else SendCmd("%s.txt=\"%ld\"", obj_name, (long)i_part);
        }
        else
        {
            if (neg) SendCmd("%s.txt=\"-%ld.%0*ld\"", obj_name, (long)i_part, (int)decimals, (long)d_part);
            else SendCmd("%s.txt=\"%ld.%0*ld\"", obj_name, (long)i_part, (int)decimals, (long)d_part);
        }
    }

    // ==========================================
    // 新增：颜色与波形实现
    // ==========================================
    
    void SetColor(const char* obj_name, uint16_t color_565) {
        SendCmd("%s.pco=%u", obj_name, color_565);
    }

    void ClearWave(uint8_t component_id, uint8_t channel) {
        SendCmd("cle %d,%d", component_id, channel);
    }

    void AddWave(uint8_t component_id, uint8_t channel, uint8_t val) {
        SendCmd("add %d,%d,%d", component_id, channel, val);
    }

    void AddWaveFast(uint8_t component_id, uint8_t channel, const uint8_t* data, uint16_t len) {
        if (!g_is_init || data == nullptr || len == 0) return;
        
        tx_mutex_get(&g_hmi_mutex, TX_WAIT_FOREVER);
        
        // 1. 发送头指令 addt [cite: 2]
        int head_len = snprintf((char*)g_tx_buf, TJC_BUF_SIZE - 3, "addt %d,%d,%d", component_id, channel, len);
        if ((head_len <= 0) || (head_len >= (TJC_BUF_SIZE - 3))) {
            tx_mutex_put(&g_hmi_mutex);
            return;
        }
        g_tx_buf[head_len] = 0xFF;
        g_tx_buf[head_len + 1] = 0xFF;
        g_tx_buf[head_len + 2] = 0xFF;
        BSP_Serial_Send(g_uart_port, g_tx_buf, (uint32_t)(head_len + 3));
        
        // 关键时序：与 Python 实验脚本保持一致，addt 头后等待 100ms
        tx_thread_sleep(100);

        // 2. 连续发送纯二进制波形数据 [cite: 2]
        BSP_Serial_Send(g_uart_port, (uint8_t*)data, len);

        // 3. 发送纯结束符 [cite: 2]
        g_tx_buf[0] = 0xFF; g_tx_buf[1] = 0xFF; g_tx_buf[2] = 0xFF;
        BSP_Serial_Send(g_uart_port, g_tx_buf, 3);

        tx_mutex_put(&g_hmi_mutex);
    }

    // ==========================================
    // 新增：接收与解析机制
    // ==========================================
    
    void SetTouchCallback(TouchEventCb_t cb) {
        g_touch_cb = cb;
    }

    void RxTaskLoop(void) {
        if (!g_is_init) return;

        uint8_t rx_byte;
        static uint8_t state = 0;
        static uint8_t frame[7];
        static uint8_t ff_count = 0;

        // 这里使用超时阻塞读取，完美契合 ThreadX，不会榨干 CPU
        if (FSP_SUCCESS == BSP_Serial_Read(g_uart_port, &rx_byte, 1)) {
            // 淘晶驰/迪文屏按键报文格式：0x65 Page Cmp Event 0xFF 0xFF 0xFF [cite: 11, 12, 15]
            switch(state) {
                case 0:
                    if (rx_byte == 0x65) {
                        frame[0] = rx_byte;
                        state = 1;
                        ff_count = 0;
                    }
                    break;
                case 1: // Page ID
                case 2: // Cmp ID
                case 3: // Event (0x01 pressed, 0x00 released)
                    frame[state] = rx_byte;
                    state++;
                    break;
                case 4:
                case 5:
                case 6:
                    if (rx_byte == 0xFF) {
                        ff_count++;
                        if (ff_count == 3) {
                            // 帧校验通过，触发回调 [cite: 13]
                            if (g_touch_cb) {
                                g_touch_cb(frame[1], frame[2], frame[3]);
                            }
                            state = 0; 
                        } else {
                            state++;
                        }
                    } else {
                        // 帧格式错误，重置状态机
                        state = 0; 
                    }
                    break;
                default:
                    state = 0;
                    break;
            }
        }
    }
}
