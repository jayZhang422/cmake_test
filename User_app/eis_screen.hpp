#pragma once 
#include "bsp_tjc_hmi.hpp"
#include "bsp_usart.h"
#include "tx_api.h"
#include "uart_screen.hpp"
#include "bsp_name.hpp"
#include <cstdint>

#define BODE 112 //随机数
#define NYQUIST 221//随机数

class Battery_screen
{
    private:
    AppHmi::BrandDetecet get_brand ; //获取品牌
    int32_t get_Capacity ;  //获取容量
    int32_t get_Cells ;     //获取节数
    AppHmi::CruveStatus get_status; //获取曲线绘制状态
    uint8_t get_page;   //获取当前页面

    public:
    void test(void);
    void init(void);//初始化
    void clear(void);//切换自动清除
    void Get(void);//获取信息
    template <size_t N>
    void send(uint8_t sort, const float (&data)[N], uint8_t channel)
    {
        switch (sort) {
            case BODE: {
                if(AppHmi::PAGE_3_BODE != get_page) return;
                
                
                if(AppHmi::CruveStatus::DYNAMIC == get_status)
                {
                AppHmi::Fast_Send_Float_Array(1,channel,data) ;
                }
                else if(AppHmi::CruveStatus::STILL == get_status)
                {
                tx_thread_sleep(5) ;
                }

                else if(AppHmi::CruveStatus::CLEAR == get_status)
                {
                AppHmi::Clear_Curve(1,channel);
                }
                break;
            case NYQUIST: {
                if(AppHmi::PAGE_2_NYQUIST != get_page) return;

                if(AppHmi::CruveStatus::DYNAMIC == get_status)
                {
                AppHmi::Fast_Send_Float_Array(1,channel,data) ;
                }
                else if(AppHmi::CruveStatus::STILL == get_status)
                {
                tx_thread_sleep(5) ;
                }

                else if(AppHmi::CruveStatus::CLEAR == get_status)
                {
                AppHmi::Clear_Curve(1,channel);
                }
                break;
            }
        }
    }
}

};