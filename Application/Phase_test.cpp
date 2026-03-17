#include "bsp_algorithm.hpp"
#include <sys/errno.h>
#include "bsp_name.hpp"
#include "bsp_adc.h"
#include "bsp_dac.h"
#include "bsp_dmac.h"
#include "bsp_timer.h"
#include "bsp_usart.h"
#include "bsp_user_elc.h"
#include "fsp_common_api.h"
#include "m-profile/armv8m_mpu.h"
#define WAVE_POINTS 200
#define R_REF_OHM 4700.0f
float sin_voltage[WAVE_POINTS];
float sin_current[WAVE_POINTS];
float stand_sin[WAVE_POINTS];
float stand_cos[WAVE_POINTS];
uint16_t g_dac_output_buffer[WAVE_POINTS];
uint16_t g_adc_raw_buffer [WAVE_POINTS];




ImpedanceResult g_phase1_result ;




extern "C" void Run_phase1_test(void) ;
extern "C" void Run_phase2_test(void) ;
extern "C" void Run_phase25_test(void);

void Run_phase1_test(void)
{
    float target_freq = 1000.0f;   // 1kHz �ź�
    float sample_rate = 100000.0f; // 100kHz ����??
    float battery_res = 0.5f;      // ���������� 0.5 ŷķ


    EisRefGenerator::Generate(sin_current, WAVE_POINTS,target_freq,sample_rate,BSP_ALG_SIN);
    for(int i=0; i<WAVE_POINTS; i++) {
        sin_voltage[i] = battery_res * sin_current[i]; 
        sin_voltage[i] += 0.002f;
    }
    EisRefGenerator::Generate(stand_sin, WAVE_POINTS, target_freq, sample_rate, BSP_ALG_SIN);
    EisRefGenerator::Generate(stand_cos, WAVE_POINTS, target_freq, sample_rate, BSP_ALG_COS);

    // 4. ���к����㷨 (The Core Algorithm)
    // ---------------------------------------------------------
    
    // ������??ʵ��(Vr) ??�鲿(Vi)
    float Vr = DigitalLockIn::Demodulate(sin_voltage, stand_sin, WAVE_POINTS);
    float Vi = DigitalLockIn::Demodulate(sin_voltage, stand_cos, WAVE_POINTS);

    // ��������??ʵ��(Ir) ??�鲿(Ii)
    float Ir = DigitalLockIn::Demodulate(sin_current, stand_sin, WAVE_POINTS);
    float Ii = DigitalLockIn::Demodulate(sin_current, stand_cos, WAVE_POINTS);

    // ����������??Z
   ImpedanceSolver::Calculate(Vr, Vi, Ir, Ii,&g_phase1_result);

    AppPrint::PrintFloat("R_real",g_phase1_result.R_real,"omega");
    

}

static void DAC_Waveform(void)
{
    // 1. ���� sin_voltage �������ɱ�׼����??(-1.0 ~ 1.0)
    EisRefGenerator::Generate(sin_voltage, WAVE_POINTS, 1000.0f, 100000.0f, BSP_ALG_SIN);

    const float offset = 2048.0f;
    const float amplitude = 1241.0f;

    for(int i=0; i<WAVE_POINTS; i++) {
        float val = sin_voltage[i];
        
        // �任: ̧�� + ����
        val = offset + (val * amplitude);

        // ǯλ
        if (val > 4095.0f) val = 4095.0f;
        if (val < 0.0f)    val = 0.0f;

        g_dac_output_buffer[i] = (uint16_t)val;
    }
    
    // AppPrint::PrintFloat("DEBUG: Buffer[500]", (float)g_dac_output_buffer[525], "code");
    // ������õ����飬׼�������������
    memset(sin_voltage, 0, sizeof(sin_voltage));
}

extern "C" void Run_phase2_test(void)
{
    fsp_err_t err;

    AppPrint::PrintLog("=== Phase 2 DAC Repeat Start ===");

    BSP_DAC_Init(BSP_DAC_WAVE);
    BSP_DMAC_Init(BSP_DMAC_DAC);
    BSP_Timer_Init(BSP_TIMER_OVERFLOW);
    BSP_ELC_Init(BSP_ELC);

    DAC_Waveform();

    err = BSP_DMAC_Reconfig(BSP_DMAC_DAC, (void const *) g_dac_output_buffer, BspAnalog::GetDacReg(0), 0);
    if (FSP_SUCCESS != err)
    {
        BSP_Printf(COM_DEBUG, "DAC DMAC Reconfig err=%d\r\n", (int) err);
        while (1)
        {
            tx_thread_sleep(100);
        }
    }

    err = BSP_DMAC_Enable(BSP_DMAC_DAC);
    if (FSP_SUCCESS != err)
    {
        BSP_Printf(COM_DEBUG, "DAC DMAC Enable err=%d\r\n", (int) err);
        while (1)
        {
            tx_thread_sleep(100);
        }
    }

    err = BSP_DAC_Start(BSP_DAC_WAVE);
    if (FSP_SUCCESS != err)
    {
        BSP_Printf(COM_DEBUG, "DAC Start err=%d\r\n", (int) err);
        while (1)
        {
            tx_thread_sleep(100);
        }
    }

    BSP_ELC_Enable(BSP_ELC);

    err = BSP_Timer_Start(BSP_TIMER_OVERFLOW);
    if (FSP_SUCCESS != err)
    {
        BSP_Printf(COM_DEBUG, "Timer Start err=%d\r\n", (int) err);
        while (1)
        {
            tx_thread_sleep(100);
        }
    }

    BSP_Printf(COM_DEBUG, "DAC repeat output running.\r\n");

    while (1)
    {
        tx_thread_sleep(1000);
    }
}

