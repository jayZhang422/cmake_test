#ifndef _BSP_USART_H
#define _BSP_USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "hal_data.h"
#include "tx_api.h"

/* 1. 逻辑设备 ID */
typedef enum
{
    COM_DEBUG = 0,    
    COM_TJC,
    /* COM_BLUETOOTH, */

    COM_NUM_MAX       
} bsp_com_id_e;

/* 用户回调定义 */
typedef void (*bsp_uart_user_cb_t)(uart_callback_args_t * p_args);

/* ============================================================== */
/* API 接口                                                       */
/* ============================================================== */

/**
 * @brief 初始化串口
 * @note  配置互斥锁、信号量，并绑定中断上下文
 */
void BSP_Serial_Init(bsp_com_id_e com_id);

/**
 * @brief 注册用户回调
 * @note  用于处理接收数据或特定错误
 */
void BSP_Serial_SetCallback(bsp_com_id_e com_id, bsp_uart_user_cb_t p_cb);

/**
 * @brief 发送原始数据 (阻塞)
 * @note  线程安全，使用 Mutex 保护
 */
fsp_err_t BSP_Serial_Send(bsp_com_id_e com_id, uint8_t * p_data, uint32_t len);

/**
 * @brief 接收数据 (阻塞)
 * @note  线程安全，等待直到接收到 len 长度的数据或超时
 */
fsp_err_t BSP_Serial_Read(bsp_com_id_e com_id, uint8_t * p_data, uint32_t len);

/**
 * @brief 格式化打印 (类似 printf)
 * @note  1. 线程安全 (支持多线程并发调用)
 * 2. 内部使用栈 buffer (256字节)，请确保线程栈足够大
 * 3. 阻塞等待发送完成
 */
fsp_err_t BSP_Printf(bsp_com_id_e com_id, const char *fmt, ...);

/**
 * @brief 通用中断回调 (需在 RASC 中填入此名称)
 */
void usart_common_callback(uart_callback_args_t * p_args);


#ifdef __cplusplus
}
#endif

#endif // _BSP_USART_H