/* generated HAL header file - do not edit */
#ifndef HAL_DATA_H_
#define HAL_DATA_H_
#include <stdint.h>
#include "bsp_api.h"
#include "common_data.h"
#include "r_dtc.h"
#include "r_transfer_api.h"
#include "r_sci_uart.h"
            #include "r_uart_api.h"
#include "r_spi.h"
#include "r_dmac.h"
#include "r_transfer_api.h"
#include "r_dac.h"
#include "r_dac_api.h"
#include "r_adc.h"
#include "r_adc_api.h"
#include "r_rtc.h"
#include "r_rtc_api.h"
#include "r_gpt.h"
#include "r_timer_api.h"
#include "r_iic_master.h"
#include "r_i2c_master_api.h"
#include "r_qspi.h"
            #include "r_spi_flash_api.h"
#include "r_sci_i2c.h"
#include "r_i2c_master_api.h"
FSP_HEADER
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer4;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer4_ctrl;
extern const transfer_cfg_t g_transfer4_cfg;
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer3;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer3_ctrl;
extern const transfer_cfg_t g_transfer3_cfg;
/** UART on SCI Instance. */
            extern const uart_instance_t      g_uart_screen;

            /** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
            extern sci_uart_instance_ctrl_t     g_uart_screen_ctrl;
            extern const uart_cfg_t g_uart_screen_cfg;
            extern const sci_uart_extended_cfg_t g_uart_screen_cfg_extend;

            #ifndef usart_common_callback
            void usart_common_callback(uart_callback_args_t * p_args);
            #endif
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer2;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer2_ctrl;
extern const transfer_cfg_t g_transfer2_cfg;
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer1;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer1_ctrl;
extern const transfer_cfg_t g_transfer1_cfg;
/** SPI on SPI Instance. */
extern const spi_instance_t g_spi0;

/** Access the SPI instance using these structures when calling API functions directly (::p_api is not used). */
extern spi_instance_ctrl_t g_spi0_ctrl;
extern const spi_cfg_t g_spi0_cfg;

/** Callback used by SPI Instance. */
#ifndef spi_common_callback
void spi_common_callback(spi_callback_args_t * p_args);
#endif


#define RA_NOT_DEFINED (1)
#if (RA_NOT_DEFINED == g_transfer1)
    #define g_spi0_P_TRANSFER_TX (NULL)
#else
    #define g_spi0_P_TRANSFER_TX (&g_transfer1)
#endif
#if (RA_NOT_DEFINED == g_transfer2)
    #define g_spi0_P_TRANSFER_RX (NULL)
#else
    #define g_spi0_P_TRANSFER_RX (&g_transfer2)
#endif
#undef RA_NOT_DEFINED
/* Transfer on DMAC Instance. */
extern const transfer_instance_t g_dma_dac;

/** Access the DMAC instance using these structures when calling API functions directly (::p_api is not used). */
extern dmac_instance_ctrl_t g_dma_dac_ctrl;
extern const transfer_cfg_t g_dma_dac_cfg;

#ifndef dmac_common_isr
void dmac_common_isr(transfer_callback_args_t * p_args);
#endif
/** DAC on DAC Instance. */
extern const dac_instance_t g_dac_output;

/** Access the DAC instance using these structures when calling API functions directly (::p_api is not used). */
extern dac_instance_ctrl_t g_dac_output_ctrl;
extern const dac_cfg_t g_dac_output_cfg;
/* Transfer on DMAC Instance. */
extern const transfer_instance_t g_dma_adc;

/** Access the DMAC instance using these structures when calling API functions directly (::p_api is not used). */
extern dmac_instance_ctrl_t g_dma_adc_ctrl;
extern const transfer_cfg_t g_dma_adc_cfg;

#ifndef dmac_common_isr
void dmac_common_isr(transfer_callback_args_t * p_args);
#endif
/** ADC on ADC Instance. */
extern const adc_instance_t g_adc5;

/** Access the ADC instance using these structures when calling API functions directly (::p_api is not used). */
extern adc_instance_ctrl_t g_adc5_ctrl;
extern const adc_cfg_t g_adc5_cfg;
extern const adc_channel_cfg_t g_adc5_channel_cfg;

#ifndef NULL
void NULL(adc_callback_args_t * p_args);
#endif

#ifndef NULL
#define ADC_DMAC_CHANNELS_PER_BLOCK_NULL  1
#endif
/* RTC Instance. */
extern const rtc_instance_t g_rtc0;

/** Access the RTC instance using these structures when calling API functions directly (::p_api is not used). */
extern rtc_instance_ctrl_t g_rtc0_ctrl;
extern const rtc_cfg_t g_rtc0_cfg;

#ifndef rtc_common_isr
void rtc_common_isr(rtc_callback_args_t * p_args);
#endif
/** Timer on GPT Instance. */
extern const timer_instance_t g_timer_overflow;

/** Access the GPT instance using these structures when calling API functions directly (::p_api is not used). */
extern gpt_instance_ctrl_t g_timer_overflow_ctrl;
extern const timer_cfg_t g_timer_overflow_cfg;

#ifndef gpt_common_isr
void gpt_common_isr(timer_callback_args_t * p_args);
#endif
/* I2C Master on IIC Instance. */
extern const i2c_master_instance_t g_i2c_touch;

/** Access the I2C Master instance using these structures when calling API functions directly (::p_api is not used). */
extern iic_master_instance_ctrl_t g_i2c_touch_ctrl;
extern const i2c_master_cfg_t g_i2c_touch_cfg;

#ifndef bsp_i2c_common_isr
void bsp_i2c_common_isr(i2c_master_callback_args_t * p_args);
#endif
extern const spi_flash_instance_t g_qspi0;
            extern qspi_instance_ctrl_t g_qspi0_ctrl;
            extern const spi_flash_cfg_t g_qspi0_cfg;
extern const i2c_master_cfg_t g_i2c4_cfg;
/* I2C on SCI Instance. */
extern const i2c_master_instance_t g_i2c4;
#ifndef bsp_i2c_common_isr
void bsp_i2c_common_isr(i2c_master_callback_args_t * p_args);
#endif

extern const sci_i2c_extended_cfg_t g_i2c4_cfg_extend;
extern sci_i2c_instance_ctrl_t g_i2c4_ctrl;
/* Transfer on DTC Instance. */
extern const transfer_instance_t g_transfer0;

/** Access the DTC instance using these structures when calling API functions directly (::p_api is not used). */
extern dtc_instance_ctrl_t g_transfer0_ctrl;
extern const transfer_cfg_t g_transfer0_cfg;
/** UART on SCI Instance. */
            extern const uart_instance_t      g_usart_debug;

            /** Access the UART instance using these structures when calling API functions directly (::p_api is not used). */
            extern sci_uart_instance_ctrl_t     g_usart_debug_ctrl;
            extern const uart_cfg_t g_usart_debug_cfg;
            extern const sci_uart_extended_cfg_t g_usart_debug_cfg_extend;

            #ifndef usart_common_callback
            void usart_common_callback(uart_callback_args_t * p_args);
            #endif
void hal_entry(void);
void g_hal_init(void);
FSP_FOOTER
#endif /* HAL_DATA_H_ */
