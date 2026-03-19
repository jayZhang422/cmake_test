#pragma once 
#include "stdint.h"

void App_UartScreen_Init(void);
void App_UartScreen_Task(void);
void App_UartScreen_RequestPage1Refresh(void);
void App_UartScreen_DumpPage1(void);

void App_UpdateHealthResult(float r_ohm, float r_ct, int health_score) ;

void App_DrawEISWaveforms(uint8_t* nyquist_data, uint8_t* bode_data, uint16_t points) ;
