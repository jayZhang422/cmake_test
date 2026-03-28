#include "bsp_adc.h"
#include "bsp_config.h"

/* ============================================================== */
/* 内部结构体定义                                                  */
/* ============================================================== */
typedef struct
{
    adc_instance_t const * p_hal_instance;
} bsp_adc_static_cfg_t;

typedef struct
{
    TX_SEMAPHORE            scan_sema;     /* 扫描完成信号量 */
    TX_MUTEX                adc_mutex;     /* 互斥锁 */
    bool                    is_init;       
} bsp_adc_runtime_ctrl_t;

/* ============================================================== */
/* 硬件映射表                                                     */
/* ============================================================== */
static const bsp_adc_static_cfg_t g_adc_hw_map[BSP_ADC_NUM] = 
{
    [BSP_ADC_0] = { .p_hal_instance = ADC0_INSTANCE },
};

static bsp_adc_runtime_ctrl_t g_adc_run_ctrl[BSP_ADC_NUM];

/* ============================================================== */
/* 函数实现                                                       */
/* ============================================================== */

void BSP_ADC_Init(bsp_adc_id_e id)
{
    if(id >= BSP_ADC_NUM) return;

    bsp_adc_runtime_ctrl_t *p_ctrl = &g_adc_run_ctrl[id];
    const bsp_adc_static_cfg_t *p_cfg  = &g_adc_hw_map[id];

    if (p_ctrl->is_init) return;

    tx_semaphore_create(&p_ctrl->scan_sema, "ADC_SEMA", 0);
    tx_mutex_create(&p_ctrl->adc_mutex, "ADC_MUTEX", TX_INHERIT);

    
    p_cfg->p_hal_instance->p_api->open(p_cfg->p_hal_instance->p_ctrl, p_cfg->p_hal_instance->p_cfg);

    
    p_cfg->p_hal_instance->p_api->scanCfg(p_cfg->p_hal_instance->p_ctrl, p_cfg->p_hal_instance->p_channel_cfg);

    /*  注入 Context 并设置回调 */
    p_cfg->p_hal_instance->p_api->callbackSet(
        p_cfg->p_hal_instance->p_ctrl, 
        bsp_common_adc, 
        (void *)p_ctrl, 
        NULL
    );

    p_ctrl->is_init = true;
}


fsp_err_t BSP_ADC_ScanStart(bsp_adc_id_e id)
{
    if(id >= BSP_ADC_NUM) return FSP_ERR_INVALID_ARGUMENT;

    /* 清空信号量 */
    while(tx_semaphore_get(&g_adc_run_ctrl[id].scan_sema, TX_NO_WAIT) == TX_SUCCESS);

    return g_adc_hw_map[id].p_hal_instance->p_api->scanStart(g_adc_hw_map[id].p_hal_instance->p_ctrl);
}

/* * 等待扫描完成 (阻塞)
 * 适用于非 DMA 模式，或者需要等待 DMA 搬运完成后再进行处理
 */
fsp_err_t BSP_ADC_WaitScanComplete(bsp_adc_id_e id, uint32_t timeout_ticks)
{
    if(id >= BSP_ADC_NUM) return FSP_ERR_INVALID_ARGUMENT;

    if(TX_SUCCESS == tx_semaphore_get(&g_adc_run_ctrl[id].scan_sema, timeout_ticks))
    {
        return FSP_SUCCESS;
    }
    return FSP_ERR_TIMEOUT;
}

/* * 读取单个通道数据 (非 DMA 模式用)
 */
fsp_err_t BSP_ADC_Read(bsp_adc_id_e id, adc_channel_t channel, uint16_t * const p_data)
{
    if(id >= BSP_ADC_NUM) return FSP_ERR_INVALID_ARGUMENT;

    return g_adc_hw_map[id].p_hal_instance->p_api->read(
        g_adc_hw_map[id].p_hal_instance->p_ctrl,
        channel,
        p_data
    );
}

/* ============================================================== */
/* 通用 ADC 回调函数                                              */
/* ============================================================== */
void bsp_common_adc(adc_callback_args_t * p_args)
{
    bsp_adc_runtime_ctrl_t *p_ctrl = (bsp_adc_runtime_ctrl_t *)p_args->p_context;
    
    if (NULL == p_ctrl) return;

    if (ADC_EVENT_SCAN_COMPLETE == p_args->event)
    {
        tx_semaphore_put(&p_ctrl->scan_sema);
    }
}