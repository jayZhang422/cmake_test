#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H


#include "hal_data.h"

#define EEPROMDEV_INSTANCE (&g_i2c4)
#define EEPROMDEV_TIMEOUT 100
#define TOUCHDEV_INSTANCE (&g_i2c_touch)
#define TOUCHDEV_TIMEOUT 500
#define DEBUGDEV_INSTANCE (&g_usart_debug)
#define LCDDEV_INSTANCE (&g_spi0)
#define GPT_OVERFLOW_INSTANCE (&g_timer_overflow)
#define RTCDEV_INSTANCE (&g_rtc0)
#define KEYDEV_INSTANCE (&g_external_irq_key)
#define DMAC_ADC_INSTANCE (&g_dma_adc)
#define ADC0_INSTANCE (&g_adc5)
#define DACWAVE_INSTANCE (&g_dac_output)
#define DMAC_DAC_INSTANCE (&g_dma_dac)
#define TJC_INSTANCE (&g_uart_screen)
#endif // _BSP_CONFIG_H
