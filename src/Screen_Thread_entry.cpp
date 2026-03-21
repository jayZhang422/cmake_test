#include "Screen_Thread.h"
#include "bsp_tjc_hmi.hpp"
#include "tx_api.h"
#include "eis_screen.hpp"
#include <cstdint>
Battery_screen battery;
extern "C" {
static ULONG g_next_test_tick = 0U;
static const ULONG g_test_period_ticks = 1000U; /* 1s: TX_TIMER_TICKS_PER_SECOND = 1000 */
   
void Screen_Thread_entry(void)
{
     BSP_Serial_Init(COM_DEBUG) ;
    battery.init();
    g_next_test_tick = tx_time_get() + g_test_period_ticks;
    while (1)
    {
       TjcHmi::RxTaskLoop() ;
       ULONG now = tx_time_get();
       if ((LONG)(now - g_next_test_tick) >= 0)
       {
        g_next_test_tick += g_test_period_ticks;
        battery.test();
       }
       tx_thread_sleep(1);
    }
}

}
