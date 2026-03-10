#include "bsp_timer.h"
#include "bsp_config.h"

/* ============================================================== */
/* 内部结构体定义                                                  */
/* ============================================================== */

/* 1. 静态配置信息 (硬件绑定) */
typedef struct
{
    timer_instance_t const * p_hal_instance; /* FSP 生成的定时器实例 */
} bsp_timer_static_cfg_t;

/* 2. 运行时控制块 (状态管理) */
typedef struct
{
    bsp_timer_user_cb_t     p_user_cb;       /* 用户回调函数 */
    bool                    is_init;         /* 初始化标志 */
    /* Timer 操作通常是原子的或由硬件保证，无需 Mutex */
} bsp_timer_runtime_ctrl_t;

/* 3. 硬件映射表 (必须与 bsp_timer_id_e 顺序一致) */
static const bsp_timer_static_cfg_t g_timer_hw_map[BSP_TIMER_NUM] = 
{
    /* 这里的宏定义需要在 bsp_config.h 中根据 FSP 配置生成 */
    [BSP_TIMER_OVERFLOW] = { .p_hal_instance = GPT_OVERFLOW_INSTANCE },


};

static bsp_timer_runtime_ctrl_t g_timer_run_ctrl[BSP_TIMER_NUM];

/* ============================================================== */
/* 函数实现                                                       */
/* ============================================================== */

void BSP_Timer_Init(bsp_timer_id_e id)
{
    if(id >= BSP_TIMER_NUM) return;
    
    bsp_timer_runtime_ctrl_t *p_ctrl = &g_timer_run_ctrl[id];
    const bsp_timer_static_cfg_t *p_cfg = &g_timer_hw_map[id];

    if (p_ctrl->is_init) return;

    /* 1. 打开硬件驱动 */
    p_cfg->p_hal_instance->p_api->open(p_cfg->p_hal_instance->p_ctrl, p_cfg->p_hal_instance->p_cfg);

    /* 2. [优化] 动态注册回调函数，注入 Context
     * 将 p_ctrl (运行时控制块) 作为 p_context 传入，实现 ISR O(1) 访问 
     */
    p_cfg->p_hal_instance->p_api->callbackSet(
        p_cfg->p_hal_instance->p_ctrl, 
        gpt_common_isr, 
        (void *)p_ctrl, /* 注入控制块指针 */
        NULL
    );

    /* 3. 使能输出 (对于 PWM 模式很重要) */
    p_cfg->p_hal_instance->p_api->enable(p_cfg->p_hal_instance->p_ctrl);
    
    p_ctrl->is_init = true;
}

void BSP_Timer_RegisterCallback(bsp_timer_id_e id, bsp_timer_user_cb_t cb)
{
    if(id < BSP_TIMER_NUM) g_timer_run_ctrl[id].p_user_cb = cb;
}

fsp_err_t BSP_Timer_Start(bsp_timer_id_e id)
{
    if(id >= BSP_TIMER_NUM || !g_timer_run_ctrl[id].is_init) return FSP_ERR_INVALID_ARGUMENT;
    return g_timer_hw_map[id].p_hal_instance->p_api->start(g_timer_hw_map[id].p_hal_instance->p_ctrl);
}

fsp_err_t BSP_Timer_Stop(bsp_timer_id_e id)
{
    if(id >= BSP_TIMER_NUM || !g_timer_run_ctrl[id].is_init) return FSP_ERR_INVALID_ARGUMENT;
    return g_timer_hw_map[id].p_hal_instance->p_api->stop(g_timer_hw_map[id].p_hal_instance->p_ctrl);
}

fsp_err_t BSP_Timer_Reset(bsp_timer_id_e id)
{
    if(id >= BSP_TIMER_NUM || !g_timer_run_ctrl[id].is_init) return FSP_ERR_INVALID_ARGUMENT;
    return g_timer_hw_map[id].p_hal_instance->p_api->reset(g_timer_hw_map[id].p_hal_instance->p_ctrl);
}

