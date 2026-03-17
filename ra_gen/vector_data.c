/* generated vector source file - do not edit */
        #include "bsp_api.h"
        /* Do not build these data structures if no interrupts are currently allocated because IAR will have build errors. */
        #if VECTOR_DATA_IRQ_COUNT > 0
        BSP_DONT_REMOVE const fsp_vector_t g_vector_table[BSP_ICU_VECTOR_NUM_ENTRIES] BSP_PLACE_IN_SECTION(BSP_SECTION_APPLICATION_VECTORS) =
        {
                        [0] = sci_uart_rxi_isr, /* SCI7 RXI (Receive data full) */
            [1] = sci_uart_txi_isr, /* SCI7 TXI (Transmit data empty) */
            [2] = sci_uart_tei_isr, /* SCI7 TEI (Transmit end) */
            [3] = sci_uart_eri_isr, /* SCI7 ERI (Receive error) */
            [4] = sci_i2c_txi_isr, /* SCI4 TXI (Transmit data empty) */
            [5] = sci_i2c_tei_isr, /* SCI4 TEI (Transmit end) */
            [6] = r_icu_isr, /* ICU IRQ6 (External pin interrupt 6) */
            [7] = iic_master_rxi_isr, /* IIC2 RXI (Receive data full) */
            [8] = iic_master_txi_isr, /* IIC2 TXI (Transmit data empty) */
            [9] = iic_master_tei_isr, /* IIC2 TEI (Transmit end) */
            [10] = iic_master_eri_isr, /* IIC2 ERI (Transfer error) */
            [11] = rtc_alarm_periodic_isr, /* RTC ALARM (Alarm interrupt) */
            [12] = rtc_alarm_periodic_isr, /* RTC PERIOD (Periodic interrupt) */
            [13] = rtc_carry_isr, /* RTC CARRY (Carry interrupt) */
            [14] = dmac_int_isr, /* DMAC1 INT (DMAC1 transfer end) */
            [15] = dmac_int_isr, /* DMAC2 INT (DMAC2 transfer end) */
            [16] = spi_rxi_isr, /* SPI1 RXI (Receive buffer full) */
            [17] = spi_txi_isr, /* SPI1 TXI (Transmit buffer empty) */
            [18] = spi_tei_isr, /* SPI1 TEI (Transmission complete event) */
            [19] = spi_eri_isr, /* SPI1 ERI (Error) */
            [20] = sci_uart_rxi_isr, /* SCI9 RXI (Receive data full) */
            [21] = sci_uart_txi_isr, /* SCI9 TXI (Transmit data empty) */
            [22] = sci_uart_tei_isr, /* SCI9 TEI (Transmit end) */
            [23] = sci_uart_eri_isr, /* SCI9 ERI (Receive error) */
        };
        #if BSP_FEATURE_ICU_HAS_IELSR
        const bsp_interrupt_event_t g_interrupt_event_link_select[BSP_ICU_VECTOR_NUM_ENTRIES] =
        {
            [0] = BSP_PRV_VECT_ENUM(EVENT_SCI7_RXI,GROUP0), /* SCI7 RXI (Receive data full) */
            [1] = BSP_PRV_VECT_ENUM(EVENT_SCI7_TXI,GROUP1), /* SCI7 TXI (Transmit data empty) */
            [2] = BSP_PRV_VECT_ENUM(EVENT_SCI7_TEI,GROUP2), /* SCI7 TEI (Transmit end) */
            [3] = BSP_PRV_VECT_ENUM(EVENT_SCI7_ERI,GROUP3), /* SCI7 ERI (Receive error) */
            [4] = BSP_PRV_VECT_ENUM(EVENT_SCI4_TXI,GROUP4), /* SCI4 TXI (Transmit data empty) */
            [5] = BSP_PRV_VECT_ENUM(EVENT_SCI4_TEI,GROUP5), /* SCI4 TEI (Transmit end) */
            [6] = BSP_PRV_VECT_ENUM(EVENT_ICU_IRQ6,GROUP6), /* ICU IRQ6 (External pin interrupt 6) */
            [7] = BSP_PRV_VECT_ENUM(EVENT_IIC2_RXI,GROUP7), /* IIC2 RXI (Receive data full) */
            [8] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TXI,GROUP0), /* IIC2 TXI (Transmit data empty) */
            [9] = BSP_PRV_VECT_ENUM(EVENT_IIC2_TEI,GROUP1), /* IIC2 TEI (Transmit end) */
            [10] = BSP_PRV_VECT_ENUM(EVENT_IIC2_ERI,GROUP2), /* IIC2 ERI (Transfer error) */
            [11] = BSP_PRV_VECT_ENUM(EVENT_RTC_ALARM,GROUP3), /* RTC ALARM (Alarm interrupt) */
            [12] = BSP_PRV_VECT_ENUM(EVENT_RTC_PERIOD,GROUP4), /* RTC PERIOD (Periodic interrupt) */
            [13] = BSP_PRV_VECT_ENUM(EVENT_RTC_CARRY,GROUP5), /* RTC CARRY (Carry interrupt) */
            [14] = BSP_PRV_VECT_ENUM(EVENT_DMAC1_INT,GROUP6), /* DMAC1 INT (DMAC1 transfer end) */
            [15] = BSP_PRV_VECT_ENUM(EVENT_DMAC2_INT,GROUP7), /* DMAC2 INT (DMAC2 transfer end) */
            [16] = BSP_PRV_VECT_ENUM(EVENT_SPI1_RXI,GROUP0), /* SPI1 RXI (Receive buffer full) */
            [17] = BSP_PRV_VECT_ENUM(EVENT_SPI1_TXI,GROUP1), /* SPI1 TXI (Transmit buffer empty) */
            [18] = BSP_PRV_VECT_ENUM(EVENT_SPI1_TEI,GROUP2), /* SPI1 TEI (Transmission complete event) */
            [19] = BSP_PRV_VECT_ENUM(EVENT_SPI1_ERI,GROUP3), /* SPI1 ERI (Error) */
            [20] = BSP_PRV_VECT_ENUM(EVENT_SCI9_RXI,GROUP4), /* SCI9 RXI (Receive data full) */
            [21] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TXI,GROUP5), /* SCI9 TXI (Transmit data empty) */
            [22] = BSP_PRV_VECT_ENUM(EVENT_SCI9_TEI,GROUP6), /* SCI9 TEI (Transmit end) */
            [23] = BSP_PRV_VECT_ENUM(EVENT_SCI9_ERI,GROUP7), /* SCI9 ERI (Receive error) */
        };
        #endif
        #endif