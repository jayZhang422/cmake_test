#include "App2_Thread.h"
#include "eis_algorithm.hpp"
                /* App2_Thread entry function */
                void App2_Thread_entry(void)
                {
                    /* TODO: add your own code here */
                    // EIS_DataProcess_Thread_Entry();
                    while(1)
                    {
                        tx_thread_sleep(1);
                    }
                }
