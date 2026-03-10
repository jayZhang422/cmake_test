#ifndef BSP_TJC_HMI_HPP
#define BSP_TJC_HMI_HPP

#include "bsp_usart.h"
#include <stdint.h>

namespace TjcHmi {

    /**
     * @brief 初始化串口屏绑定的串口
     * @param port 你的串口 ID，例如 COM_DEBUG
     */
    void Init(bsp_com_id_e port);

    /**
     * @brief 1. 发送基础控制指令 (支持格式化)
     * @example TjcHmi::SendCmd("page home");
     * @example TjcHmi::SendCmd("click b0,1"); 
     */
    void SendCmd(const char* fmt, ...);

    /**
     * @brief 2. 整数赋值
     * @example TjcHmi::SetValue("j0", 50);  // 对应 j0.val=50
     */
    void SetValue(const char* obj_name, int32_t val);

    /**
     * @brief 3. 文本赋值 (自动处理双引号转义)
     * @example TjcHmi::SetText("t0", "测试中"); // 对应 t0.txt="测试中"
     */
    void SetText(const char* obj_name, const char* text);

    /**
     * @brief 4. 浮点数赋值 (🔥 核心：零浮点库开销，定点拆分)
     * @param decimals 保留的小数位数 (1~3)
     * @example TjcHmi::SetFloat("r_ct", 15.42f, 2); // 对应 r_ct.txt="15.42"
     */
    void SetFloat(const char* obj_name, float val, uint8_t decimals = 2);

}

#endif // BSP_TJC_HMI_HPP