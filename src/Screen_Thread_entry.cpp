#include "Screen_Thread.h"
#include "bsp_tjc_hmi.hpp"
extern "C"{
#include "bsp_usart.h"
                /* Screen_Thread entry function */
                void Screen_Thread_entry(void)
                {
                    /* TODO: add your own code here */

                 
                    while(1)
                    {
      

      
        tx_thread_sleep(5000);
                    }
                }
            }