/*
 * 延时工具驱动（FreeRTOS兼容版本）
 * 使用DWT cycle counter实现微秒级延时，不占用SysTick
 */
#ifndef __DELAY_H
#define __DELAY_H

#include "stm32f10x.h"

void DWT_Init(void);
void Delay_us(uint32_t us);
void Delay_ms(uint32_t ms);
void Delay_s(uint32_t s);

#endif
