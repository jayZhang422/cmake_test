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
                   
                     
                    
                    while (1)
                    {
                     
                       tx_thread_sleep(100) ;
                    }
                }

        }
