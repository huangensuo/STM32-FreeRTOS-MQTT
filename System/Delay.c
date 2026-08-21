/*
 * 延时工具驱动（FreeRTOS兼容版本）
 * 使用DWT(Data Watchpoint and Trace) cycle counter实现微秒级延时
 * 不占用SysTick，与FreeRTOS调度器完全兼容
 */
#include "Delay.h"
#include "stm32f10x.h"

/* DWT寄存器地址（旧版core_cm3.h未定义DWT结构体，手动映射） */
#define DWT_CTRL_ADDR       (*((volatile uint32_t*)0xE0001000))
#define DWT_CYCCNT_ADDR     (*((volatile uint32_t*)0xE0001004))
#define DEMCR_ADDR          (*((volatile uint32_t*)0xE000EDFC))
#define DEMCR_TRCENA_BIT    (1UL << 24)
#define DWT_CYCCNTENA_BIT   (1UL << 0)

void DWT_Init(void)
{
    DEMCR_ADDR |= DEMCR_TRCENA_BIT;
    DWT_CYCCNT_ADDR = 0;
    DWT_CTRL_ADDR |= DWT_CYCCNTENA_BIT;
}

void Delay_us(uint32_t xus)
{
    uint32_t startTick = DWT_CYCCNT_ADDR;
    uint32_t delayTicks = xus * 72;
    while((DWT_CYCCNT_ADDR - startTick) < delayTicks);
}

void Delay_ms(uint32_t xms)
{
    while(xms--)
    {
        Delay_us(1000);
    }
}

void Delay_s(uint32_t xs)
{
    while(xs--)
    {
        Delay_ms(1000);
    }
}
