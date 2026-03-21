#include "bsp_usart.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bsp_config.h"

/* ============================================================== */
/* 宏定义 (优化点：新增接收超时)                                   */
/* ============================================================== */
#define UART_TX_TIMEOUT_TICKS   (1000)      /* 发送超时: 1秒 */
#define UART_RX_TIMEOUT_TICKS   (2000)      /* 接收超时: 2秒 (防止永久卡死) */
#define PRINTF_BUF_SIZE         (128)
#define UART_RX_RING_SIZE       (256U)

/* ============================================================== */
/* 内部结构体定义 (保持不变)                                       */
/* ============================================================== */
typedef struct
{
    uart_instance_t const * p_hal_instance;
    char * mutex_name;
} bsp_uart_static_cfg_t;

typedef struct
{
    TX_SEMAPHORE       tx_sema;
    TX_SEMAPHORE       rx_sema;
    TX_MUTEX           tx_mutex;
    TX_MUTEX           rx_mutex;
    bool               is_init;
    bsp_uart_user_cb_t p_user_cb;
    volatile uart_event_t last_event;
    char                    print_buf[PRINTF_BUF_SIZE];
    volatile uint16_t       rx_head;
    volatile uint16_t       rx_tail;
    volatile uint32_t       rx_overflow_cnt;
    uint8_t                 rx_ring[UART_RX_RING_SIZE];
} bsp_uart_runtime_ctrl_t;

/* ============================================================== */
/* 硬件映射表 (保持不变)                                           */
/* ============================================================== */
static const bsp_uart_static_cfg_t g_uart_cfg_map[COM_NUM_MAX] = 
{
    [COM_DEBUG]     = { .p_hal_instance = DEBUGDEV_INSTANCE, .mutex_name = "UART0_M" },
    [COM_TJC]       = {.p_hal_instance = TJC_INSTANCE , .mutex_name = "UART_SCERRN"} ,
};

static bsp_uart_runtime_ctrl_t g_uart_run_ctrl[COM_NUM_MAX];

/* ============================================================== */
/* 内部函数实现 (保持不变)                                         */
/* ============================================================== */
static fsp_err_t bsp_serial_send_internal(bsp_com_id_e com_id, uint8_t const * p_data, uint32_t len)
{
    bsp_uart_runtime_ctrl_t *p_ctrl = &g_uart_run_ctrl[com_id];
    const bsp_uart_static_cfg_t *p_cfg  = &g_uart_cfg_map[com_id];
    fsp_err_t err;
    uint32_t wait_res;

    while(tx_semaphore_get(&p_ctrl->tx_sema, TX_NO_WAIT) == TX_SUCCESS);
    p_ctrl->last_event = 0;

    err = p_cfg->p_hal_instance->p_api->write(p_cfg->p_hal_instance->p_ctrl, p_data, len);
    if (FSP_SUCCESS != err) return err;

    wait_res = tx_semaphore_get(&p_ctrl->tx_sema, UART_TX_TIMEOUT_TICKS);

    if (TX_SUCCESS != wait_res)
    {
        p_cfg->p_hal_instance->p_api->communicationAbort(p_cfg->p_hal_instance->p_ctrl, UART_DIR_TX);
        return FSP_ERR_TIMEOUT;
    }

    if (UART_EVENT_TX_COMPLETE != p_ctrl->last_event)
    {
        return FSP_ERR_ABORTED;
    }

    return FSP_SUCCESS;
}

