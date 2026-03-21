#include "eeprom_app.h"
#include "at24c02.hpp"
#include "bsp_usart.h"
#include <cstdint>
#include <cstdio>

/* 实例化对象 (静态，隐藏在当前文件) */
static AT24C02 eeprom;





void EEPROMAppTest(void) {
    BSP_Printf(COM_DEBUG, "[App] EEPROM Mini Test Start...\r\n");

    /* 1. 初始化 */
    if (!eeprom.init()) {
        BSP_Printf(COM_DEBUG, "[App] Init Failed!\r\n");
        return;
    }

    /* 2. 准备测试数据 (仅4字节) */
    uint8_t addr = 0x00;
    uint8_t tx_buf[4] = {0x11, 0x22, 0x33, 0x44};
    uint8_t rx_buf[4] = {0, 0, 0, 0};

    /* 3. 写入数据 */
    BSP_Printf(COM_DEBUG, " -> Writing: %02X %02X %02X %02X\r\n", 
               tx_buf[0], tx_buf[1], tx_buf[2], tx_buf[3]);
               
    if (eeprom.write(addr, tx_buf, 4)) {
        BSP_Printf(COM_DEBUG, " -> Write Done.\r\n");
    } else {
        BSP_Printf(COM_DEBUG, " -> Write Error!\r\n");
        return;
    }

    /* 4. 读取数据 */
    // 驱动层已有延时，这里直接读
    if (eeprom.read(addr, rx_buf, 4)) {
        BSP_Printf(COM_DEBUG, " -> Read:    %02X %02X %02X %02X\r\n", 
                   rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3]);
    } else {
        BSP_Printf(COM_DEBUG, " -> Read Error!\r\n");
        return;
    }

    /* 5. 校验结果 */
    if (tx_buf[0] == rx_buf[0] && tx_buf[1] == rx_buf[1] &&
        tx_buf[2] == rx_buf[2] && tx_buf[3] == rx_buf[3]) 
    {
        BSP_Printf(COM_DEBUG, "[Result] SUCCESS!\r\n");
    } else {
        BSP_Printf(COM_DEBUG, "[Result] FAIL! Data mismatch.\r\n");
    }
}