extern "C" void Run_phase25_test(void)
{ 

    AppPrint::PrintLog("=== Phase 2.5: Resistor Divider Test ===");

    // 1. ��ʼ??
    BSP_DAC_Init(BSP_DAC_WAVE);
    BSP_ADC_Init(BSP_ADC_0);
    BSP_DMAC_Init(BSP_DMAC_DAC);
    BSP_DMAC_Init(BSP_DMAC_ADC_0);
    BSP_Timer_Init(BSP_TIMER_OVERFLOW);
    BSP_ELC_Init(BSP_ELC);

    // 2. ׼������
    DAC_Waveform();

    // 3. ���� DMA
    BSP_DMAC_Reconfig(BSP_DMAC_DAC, (void*)g_dac_output_buffer ,BspAnalog::GetDacReg( 0), WAVE_POINTS);
    BSP_DMAC_Reconfig(BSP_DMAC_ADC_0, BspAnalog::GetAdcReg<0>(5), (void*)g_adc_raw_buffer, WAVE_POINTS);

    // 4. ����?
    BSP_DMAC_Enable(BSP_DMAC_DAC);    
    BSP_DMAC_Enable(BSP_DMAC_ADC_0);  
    BSP_DAC_Start(BSP_DAC_WAVE);      
    BSP_ADC_ScanStart(BSP_ADC_0);     
    BSP_ELC_Enable(BSP_ELC);
    
    // 5. ����?Timer
    BSP_Timer_Start(BSP_TIMER_OVERFLOW); 
    AppPrint::PrintLog("Timer Started. Waiting...");

    // 6. �ȴ� (2000 ticks)
    fsp_err_t err = BSP_DMAC_WaitComplete(BSP_DMAC_ADC_0, 2000);

    if (FSP_SUCCESS == err) {
        // �ɼ��ɹ�������ͣ??Timer ��ֹ���ݸ�д
        BSP_Timer_Stop(BSP_TIMER_OVERFLOW);
        
        // ��ӡ??50 �������ڹ۲첨���Ƿ��������ɺͲ�??
        for(int i = 0 ; i<50 ; i++)
        {
            BSP_Printf(COM_DEBUG, "%d\r\n", g_adc_raw_buffer[i]); 
        }
     
        // ======================================================
        // �����ǲ�����?Phase 2.5 ���Ľ��㲿��
        // ======================================================
        AppPrint::PrintLog("Done. Calculating...");

        // A. ����ֱ��ƫ�� (DC Bias)
        long sum = 0;
        for(int i = 0; i < WAVE_POINTS; i++) {
            sum += g_adc_raw_buffer[i];
        }
        float dc_bias = (float)sum / WAVE_POINTS;
        AppPrint::PrintFloat("1. ADC DC Bias (Target: ~1024)", dc_bias, "LSB");

        // B. ȥֱ??(DC Removal) -> �õ��������źţ����� sin_voltage
        for(int i = 0; i < WAVE_POINTS; i++) {
            sin_voltage[i] = (float)g_adc_raw_buffer[i] - dc_bias;
        }

        // C. ���ɱ�׼�ο��� (���ѳ��ӣ����Һ�����)
        EisRefGenerator::Generate(stand_sin, WAVE_POINTS, 1000.0f, 100000.0f, BSP_ALG_SIN);
        EisRefGenerator::Generate(stand_cos, WAVE_POINTS, 1000.0f, 100000.0f, BSP_ALG_COS);

        // D. ��������?(Demodulate) -> ��ȡ��ѹ??(V_mid) ��ʵ�����鲿
        float V_mid_r = DigitalLockIn::Demodulate(sin_voltage, stand_sin, WAVE_POINTS);
        float V_mid_i = DigitalLockIn::Demodulate(sin_voltage, stand_cos, WAVE_POINTS);

        // E. �趨Դͷ��ѹ (V_source)
        // ����û���ö�����?ADC ͨ��ȥ�� DAC �������ֱ�Ӽٶ����������������??DAC_Waveform ���趨�Ľ������� (1241)
        float V_src_r = 1241.0f; 
        float V_src_i = 0.0f;

        // F. ʸ����������?-> I = (V_source - V_mid) / R_ref
        float I_r = (V_src_r - V_mid_r) / R_REF_OHM;
        float I_i = (V_src_i - V_mid_i) / R_REF_OHM;

        // G. �������迹 -> Z_dut = V_mid / I
        ImpedanceResult res;
        ImpedanceSolver::Calculate(V_mid_r, V_mid_i, I_r, I_i, &res);

        // H. ��ӡ���ս�����??
        AppPrint::PrintFloat("2. Measured R (Target: ~4700)", res.R_real, "Ohm");
        
        // �жϲ����Ƿ�ͨ��
        if (res.R_real > 4000.0f && res.R_real < 5500.0f) {
            AppPrint::PrintLog(">>> SUCCESS: Resistor Measured Correctly! <<<");
        } else {
            AppPrint::PrintLog(">>> FAIL: Value off. Check Wiring/Resistor Value. <<<");
        }

    } else {
        // �ɼ���ʱ����
        BSP_Timer_Stop(BSP_TIMER_OVERFLOW);
        AppPrint::PrintLog("FAILURE: Timeout occurred!");
    }
}



