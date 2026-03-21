#include "eis_screen.hpp"
#include "uart_screen.hpp"
#include <cstdint>

    
float testCruve[3] = {230.5,330.3,220.0};

    

void Battery_screen::init(void)
{
        TjcHmi::Init(COM_TJC) ;
        AppHmi::Init() ;
}

    void Battery_screen:: test(void){

        Get();
        clear();
        BSP_Printf(COM_DEBUG, "cells :%d",get_Cells) ;
        BSP_Printf(COM_DEBUG, "Cap :%d",get_Capacity) ;
        
        send(BODE, testCruve, 0);
        send(NYQUIST,testCruve,0);
       
        if(AppHmi::BrandDetecet::Tattu == get_brand )
        {
            AppPrint::PrintLog("now is Tattu") ;
        }
        else  if(AppHmi::BrandDetecet::BosLi == get_brand )
        {
            AppPrint::PrintLog("now is Bosli") ;
        }
        else  if(AppHmi::BrandDetecet::Infinity == get_brand )
        {
            AppPrint::PrintLog("now is Infinity") ;
        }

        Update_InternalResistance(30.2, AppHmi::MeasureState::HIGH);
        Update_TransferImpedance(20.3, AppHmi::MeasureState::LOW );
        Update_HealthStatus(AppHmi::HealthLevel::FAIR);
     
    }
void Battery_screen::clear(void)
{
    if(AppHmi::PAGE_2_NYQUIST != get_page && AppHmi::PAGE_3_BODE != get_page)
    {
        get_status = AppHmi::CruveStatus::CLEAR ;
    }


}
void Battery_screen::Get(void)
{
        get_brand = AppHmi::GetBrand() ;
        get_Capacity =AppHmi::GetCapacity() ;
        get_Cells =AppHmi::GetCells() ;
        get_status = AppHmi::GetCurveStatus();
        get_page = AppHmi::GetCurrentPage() ;
}