fsp_err_t BSP_Timer_GetCounter(bsp_timer_id_e id, uint32_t *p_count)
{
    if(id >= BSP_TIMER_NUM || !g_timer_run_ctrl[id].is_init) return FSP_ERR_INVALID_ARGUMENT;
    
    timer_status_t status;
    fsp_err_t err = g_timer_hw_map[id].p_hal_instance->p_api->statusGet(g_timer_hw_map[id].p_hal_instance->p_ctrl, &status);
    
    if (FSP_SUCCESS == err)
    {
        *p_count = status.counter;
    }
    return err;
}

fsp_err_t BSP_Timer_SetPeriod(bsp_timer_id_e id, uint32_t period_counts)
{
    if(id >= BSP_TIMER_NUM || !g_timer_run_ctrl[id].is_init) return FSP_ERR_INVALID_ARGUMENT;
    return g_timer_hw_map[id].p_hal_instance->p_api->periodSet(g_timer_hw_map[id].p_hal_instance->p_ctrl, period_counts);
}

fsp_err_t BSP_Timer_SetFreq_Hz(bsp_timer_id_e id, uint32_t freq_hz)
{
    if(id >= BSP_TIMER_NUM || !g_timer_run_ctrl[id].is_init) return FSP_ERR_INVALID_ARGUMENT;
    if(freq_hz == 0) return FSP_ERR_INVALID_ARGUMENT;

    timer_instance_t const * p_inst = g_timer_hw_map[id].p_hal_instance;
    timer_info_t info;

    /* 1. 获取当前定时器的时钟源频率 */
    fsp_err_t err = p_inst->p_api->infoGet(p_inst->p_ctrl, &info);
    if(FSP_SUCCESS != err) return err;
    
    /* 2. 计算 Ticks = 时钟频率 / 目标频率 */
    /* 增加保护：如果 freq_hz 大于时钟频率，period 会变成 0 */
    if(freq_hz > info.clock_frequency) return FSP_ERR_INVALID_ARGUMENT;

    uint32_t period_counts = info.clock_frequency / freq_hz;

    /* 3. 设置周期 */
    return p_inst->p_api->periodSet(p_inst->p_ctrl, period_counts);
}

fsp_err_t BSP_Timer_SetDuty(bsp_timer_id_e id, uint32_t duty_permille, uint32_t channel_mask)
{
    if(id >= BSP_TIMER_NUM || !g_timer_run_ctrl[id].is_init) return FSP_ERR_INVALID_ARGUMENT;
    
    /* 限制占空比范围 0-100% */
    if(duty_permille > 1000) duty_permille = 1000; 

    timer_instance_t const * p_inst = g_timer_hw_map[id].p_hal_instance;
    timer_info_t info;
    uint32_t duty_counts;
    
    /* 1. 获取当前周期值 */
    fsp_err_t err = p_inst->p_api->infoGet(p_inst->p_ctrl, &info);
    if (FSP_SUCCESS != err) return err;

    /* 2. 计算占空比计数值
     * 使用 uint64_t 避免中间乘法溢出
     */
    duty_counts = (uint32_t)((uint64_t)info.period_counts * duty_permille / 1000);

    /* 3. 设置占空比 */
    return p_inst->p_api->dutyCycleSet(p_inst->p_ctrl, duty_counts, channel_mask);
}

void gpt_common_isr(timer_callback_args_t * p_args)
{
    /* [优化] 直接转换 Context 指针，无需遍历 */
    bsp_timer_runtime_ctrl_t *p_ctrl = (bsp_timer_runtime_ctrl_t *)p_args->p_context;

    /* 安全检查 */
   if (NULL == p_ctrl) {
        p_ctrl = &g_timer_run_ctrl[BSP_TIMER_LVGL];
    }

    /* 调用用户回调 */
    if(p_ctrl->p_user_cb != NULL) 
    {
        p_ctrl->p_user_cb(p_args);
    }
}