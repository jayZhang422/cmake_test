#include "Screen_Thread.h"
#include "bsp_tjc_hmi.hpp"
#include "uart_screen.hpp"
#include <cmath>

extern "C" {

void Screen_Thread_entry(void)
{
    static uint8_t nyquist_data[256];
    static uint8_t bode_data[256];
    static bool wave_ready = false;
    static ULONG last_wave_tick = 0U;
    static ULONG last_page1_tick = 0U;

    App_UartScreen_Init();

    while (1)
    {
        ULONG now = tx_time_get();

        if (!wave_ready)
        {
            for (uint16_t i = 0U; i < 256U; ++i)
            {
                float s0 = std::sin((float)(i + 1U) * 3.1415926f / 32.0f);
                float s1 = std::sin((float)(i + 1U) * 3.1415926f / 24.0f + 1.2f);
                int v0 = (int)((s0 + 1.0f) * 110.0f);
                int v1 = (int)((s1 + 1.0f) * 110.0f);
                if (v0 < 0) v0 = 0;
                if (v0 > 255) v0 = 255;
                if (v1 < 0) v1 = 0;
                if (v1 > 255) v1 = 255;
                nyquist_data[i] = (uint8_t)v0;
                bode_data[i] = (uint8_t)v1;
            }
            wave_ready = true;
        }

        if ((now - last_wave_tick) >= 500U)
        {
            App_DrawEISWaveforms(nyquist_data, bode_data, 256U);
            App_UpdateHealthResult(38, 27, 0);
            last_wave_tick = now;
        }

        if ((now - last_page1_tick) >= 1000U)
        {
            App_UartScreen_RequestPage1Refresh();
            App_UartScreen_DumpPage1();
            last_page1_tick = now;
        }

        App_UartScreen_Task();
        tx_thread_sleep(2);
    }
}

}
