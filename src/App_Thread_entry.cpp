#include "App_Thread.h"
#include "tx_api.h"

extern "C" {
#include "Phase_test.h"
#include "eis_uart_verify.hpp"
#include "bsp_usart.h"

                /* App_Thread entry function */
                void App_Thread_entry(void)
                {
                    // Run_eis_uart_verify_server();
                  
                    //  Run_phase2_test() ;
                        
                    while (1)
                    {
                       tx_thread_sleep(10) ;
                    }
                }

            }
