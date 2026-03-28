#include "App_Thread.h"
#include "tx_api.h"
#include "bsp_usart.h"
#include "eis_algorithm.hpp"
#include "app_eis.hpp"
#include "eis_data_pipeline.hpp"
#include "bsp_timer.h"
extern "C" {

extern volatile bool g_is_signal_stable;


                /* App_Thread entry function */
                void App_Thread_entry(void) 
                {
                   
                     BSP_Serial_Init(COM_DEBUG) ;
                     // EIS::DataPipeline::Init();
                     
                     // UpdateDacWaveParameters(1.65f, 1.0f);
                     UpdateDacWaveParameters(0.1f, 0.05f);
                     // DAC_SingleSignal_131();
                     DAC_SingleSignal();
                  //    while (g_is_signal_stable)
                  // {
                  //    tx_thread_sleep(1);
                  // }

       
                  // BSP_Timer_Stop(BSP_TIMER_OVERFLOW);
                  // AppPrint::PrintLog(">>> [控制线程] 测量结束，硬件已安全关闭。");  
                    while (1)
                    {
                     
                       tx_thread_sleep(100) ;
                    }
                }

        }