/* ============================================================== */
/* API 函数实现                                                   */
/* ============================================================== */
void BSP_Serial_Init(bsp_com_id_e com_id)
{
    if (com_id >= COM_NUM_MAX) return;

    bsp_uart_runtime_ctrl_t *p_ctrl = &g_uart_run_ctrl[com_id];
    const bsp_uart_static_cfg_t *p_cfg  = &g_uart_cfg_map[com_id];

    if (p_ctrl->is_init) return;

    tx_mutex_create(&p_ctrl->tx_mutex, p_cfg->mutex_name, TX_INHERIT);
    tx_mutex_create(&p_ctrl->rx_mutex, "UART_RX_M", TX_INHERIT);
    
    tx_semaphore_create(&p_ctrl->tx_sema, "UART_TX_S", 0);
    tx_semaphore_create(&p_ctrl->rx_sema, "UART_RX_S", 0);
    p_ctrl->rx_head = 0U;
    p_ctrl->rx_tail = 0U;
    p_ctrl->rx_overflow_cnt = 0U;

    p_cfg->p_hal_instance->p_api->open(p_cfg->p_hal_instance->p_ctrl, 
                                       p_cfg->p_hal_instance->p_cfg);

    p_cfg->p_hal_instance->p_api->callbackSet(
        p_cfg->p_hal_instance->p_ctrl, 
        usart_common_callback, 
        (void *)p_ctrl,
        NULL
    );

    p_ctrl->is_init = true;
}

void BSP_Serial_SetCallback(bsp_com_id_e com_id, bsp_uart_user_cb_t p_cb)
{
    if (com_id < COM_NUM_MAX)
    {
        g_uart_run_ctrl[com_id].p_user_cb = p_cb;
    }
}

fsp_err_t BSP_Printf(bsp_com_id_e com_id, const char *fmt, ...)
{
    if (com_id >= COM_NUM_MAX) return FSP_ERR_INVALID_ARGUMENT;
    bsp_uart_runtime_ctrl_t *p_ctrl = &g_uart_run_ctrl[com_id];

    if (!p_ctrl->is_init) return FSP_ERR_NOT_OPEN;

    fsp_err_t err;
    va_list args;

    if (TX_SUCCESS != tx_mutex_get(&p_ctrl->tx_mutex, TX_WAIT_FOREVER))
    {
        return FSP_ERR_INTERNAL;
    }

    va_start(args, fmt);
    int len = vsnprintf(p_ctrl->print_buf, PRINTF_BUF_SIZE, fmt, args);
    va_end(args);

    if (len > 0)
    {
        uint32_t tx_len = (uint32_t)len;
        if (tx_len >= PRINTF_BUF_SIZE)
        {
            tx_len = PRINTF_BUF_SIZE - 1U;
        }
        err = bsp_serial_send_internal(com_id, (uint8_t *)p_ctrl->print_buf, tx_len);
    }
    else
    {
        err = FSP_ERR_INVALID_DATA;
    }

    tx_mutex_put(&p_ctrl->tx_mutex);
    return err;
}

fsp_err_t BSP_Serial_Send(bsp_com_id_e com_id, uint8_t * p_data, uint32_t len)
{
    if (com_id >= COM_NUM_MAX) return FSP_ERR_INVALID_ARGUMENT;
    if (!g_uart_run_ctrl[com_id].is_init) return FSP_ERR_NOT_OPEN;
    
    bsp_uart_runtime_ctrl_t *p_ctrl = &g_uart_run_ctrl[com_id];
    fsp_err_t err;

    if (TX_SUCCESS != tx_mutex_get(&p_ctrl->tx_mutex, TX_WAIT_FOREVER))
    {
        return FSP_ERR_INTERNAL;
    }

    err = bsp_serial_send_internal(com_id, p_data, len);

    tx_mutex_put(&p_ctrl->tx_mutex);
    return err;
}

/* * 优化重点：BSP_Serial_Read
 * 将 TX_WAIT_FOREVER 改为 UART_RX_TIMEOUT_TICKS
 */
