#include "bsp_tjc_hmi.hpp"
#include "tx_api.h"
#include <cstdio>
#include <cstdarg>

namespace TjcHmi {

    // 缓冲区大小，剔除画图后 128 字节绰绰有余
    #define TJC_BUF_SIZE 128
    
    // 静态私有资源 (放入 .bss 段，省栈空间)
    static bsp_com_id_e g_uart_port;
    static uint8_t      g_tx_buf[TJC_BUF_SIZE];
    static TX_MUTEX     g_hmi_mutex;
    static bool         g_is_init = false;

    /**
     * @brief 内部发送引擎
     * @note 调用前必须已获取 Mutex 锁
     */
    static void SendRawBuffer(int valid_len) {
        // 边界防御：长度异常或超过缓冲区，直接丢弃
        if (valid_len <= 0 || valid_len >= (TJC_BUF_SIZE - 3)) return;

        // 追加 TJC 专属结束符
        g_tx_buf[valid_len]     = 0xFF;
        g_tx_buf[valid_len + 1] = 0xFF;
        g_tx_buf[valid_len + 2] = 0xFF;

        // 触发 DTC 硬件搬运，线程在此阻塞出让 CPU
        BSP_Serial_Send(g_uart_port, g_tx_buf, (uint32_t)(valid_len + 3));

        // ⚠️ 时序避让：给屏幕内部 CPU 留出 2 个 Tick 的解析时间，防止吞指令
        tx_thread_sleep(2); 
    }

    void Init(bsp_com_id_e port) {
        if (g_is_init) return;
        g_uart_port = port;
        // 创建独立互斥锁，支持优先级继承防止反转
        tx_mutex_create(&g_hmi_mutex, (CHAR*)"TJC_MUTEX", TX_INHERIT);
        BSP_Serial_Init(port);
        g_is_init = true;
    }

    void SendCmd(const char* fmt, ...) {
        if (!g_is_init) return;
        
        // 🔒 第一时间上锁，保护 g_tx_buf
        tx_mutex_get(&g_hmi_mutex, TX_WAIT_FOREVER);
        
        va_list args;
        va_start(args, fmt);
        // snprintf 天生自带截断保护，不需要额外测 strlen
        int len = vsnprintf((char*)g_tx_buf, TJC_BUF_SIZE - 3, fmt, args);
        va_end(args);

        SendRawBuffer(len);
        
        // 🔓 发送完毕解锁
        tx_mutex_put(&g_hmi_mutex);
    }

    void SetValue(const char* obj_name, int32_t val) {
        if (!g_is_init) return;
        tx_mutex_get(&g_hmi_mutex, TX_WAIT_FOREVER);
        
        int len = snprintf((char*)g_tx_buf, TJC_BUF_SIZE - 3, "%s.val=%ld", obj_name, (long)val);
        SendRawBuffer(len);
        
        tx_mutex_put(&g_hmi_mutex);
    }

    void SetText(const char* obj_name, const char* text) {
        if (!g_is_init) return;
        tx_mutex_get(&g_hmi_mutex, TX_WAIT_FOREVER);
        
        // 自动拼装 .txt="" 的双引号语法，上层不再操心转义符
        int len = snprintf((char*)g_tx_buf, TJC_BUF_SIZE - 3, "%s.txt=\"%s\"", obj_name, text);
        SendRawBuffer(len);
        
        tx_mutex_put(&g_hmi_mutex);
    }

    void SetFloat(const char* obj_name, float val, uint8_t decimals) {
        if (!g_is_init) return;
        tx_mutex_get(&g_hmi_mutex, TX_WAIT_FOREVER);

        // 沿用您的 AppPrint 定点化拆分逻辑，彻底告别 %f 带来的 Flash 膨胀
        char sign = '+';
        if (val < 0.0f) {
            sign = '-';
            val = -val;
        }
        
        int i_part = (int)val;
        float multiplier = (decimals == 1) ? 10.0f : ((decimals == 2) ? 100.0f : 1000.0f);
        int d_part = (int)((val - (float)i_part) * multiplier);

        // 构造指令并自动补齐小数位的 0 (比如 .05 不会变成 .5)
        int len = 0;
        const char* fmt_str = (sign == '-') ? "%s.txt=\"-%d.%0*d\"" : "%s.txt=\"%d.%0*d\"";
        
        len = snprintf((char*)g_tx_buf, TJC_BUF_SIZE - 3, fmt_str, obj_name, i_part, decimals, d_part);

        SendRawBuffer(len);
        tx_mutex_put(&g_hmi_mutex);
    }
}