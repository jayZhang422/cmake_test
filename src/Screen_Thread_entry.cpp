#include "Screen_Thread.h"
#include "bsp_tjc_hmi.hpp"
extern "C"{
#include "bsp_usart.h"
                /* Screen_Thread entry function */
                void Screen_Thread_entry(void)
                {
                    /* TODO: add your own code here */

                    TjcHmi::Init(COM_TJC);
                    tx_thread_sleep(500);
  
                    TjcHmi::SendCmd("page page0");
                    tx_thread_sleep(10); // 切页后避让
                    while(1)
                    {
                        
                        TjcHmi::SetText("b0", "正在配置...");
                        TjcHmi::SendCmd("b0.bco=63488"); // 变红 (RGB565 红色)，警示正在操作

      
                        TjcHmi::SendCmd("tsw b1,0");
                        TjcHmi::SendCmd("tsw b2,0");

     
        tx_thread_sleep(2000); // 模拟耗时 2 秒的硬件配置过程

        /* ====================================================
         * 场景模拟：配置完成，释放 UI 锁
         * ==================================================== */

        // 1. 恢复 b0 的初始状态
        TjcHmi::SetText("b0", "电池类型设定");
        TjcHmi::SendCmd("b0.bco=4863"); // 恢复为原来的蓝色 (假设蓝色 RGB565 为 4863)

        // 2. 重新使能 b1 和 b2，允许用户点击跳转到 EIS 曲线页
        TjcHmi::SendCmd("tsw b1,1");
        TjcHmi::SendCmd("tsw b2,1");

        // 挂起线程，等待下一个事件周期
        tx_thread_sleep(5000);
                    }
                }
            }