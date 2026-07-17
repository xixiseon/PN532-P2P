
#include "timer.h"
#include "stm32f10x.h"

static __IO uint8_t  UsNumber = 0;
static __IO uint16_t MsNumber = 0;

extern uint32_t SystemCoreClock;

/**
 * @brief  initialize delay function
 * @param  none
 * @retval none
 */
void SysTickDelayInit()
{
    SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);
    UsNumber = SystemCoreClock / 8000000;
    MsNumber = (u16)UsNumber * 1000;
}

/**
 * @brief  inserts a delay time.
 * @param  nus: specifies the delay time length, in microsecond.
 * @retval none
 */
void SysTickDelayUs(uint32_t nus)
{
    u32 temp;
    SysTick->LOAD = nus * UsNumber;
    SysTick->VAL  = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    do {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0X00;
}

/**
 * @brief  inserts a delay time.
 * @param  nms: specifies the delay time length, in milliseconds.
 * @retval none
 */
void SysTickDelayMs(uint16_t nms)
{
    u32 temp;
    SysTick->LOAD = (u32)nms * MsNumber;
    SysTick->VAL  = 0x00;
    SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
    do {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && !(temp & (1 << 16)));
    SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;
    SysTick->VAL = 0X00;
}

/**
 * @brief  inserts a delay time.
 * @param  sec: specifies the delay time, in seconds.
 * @retval none
 */
void SysTickDelaySec(uint16_t sec)
{
    uint16_t index;
    for (index = 0; index < sec; index++) {
        SysTickDelayMs(500);
        SysTickDelayMs(500);
    }
}