fsp_err_t BSP_Serial_Read(bsp_com_id_e com_id, uint8_t * p_data, uint32_t len)
{
    if (com_id >= COM_NUM_MAX) return FSP_ERR_INVALID_ARGUMENT;
    if (!g_uart_run_ctrl[com_id].is_init) return FSP_ERR_NOT_OPEN;

    bsp_uart_runtime_ctrl_t *p_ctrl = &g_uart_run_ctrl[com_id];
    const bsp_uart_static_cfg_t *p_cfg  = &g_uart_cfg_map[com_id];
    fsp_err_t err;

    /* 获取接收锁 */
    tx_mutex_get(&p_ctrl->rx_mutex, TX_WAIT_FOREVER);

    /* 清空信号量 */
    while(tx_semaphore_get(&p_ctrl->rx_sema, TX_NO_WAIT) == TX_SUCCESS);
    p_ctrl->last_event = 0;

    /* 启动接收 */
    err = p_cfg->p_hal_instance->p_api->read(p_cfg->p_hal_instance->p_ctrl, p_data, len);

    if (FSP_SUCCESS == err)
    {
        /* 优化：使用超时机制替代死等 */
        if (TX_SUCCESS != tx_semaphore_get(&p_ctrl->rx_sema, UART_RX_TIMEOUT_TICKS))
        {
            /* 超时：终止接收，防止硬件一直处于 Busy 状态 */
            p_cfg->p_hal_instance->p_api->communicationAbort(p_cfg->p_hal_instance->p_ctrl, UART_DIR_RX);
            err = FSP_ERR_TIMEOUT;
        }
        else
        {
             if ((UART_EVENT_RX_COMPLETE != p_ctrl->last_event) &&
                 !((1U == len) && (UART_EVENT_RX_CHAR == p_ctrl->last_event)))
             {
                 err = FSP_ERR_ABORTED;
             }
        }
    }

    tx_mutex_put(&p_ctrl->rx_mutex);
    return err;
}

fsp_err_t BSP_Serial_ReadByteTry(bsp_com_id_e com_id, uint8_t * p_data)
{
    if ((com_id >= COM_NUM_MAX) || (NULL == p_data))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }
    if (!g_uart_run_ctrl[com_id].is_init)
    {
        return FSP_ERR_NOT_OPEN;
    }

    bsp_uart_runtime_ctrl_t *p_ctrl = &g_uart_run_ctrl[com_id];
    uint16_t tail = p_ctrl->rx_tail;

    /* 单生产者(ISR) + 单消费者(线程)环形缓冲，空则立即返回。 */
    if (tail == p_ctrl->rx_head)
    {
        return FSP_ERR_TIMEOUT;
    }

    *p_data = p_ctrl->rx_ring[tail];
    p_ctrl->rx_tail = (uint16_t)((tail + 1U) % UART_RX_RING_SIZE);
    return FSP_SUCCESS;
}

/* ============================================================== */
/* 通用中断服务函数 (保持不变)                                     */
/* ============================================================== */
void usart_common_callback(uart_callback_args_t * p_args)
{
    bsp_uart_runtime_ctrl_t *p_ctrl = (bsp_uart_runtime_ctrl_t *)p_args->p_context;
    if (NULL == p_ctrl) return;

    p_ctrl->last_event = p_args->event;

    if (UART_EVENT_RX_CHAR == p_args->event)
    {
        uint16_t head = p_ctrl->rx_head;
        uint16_t next = (uint16_t)((head + 1U) % UART_RX_RING_SIZE);

        if (next != p_ctrl->rx_tail)
        {
            p_ctrl->rx_ring[head] = (uint8_t) p_args->data;
            p_ctrl->rx_head = next;
        }
        else
        {
            p_ctrl->rx_overflow_cnt++;
        }
    }

    if (UART_EVENT_TX_COMPLETE == p_args->event)
    {
        tx_semaphore_put(&p_ctrl->tx_sema);
    }
    else if ((UART_EVENT_RX_COMPLETE == p_args->event) ||
             (UART_EVENT_RX_CHAR     == p_args->event))
    {
        tx_semaphore_put(&p_ctrl->rx_sema);
    }
    else if ((UART_EVENT_ERR_PARITY   == p_args->event) ||
             (UART_EVENT_ERR_FRAMING  == p_args->event) ||
             (UART_EVENT_ERR_OVERFLOW == p_args->event))
    {
        tx_semaphore_put(&p_ctrl->tx_sema); 
        tx_semaphore_put(&p_ctrl->rx_sema);
    }

    if (p_ctrl->p_user_cb != NULL)
    {
        p_ctrl->p_user_cb(p_args);
    }
}
