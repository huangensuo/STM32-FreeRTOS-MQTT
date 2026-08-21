/*
 * 报警指示灯驱动
 * 引脚：PA0（推挽输出）
 * 功能：震动/火焰触发时点亮，持续1秒后自动熄灭
 */
#ifndef __LED_H
#define __LED_H
#include "stm32f10x.h"

#define LED_PIN    GPIO_Pin_0  // 报警指示灯引脚
#define LED_PORT   GPIOA       // 报警指示灯端口

void LED_Init(void);  // 初始化报警指示灯
void LED_On(void);    // 点亮报警指示灯
void LED_Off(void);   // 熄灭报警指示灯

#endif
